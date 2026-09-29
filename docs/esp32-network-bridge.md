# ESP32 Network Bridge

An optional second board that gives the pumpkin a phone-friendly web UI and
puts it on the network, without changing anything about how the RP2040
itself runs faces. It's a thin bridge: it just relays the RP2040's existing
serial command protocol (see the top-level README's "Runtime Controls")
over Wi-Fi, and serves a control page for it. The RP2040 works exactly the
same with or without this board attached or powered.

Firmware lives in `esp32-bridge/` (its own PlatformIO project, separate
from `firmware/`).

## Why serial, not SPI

The original stretch goal noted serial "will be limited to changing
pre-stored modes" and asked whether SPI would allow something more dynamic.
In practice the RP2040's serial console is already the full live control
surface a human gets at the USB console - every setting (face on/off,
rotate interval, brightness, face order, the configurable text/font, save/
load/reset) is already settable that way, nothing is pre-stored-only. SPI
would add real complexity (a request/response framing layer, an interrupt
or polling scheme on both ends) for no additional capability, so the bridge
just reuses the existing text protocol - over either of the two links
below.

## Two ways to link to the RP2040

Both talk the exact same command protocol and serve the identical web UI;
pick whichever matches the ESP32 board actually in hand and build that
PlatformIO environment. `esp32-bridge/src/jack_link.h` is the shared
interface both implementations satisfy, so `main.cpp`, the web UI, and the
HTTP handlers are identical either way.

| | UART link | USB host link |
| --- | --- | --- |
| PlatformIO environment | `esp32dev` | `esp32s3_usbhost` |
| ESP32 requirement | any classic ESP32 (WROOM/WROVER) | ESP32-S2/S3/P4 - needs native USB-OTG |
| Wiring | 3 wires (TX, RX, GND) to two RP2040 GPIOs | one USB-C cable into the RP2040's existing USB port |
| Maturity | plain 2-wire UART, the well-trodden path | newer, more moving parts (USB host stack + a class driver) - see caveats below |

If you don't already have an ESP32 in hand and don't specifically need the
single-cable setup, the UART link is the safer first choice. The USB host
link is genuinely more convenient once working (no GPIO wiring at all,
matches the "USB-C to USB-C" idea directly), but leans on ESP-IDF's USB
Host Library and CDC-ACM class driver, which is less commonly used than a
plain UART and has had less real-hardware testing here.

### UART link (`esp32dev`)

The RP2040 firmware dedicates its UART1 to this link, active whether or not
anything is actually connected to it:

| Signal | RP2040 GPIO | Pico header pin | ESP32 pin |
| --- | ---: | ---: | --- |
| RP2040 TX -> ESP32 RX | GP20 | 26 | GPIO32 |
| RP2040 RX <- ESP32 TX | GP21 | 27 | GPIO33 |
| Common ground | GND | any GND pin | GND |

Baud rate is 115200 8N1 on both ends.

**Do not tie the two boards' 3V3 rails together.** Power the ESP32 from its
own USB/5V supply (or a separate 3.3V regulator) - only the two data lines
and ground are shared. An ESP32 can draw well over 300mA in bursts during
Wi-Fi transmit, more than the RP2040 clone board's onboard 3.3V regulator is
meant to supply on top of its own load. Both boards' GPIOs are native 3.3V
logic, so no level shifting is needed - just the shared ground.

#### Changing which pins are used

Both ends' pins are runtime-configurable (no reflash needed), but the two
sides work quite differently and **the RP2040 side is much more
constrained than it looks** - read this before picking different pins.

**RP2040 side:** its UART1 peripheral can only be mapped to specific GPIOs.
The SoC's pin-function mux offers UART1 in fixed groups of 4 consecutive
GPIOs - `{4,5,6,7}`, `{12,13,14,15}`, `{20,21,22,23}`, `{28,29,...}` - laid
out as (TX, RX, CTS, RTS) within each group, so only the *first two* pins
of a group are ever valid TX/RX choices. GP22, for instance, is UART1's
CTS pin, not a usable RX; GP27 belongs to UART0, not UART1, at all.
Requesting an invalid pin doesn't just fail quietly - it hard-faults the
whole chip before USB even finishes enumerating, which happened during
development of this feature and needed a BOOTSEL-mode reflash to recover
from. The firmware now validates against the legal set (`4/5`, `12/13`,
`20/21`, `28/29`) both when you set a pin and when it loads a saved
config, refusing anything else rather than repeating that mistake - but
picking a *valid-but-wrong* pair (e.g. one already in use) is still on you.
Given GP0-15 are taken by the display bus and GP16-18 by the buttons,
**GP20/GP21 (the defaults) are the only pair actually free on this board**;
GP28/29 would also be legal, but GP29 is conventionally reserved for VSYS
sensing on official Pico boards, so it's untested here.

Change it with the RP2040's own `esp32link` serial/USB command (see the
top-level README's "Runtime Controls" table): `esp32link` alone shows the
current pins, `esp32link <tx> <rx>` sets them. Because changing a live
UART's pins mid-session reliably hangs this core (the same hard-fault
category as above), this command doesn't apply the change immediately -
it saves the *entire* current config to flash and reboots the board on
the spot, the same way `save` followed by a power cycle would. Faces
in-progress or unsaved settings besides the pins get swept up in that
save too.

