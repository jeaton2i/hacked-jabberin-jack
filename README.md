# Jabberin' Jack Pumpkin Hack

The goal of this project is to take a "Jabberin' Jack" digital pumpkin from
[Mindscope Products](https://mindscopeproducts.com/products/jabberin-jack) and
modify it to be driven from an RP2040 clone board with custom faces,
animations, and similar.

Based on the work from pburgess:
[Hackable Halloween Prop Yields Short-Throw Projector](https://adafruit-playground.com/u/pburgess/pages/hackable-halloween-prop-yields-short-throw-projector)

## Parts List

- [Jabberin' Jack](https://www.amazon.com/dp/B0CD2Z9DGP)
- [Jabberin' Jack XL](https://www.amazon.com/dp/B0H2K4M1ZR) (note: I haven't gotten this version working yet)
- [Jabberin' Jack XL (white)](https://www.amazon.com/dp/B0H2K7R6RT) note: I haven't gotten this version working yet)
- [Generic Raspberry Pi Pico clone](https://www.amazon.com/dp/B0CG9FWDDC)
- [Adaptor for projector ribbon cable](https://www.amazon.com/dp/B09VPHWM26)
- Optional [MAX98357 for audio](https://www.amazon.com/dp/B0B4GK5R1R) (working - see `docs/audio-i2s-wiring.md`)
- Optional generic esp32 to make it networked (works, but needs its own power supply, the pumpkin's usb port doesn't provide enough for the projector light and the esp32 with wifi)

## References

- [pburgess DOOM port (rp2040-doom-LCD)](https://github.com/PaintYourDragon/rp2040-doom-LCD)
- [Arduino GFX library](https://github.com/moononournation/Arduino_GFX)

## Initial Goals

- Simple test pattern mode
- Basic triangle-face pumpkin static face
  - Flickering fire-like illumination
  - 3D style — done: every carved opening (eyes, nose, mouth) shows a
    visible wall of pumpkin flesh (a fixed-width darker red-orange band)
    around its cut edge, with a hot yellow-white glow through the
    interior, rather than filled flat (see `kWallWidthPx`'s comment in
    `firmware/src/faces/triangle_face.cpp`)
- Dynamic animated pumpkin face
  - Moving mouth for future lip sync — started: `TriangleFaceAnimated`
    flaps the mouth open/closed in a procedural, irregular pattern (see
    `pickNewJawTarget()`/`updateJaw()` in
    `firmware/src/faces/triangle_face.cpp`), not yet driven by any real
    audio - `TriangleFace` (the plain, non-animated face) is unchanged and
    still registered separately
  - Moving eyes/nose
- Special effects/overlays (not yet done)
  - Blood dripping
  - Fading away
  - Color changing illumination
- Alternative face styles
  - Jack Skellington
  - Skull Face
  - Robot
- Alternative Displays
  - Text
  - Eyeball
  - Other fun things

## Stretch Goals

- Audio support
  - Speaking voice (pre-rendered clips) — basic playback started, see
    `docs/audio-i2s-wiring.md`: a MAX98357A I2S amp on each board, with a
    test tone, one embedded test speech clip, and a Pac-Man power-pellet
    clip so far. The RP2040 plays them on its own speaker (`audio
    tone`/`audio voice`/`audio pacman`, with a runtime `volume` control);
    the optional ESP32 bridge can play the tone/voice pair on its own
    separate speaker too, either from its web UI or triggered remotely by
    the RP2040 (`audio esp-tone`/`audio esp-voice`, polled over the
    existing bridge link). Still just test tone + one test word + one
    game-audio clip - actual pre-rendered speech content is the remaining
    piece.
  - Speaking voice (text to speech)
  - Lip sync to external audio / microphone — not started against real
    speech yet, but `PacManFace` now proves out the underlying technique:
    it syncs its power-pellet visual to the exact moment in its own audio
    clip via a non-blocking playback path (`I2sPlayer::startClipAsync()`/
    `pump()`) that keeps animating while the clip plays, deriving its
    animation progress from actual playback position rather than a
    separate timer. `TriangleFaceAnimated`'s jaw flap is the natural next
    thing to wire up this way once there's real speech content to sync to.
- Sensor support
  - Basic motion sensor to trigger effects
  - Camera-based sensor for human detection (look at person, trigger speech)
- esp32 add-on board to make wifi controllable — done, see
  `docs/esp32-network-bridge.md` and `esp32-bridge/` (two interchangeable
  link options: a wired UART for any classic ESP32, or USB host mode for
  an ESP32-S2/S3/P4 plugged straight into the RP2040's own USB-C port).
  Answering the interface question below: plain serial either way, reusing
  the RP2040's existing text command protocol. That protocol already
  exposes every runtime setting live (not just "pre-stored modes"), so SPI
  wouldn't have bought any more dynamism, just more complexity on both
  ends.
- Countdown to a holiday — done: the `Countdown` face shows "`<N>` days
  until `<holiday>`" (configurable name/date, defaults to Halloween). The
  RP2040 has no battery-backed RTC, so it only knows what day it is once
  the ESP32 bridge feeds it a real date over NTP (`settime`, synced
  periodically - see `docs/esp32-network-bridge.md`); without the bridge
  attached, it just shows a "needs the ESP32 bridge" placeholder instead.


## Project Layout

- `firmware/` — PlatformIO project (Arduino framework + Arduino_GFX) that
  runs on the RP2040 clone board.
- `esp32-bridge/` — optional second PlatformIO project (ESP32) that bridges
  the RP2040's serial console to a phone-friendly web UI over Wi-Fi, over
  either a wired UART (`esp32dev` env) or USB host mode (`esp32s3_usbhost`
  env). See `docs/esp32-network-bridge.md`.
- `docs/rp2040-display-pinout.md` — working RP2040 display wiring map and
  hardware assumptions.
- `docs/esp32-network-bridge.md` — ESP32 bridge wiring, Wi-Fi setup, and
  its web UI/API.
- `docs/audio-i2s-wiring.md` — I2S audio (MAX98357A) wiring, the
  `audio`/`volume` commands, and `I2sPlayer`'s non-blocking playback path.
- `tools/convert_image_to_rgb565.py` — converts an image in `sample-images/`
  into a PROGMEM RGB565 header under `firmware/src/assets/` for use by
  `StaticImageFace`/`CandleLitImageFace`.
- `tools/convert_wav_to_pcm.py` — converts a WAV file in `sample-audio/`
  into a mono 16-bit PCM header under `firmware/src/assets/` for
  `I2sPlayer::playClip()`.
- `tools/face_preview.py` — renders a face off-device (no hardware needed)
  for a quick look before flashing.
- `Arduino_GFX/` and `rp2040-doom-LCD/` — reference material only, not part
  of the build. `firmware/` pulls Arduino_GFX from the PlatformIO registry
  instead of these local copies.

## Getting Started

1. Install [PlatformIO](https://platformio.org/install/cli) (VS Code
   extension or CLI).
2. `cd firmware && pio run` to build, `pio run -t upload` to flash.
3. The projector matches the reference `rp2040-doom-LCD` target: an ILI9225
   panel at 220x176 using an 8-bit parallel interface. See
   `docs/rp2040-display-pinout.md` for the GPIO mapping.
4. Connect over serial at 115200 baud (`pio device monitor`) to switch faces
   and configure the firmware — see "Runtime Controls" below.

## Runtime Controls

Three push buttons (wired active-low to GND), each single-click *and*
double-click, or the serial console, all drive the same controls:

| Button | Single-click | Double-click |
|---|---|---|
| GP16 | Next face | Previous face |
| GP17 | Pause/resume auto-rotate | Toggle random/in-order face advance |
| GP18 | Mute/unmute this board's own audio | *(unused)* |

A double-click is only disambiguated from two single-clicks on GP16/GP17
(the ones with something bound to both) - each waits up to 350ms after a
first click to see whether a second one follows before committing to the
single-click action, the same brief delay any double-click needs to be
told apart from a single one. GP18 has nothing bound to a double-click,
so it reacts immediately. Pressing anything other than next/previous
briefly overlays a status message on the display confirming what it just
switched to. On boot, the serial console prints the command list; send
`help` any time to see it again.

| Command       | Effect                                                    |
|---------------|-------------------------------------------------------------|
| *(enter)*     | Advance to the next enabled face                           |
| `list`        | List every face with its on/off state, and the rotate interval |
| `goto <n>`    | Jump directly to face `n` (index from `list`), regardless of its on/off state |
| `status`      | Print a machine-readable JSON snapshot of everything below (used by the ESP32 bridge, see below) |
| `<n>`         | Toggle face `n` on/off (index from `list`)                 |
| `text <l1>[\|l2\|l3\|l4]` | Set the `ConfigurableText` face's message (up to 4 lines, split on `\|`) and jump to it |
| `font [name]` | List available fonts, or switch `ConfigurableText` to one (see below) |
| `rotate <ms>` | Set the auto-rotate interval in milliseconds (`0` disables) |
| `brightness [percent]` | Show, or set, the candle brightness (`100` = the flicker's original intensity; default is `115`) |
| `volume [percent]` | Show, or set, this board's own audio playback volume (`100` = original clip/tone level, no boost above that) |
| `mute`        | Mute this board's own audio, or restore whatever volume was playing before muting (same button as the old "order" toggle - see "Runtime Controls" above) |
| `eyecolor [blue\|red\|green\|brown]` | Show, or set, the animated eye faces' iris color (default `blue`, the native art) - only the plain eye faces; the candle-lit ones always stay their usual monochrome orange/yellow regardless |
| `order [random\|in-order]` | Show, or set, whether auto-rotate/next-face advances in list order or picks a random enabled face |
| `esp32link [tx rx]` | Show, or set (as GP numbers), the pins the optional ESP32 bridge's UART is wired to - setting them saves the whole config and reboots to apply; see `docs/esp32-network-bridge.md` for which pins are actually valid |
| `audio <tone\|voice\|pacman>` | Play the synthesized test tone, the test speech clip, or the Pac-Man power-pellet clip, over this board's own optional I2S audio output - see `docs/audio-i2s-wiring.md` |
| `audio <esp-tone\|esp-voice>` | Queue the same tone/voice clips to instead play on the optional ESP32 bridge's own speaker (it polls for this - see `audiotrigger`) |
| `audiotrigger` | Read + clear the pending ESP32 audio queue - polled by the ESP32 bridge, not really meant for humans |
| `countdown [<month> <day> <name>]` | Show, or set, the `Countdown` face's target date/holiday name (defaults to Halloween, `10 31`) - `name` may contain one `\|` to split it across 2 lines, e.g. `countdown 6 14 Connie's\|Birthday` |
| `settime <year> <month> <day>` | Feed today's actual date to the `Countdown` face - the RP2040 has no clock of its own, so this is meant to be sent periodically by the ESP32 bridge once it has real time over NTP (see `docs/esp32-network-bridge.md`); nothing stops sending it by hand too |
| `setclock <hour> <minute> <second>` | Feed the current wall-clock time to the `Clock` face, the same way `settime` feeds the `Countdown` face its date |
| `clockformat [12\|24]` | Show, or set, whether the `Clock` face displays 12- or 24-hour time (default `24`) |
| `save`        | Persist the current face selection + rotate interval + brightness + volume + order + ESP32 link pins + countdown holiday to flash |
| `load`        | Reload the saved config from flash                          |
| `reset`       | Restore the compiled-in defaults (does not touch flash)     |
| `debug`       | Toggle diagnostic logging for the animated-eye faces' motion (off by default) |

Faces also auto-rotate on their own every `rotate` milliseconds (6s by
default) among whichever faces are currently enabled, in list order or
randomly per `order`.

An optional second board (an ESP32, see `esp32-bridge/` and
`docs/esp32-network-bridge.md`) can link to the RP2040 - over a second UART,
or a single USB-C cable in USB host mode on an ESP32-S2/S3/P4 - and relay
this same command set to a phone-friendly web UI over Wi-Fi.

`brightness` scales every flickering face together (`CandleFlicker` is
shared by `TriangleFace`, the flickering text faces, and all the
candle-lit image faces) - above 100% brightens (clamped so colors never
overflow/wrap), below 100% dims. It's saved to flash the same way the
rotate interval is.

`font` offers each letterform (`sans`/`mono`/`serif`) in two textures:
the plain name renders with `TextFace`'s usual nearest-neighbor zoom
(render at native size, then rescale to fit the visible circular area) -
that's what actually gives text its chunky, pixelated look, not the font
itself. The `-smooth` variant (e.g. `mono-smooth`) bilinearly resamples
that same rendering instead, for anti-aliased edges - same letterforms,
different texture. Smooth rendering is a one-time ~1s recompute when you
switch to it (no ongoing cost - draw() itself is equally cheap either
way), so expect a beat of delay before `font <name>-smooth` takes visible
effect. Neither the font nor the message chosen over serial is persisted
to flash, so both reset to the compiled-in defaults (`sans`, "Set my
text!") on reboot.

Saved config is stored in the RP2040's emulated EEPROM — a 4KB flash sector
the arduino-pico core reserves regardless of any filesystem — so it survives
power cycles without needing a LittleFS partition. `save`/`load` only take
effect explicitly; toggling faces or changing the rotate interval over serial
does not touch flash until you `save`. The saved face bitmask is positional
(bit *n* = face index *n* from `list`), so it should be re-saved after
reordering or adding/removing faces in `main.cpp`.

## Current Status

- The PlatformIO firmware structure, Arduino_GFX dependency, display wrapper,
  and face interface are in place, targeting the ILI9225 220x176 panel.
- 32 faces are registered (31 visible in the normal rotation, plus the
  hidden `StatusMessage` button-feedback overlay) and cycle via button,
  serial, or auto-rotation, grouped by theme: geometric faces (`Triangle`
  with a warm candle flicker, plus a `TriangleAnimated` variant whose
  mouth flaps open/closed in a procedural "talking" pattern), test
  patterns (color bars — currently noisy so disabled by default,
  checkerboard, bullseye), themed character faces (Jack Skellington,
  Skull, Robot with a look-around variant, an animated eyeball whose iris
  darts around inside the sclera and can be recolored live with
  `eyecolor`), text faces (`TextFace`, including a `ConfigurableText`
  instance whose message is set live over serial, a `Countdown` face
  showing "`<N>` days until `<holiday>`" once synced, and a `Clock` face
  rendered as seven-segment "LED" digits, both needing the ESP32 bridge
  for real date/time), and an "other" catch-all (Pac-Man - its
  power-pellet visual now synced to its own audio clip, an ESP32 logo, a
  Tropical scene, plus WPI logo variants) - all available plain or
  candle-lit via a shared flicker helper where it makes sense. A QR-code
  face was also built (encoding a link to this repo) but is compiled out
  behind a flag: this pumpkin's display is projected onto a curved
  surface, and a QR's fine data modules defocus into an unscannable blur
  there - kept in place in case a bigger/flatter pumpkin makes it worth
  retrying.
- Runtime config (which faces are enabled, the rotate interval, brightness,
  volume, face order, eye color, and the countdown holiday) can be changed
  and persisted to flash over the serial console, physical buttons, or the
  optional ESP32 web UI — see "Runtime Controls" above and
  `docs/esp32-network-bridge.md`.
- `TriangleFace` shows a visible wall of pumpkin flesh around every carved
  opening with a hot glowing interior, rather than filling it flat, for a
  3D-carved look (see "Initial Goals" above).
- Overlays, a moving nose, full pre-rendered speech content, and sensors
  remain future work.

## Firmware Previews

The current firmware renders these 220x176 previews:

![Test pattern preview](docs/images/test-pattern.png)

![Triangle face preview](docs/images/triangle-face.png)

### Windows on ARM64 note

`firmware/platformio.ini` points at the community
`maxgerhardt/platform-raspberrypi` platform so Arduino_GFX runs on
earlephilhower's arduino-pico core (PlatformIO's official `raspberrypi`
platform defaults to a Mbed-based core that Arduino_GFX doesn't fully
support). That community platform's toolchain packages don't ship
`windows_arm64` builds — only `windows_amd64`, which runs fine under
Windows 11's x64 emulation. If a fresh PlatformIO install on an ARM64
Windows machine fails with a `KeyError: 'windows_arm64'` or the build fails
looking up `tool-pioasm-rp2040-earlephilhower`/`toolchain-rp2040-earlephilhower`,
it's this gap — the fix is adding a `windows_arm64` entry (pointing at the
same amd64 URL) to the `earle_*` dicts in that platform's installed
`platform.py`, and adding `"windows_arm64"` to the `system` list in the
downloaded packages' `package.json` files under `~/.platformio/packages/`.

## Development Note

This project was developed with substantial assistance from Anthropic's
Claude. Third-party code and libraries remain under their respective licenses.