**ESP32 side:** more flexible - its GPIO matrix can route UART2 to nearly
any pin, so pick any two free, non-input-only GPIOs (see below). Change it
either from the web UI's "Bridge Link" section, or directly via
`GET`/`POST /api/link-pins` (`rxPin`/`txPin` as form fields on the POST).
Setting new pins here also can't apply live for the same reason as the
RP2040 side - it persists them (to NVS via `Preferences`) and restarts the
board immediately after replying, re-running `jackLinkBegin()` fresh.

Avoid GPIO34-39 for the ESP32's RX pin specifically: that whole range has
no pull-up/pull-down circuitry in silicon at all, so an idle or
momentarily-disconnected line there floats completely undefined instead of
settling high, which is exactly the kind of thing that lets it pick up
crosstalk from an adjacent wire and misread it as real data - this
actually happened during development and looked exactly like a live
device on the other end, which cost real time to run down. `uart_link.cpp`
rejects that range for RX outright, and also explicitly enables the
chosen RX pin's internal pull-up as a defensive measure on top of that.

### USB host link (`esp32s3_usbhost`)

One USB-C cable, straight from the ESP32-S2/S3/P4's own USB-OTG port into
the RP2040's existing USB-C/micro-USB port - the same port already used to
flash it and to run `pio device monitor` from a PC. No RP2040-side wiring
or firmware changes beyond what the UART link already needed (the RP2040
doesn't know or care whether the far end of its USB cable is a PC or this
ESP32 - it's the same CDC-ACM serial device either way).

Power comes from whichever end is acting as USB host - the ESP32 supplies
5V (VBUS) to the RP2040 over that same cable, the same way a PC would.
Don't also power the RP2040 from a second source at the same time (e.g. a
second USB cable to a PC) while it's plugged into the ESP32 - exactly as
you'd never plug a device into two USB hosts at once.

**Caveats before relying on this one:**

- Needs an ESP32-S2, ESP32-S3, or ESP32-P4 specifically. A plain classic
  ESP32 (WROOM/WROVER) cannot do this - its USB-C/micro-USB port is wired
  to a fixed-function CP2102/CH340 USB-*serial* bridge chip, not a general
  USB host controller, and cannot be switched into host mode.
- `esp32-bridge/src/usb_host_link.cpp` is written against Espressif's
  `usb_host_cdc_acm` component, vendored under
  `esp32-bridge/components/usb_host_cdc_acm/` (see the `VENDORED.md` there
  for why it's a vendored copy rather than a managed dependency, and for
  update instructions). It builds cleanly, but hasn't been exercised
  against real hardware - the UART link has more real-world mileage. If it
  misbehaves, the RP2040 firmware's own serial monitor logging (unaffected
  either way) is the first place to check whether the RP2040 side even
  sees anything.
- The RP2040 accepts either an idle/unplugged connection or a legitimate
  CDC host with no handshake beyond USB enumeration itself, so nothing
  extra needs to change on that side to support this link.

## Building and flashing

```
cd esp32-bridge
pio run -e esp32dev              # UART link build
pio run -e esp32s3_usbhost       # USB host link build
pio run -e <env> -t upload       # flash whichever one you built (put the
                                  # ESP32 in its own bootloader mode the
                                  # usual way for your board if needed)
pio device monitor -e <env>      # 115200 baud, to watch Wi-Fi/mDNS setup
                                  # logs (and, for the USB host build, USB
                                  # host/CDC enumeration logs)
```

## First-time Wi-Fi setup

The bridge has no hardcoded Wi-Fi credentials. On first boot (or any time it
can't reach a previously-saved network), it opens its own temporary access
point:

- **Network name:** `JabberinJack-Setup`
- **Password:** `pumpkin123`

Connect a phone or laptop to that network - a captive portal should open
automatically (or open `http://192.168.4.1/` manually); pick your home Wi-Fi
network and enter its password there. The ESP32 saves it, reboots onto your
network, and the setup AP disappears. If it can't join within 3 minutes it
reopens the setup AP automatically. To make it forget the saved network and
start over, use the portal's own "reset settings" style options exposed by
[WiFiManager](https://github.com/tzapu/WiFiManager) (e.g. reflashing after
erasing flash, or wiring a reset button per that library's docs - none is
wired up by default here).

## Finding it on the network

Once connected, the bridge advertises itself over mDNS/Bonjour as:

```
http://jabberinjack.local/
```

Most phones (iOS/Android with a Bonjour-aware browser), Macs, and Linux
desktops resolve `.local` names out of the box; some Windows setups need
[Bonjour Print Services](https://support.apple.com/en-us/106380) or iTunes
installed for `.local` resolution to work. The serial monitor also prints
the plain IP address at boot as a fallback (works everywhere, mDNS or not).

## Using the web UI

The page at `/` exposes most of what the serial console does: current
face + a "next face" button, auto-rotate on/off and interval, face order
(in-order/random), brightness, per-face on/off toggles, the configurable
text message and its font, the Countdown face's holiday name/date (see
"Countdown / date sync" below), audio (see below), and save/load/reset.
It polls the controller every few seconds so it stays in sync with
physical button presses too - and the whole page disables itself if that
polling ever fails, rather than leaving controls sitting there that would
just silently do nothing. The eye-look debug logging toggle isn't
exposed here (still available directly as the RP2040's own `debug`
serial command) - a diagnostic aid for tuning that motion, not something
this page's actual audience needs day to day.

Its "Bridge Link" section shows and edits *this board's own* link GPIOs
only - see "Changing which pins are used" above for what's valid there.
Applying them triggers this board's own reboot, which the page waits out
and reconnects to on its own. The RP2040's pins are shown read-only
instead of editable: changing them only works if this link is already
correctly configured, which defeats the point of fixing it remotely - set
those directly at the RP2040's own serial console (`esp32link` command)
instead. On the USB host build, the "This board's pins" half of that
section is replaced with a note that there's nothing to configure there.

Its "Audio" section plays test sounds on the RP2040's own I2S speaker
(`audio tone`/`audio voice`/`audio pacman` - see
`docs/audio-i2s-wiring.md`) and shows/sets its volume, with a Mute
button. This bridge's *own* separate speaker (if wired) isn't controlled
from this page - see `POST /api/local-audio` below if you want to script
that one directly - but the RP2040 can still trigger it remotely the same
as before (`audio esp-tone`/`audio esp-voice`); this bridge polls for
that on its own every 500ms regardless of whether the web page is open.

Small HTTP endpoints back the page (and beyond it, for scripting):

- `GET /api/status` - the RP2040's own JSON status snapshot, passed straight
  through unchanged.
- `POST /api/command` - body is one raw serial command (e.g. `rotate 4000`,
  `order random`, `text Hello|World`, a bare face index to toggle it, or an
  empty body to advance to the next face); returns
  `{"ok":true,"reply":"..."}` with whatever the RP2040 printed back.
- `GET /api/link-pins` - this bridge's own link GPIOs (distinct from
  anything in the RP2040's own status, which knows nothing about the ESP32
  side): `{"supported":true,"rxPin":32,"txPin":33}` (UART build) or
  `{"supported":false,"rxPin":-1,"txPin":-1}` (USB host build).
- `POST /api/link-pins` - form fields `rxPin`/`txPin`; persists them and
  restarts the bridge to apply. Only meaningful on the UART build.
- `POST /api/local-audio` - body `tone` or `voice`; plays immediately on
  this bridge's own separate speaker, no RP2040 involved. Not linked from
  any button on the page itself (see "Using the web UI" above) - purely
  for scripting/testing that speaker's wiring directly.

## Countdown / date sync

The RP2040's `Countdown` face shows "`<N>` days until `<holiday>`", but the
RP2040 itself has no battery-backed RTC - it has no idea what today's date
is on its own, and forgets whatever it was told the moment it loses power.
This bridge is what actually supplies that: once connected to Wi-Fi, it
starts an SNTP client (`configTime()`), and once that's resolved a real
time, sends it to the RP2040 as `settime <year> <month> <day>` - retried
every 10s until the first successful sync, then every 30 minutes after
that (frequent enough that the day count can't drift stale, infrequent
enough not to spam the link over what's otherwise unchanging information).
Without this bridge attached and connected, the Countdown face just shows
a "needs the ESP32 bridge" placeholder instead of a day count - the RP2040
firmware's `settime` command works over plain USB serial too, so it can
still be exercised/tested with nothing but a PC and no bridge at all.

**No timezone handling**: `configTime()` is called with a 0 UTC offset and
no DST, so the date handed to the RP2040 is whatever date it currently is
in UTC, not the pumpkin's actual local timezone. In practice this can only
matter for a few hours right around local midnight (the day count would be
off by one for that window) - a real timezone picker felt like unwarranted
complexity for what's just a decorative day-count, but it's a
straightforward addition later (an offset input in the web UI, applied to
`configTime()`'s `gmtOffset_sec` argument) if it ever matters enough.

The web UI's "Countdown" section shows the current holiday name/date and
whether a sync has actually landed yet, and can set a new holiday name and
target month/day (`countdown <month> <day> <name>` under the hood, same as
every other setting here - not persisted to the RP2040's flash until you
`save`).

## What's not persisted

Same rule as the serial console itself: toggling faces, brightness, rotate
interval, or face order over the web UI takes effect immediately but isn't
written to the RP2040's flash until you hit "Save to Flash" (or send the
`save` command) - see the top-level README's "Runtime Controls" section.
