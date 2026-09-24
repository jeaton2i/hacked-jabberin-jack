# Jabberin' Jack Pumpkin Hack

The goal of this project is to take a "Jabberin' Jack" digital pumpkin from
[Mindscope Products](https://mindscopeproducts.com/products/jabberin-jack) and
modify it to be driven from an RP2040 clone board with custom faces,
animations, and similar.

Based on the work from pburgess:
[Hackable Halloween Prop Yields Short-Throw Projector](https://adafruit-playground.com/u/pburgess/pages/hackable-halloween-prop-yields-short-throw-projector)

## Parts List

- [Jabberin' Jack XL](https://www.amazon.com/dp/B0H2K4M1ZR)
- [Jabberin' Jack XL (white)](https://www.amazon.com/dp/B0H2K7R6RT)
- [Generic Raspberry Pi Pico clone](https://www.amazon.com/dp/B0CG9FWDDC)
- [Adaptor for projector ribbon cable](https://www.amazon.com/dp/B09VPHWM26)

## References

- [pburgess DOOM port (rp2040-doom-LCD)](https://github.com/PaintYourDragon/rp2040-doom-LCD)
- [Arduino GFX library](https://github.com/moononournation/Arduino_GFX)

## Initial Goals

- Simple test pattern mode
- Basic triangle-face pumpkin static face
  - Flickering fire-like illumination
  - 3D style
- Dynamic animated pumpkin face
  - Moving mouth for future lip sync
  - Moving eyes/nose
- Special effects/overlays
  - Blood dripping
  - Fading away
  - Color changing illumination
- Alternative face styles
  - Jack Skellington
  - Skull Face / Reaper
  - Robot

## Stretch Goals

- Audio support
  - Speaking voice (pre-rendered clips)
  - Speaking voice (text to speech)
  - Lip sync to external audio / microphone
- Sensor support
  - Basic motion sensor to trigger effects
  - Camera-based sensor for human detection (look at person, trigger speech)
- esp32 add-on board to make wifi controllable
  - how should this interface?  serial, spi?
  - serial will be limited to changing pre-stored modes
  - can spi or something be used to be more dynamic?


## Project Layout

- `firmware/` — PlatformIO project (Arduino framework + Arduino_GFX) that
  runs on the RP2040 clone board.
- `docs/rp2040-display-pinout.md` — working RP2040 display wiring map and
  hardware assumptions.
- `tools/convert_image_to_rgb565.py` — converts an image in `sample-images/`
  into a PROGMEM RGB565 header under `firmware/src/assets/` for use by
  `StaticImageFace`/`CandleLitImageFace`.
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

Faces can be advanced with a push button (GP13, wired active-low to GND) or
over the serial console. On boot, the serial console prints the command list;
send `help` any time to see it again.

| Command       | Effect                                                    |
|---------------|-------------------------------------------------------------|
| *(enter)*     | Advance to the next enabled face                           |
| `list`        | List every face with its on/off state, and the rotate interval |
| `<n>`         | Toggle face `n` on/off (index from `list`)                 |
| `text <l1>[\|l2\|l3\|l4]` | Set the `ConfigurableText` face's message (up to 4 lines, split on `\|`) and jump to it |
| `font [name]` | List available fonts, or switch `ConfigurableText` to one (see below) |
| `rotate <ms>` | Set the auto-rotate interval in milliseconds (`0` disables) |
| `save`        | Persist the current face selection + rotate interval to flash |
| `load`        | Reload the saved config from flash                          |
| `reset`       | Restore the compiled-in defaults (does not touch flash)     |
| `debug`       | Toggle diagnostic logging for the animated-eye faces' motion (off by default) |

Faces also auto-rotate on their own every `rotate` milliseconds (6s by
default) among whichever faces are currently enabled.

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
- Around 24 faces are registered and cycle via button, serial, or
  auto-rotation: geometric faces (`TriangleFace` with a warm candle flicker,
  `TestPatternFace` — currently noisy so disabled by default, `Checkerboard`,
  `Bullseye`, `PacMan`), text faces (`TextFace`, including a
  `ConfigurableText` instance whose message is set live over serial - see
  "Runtime Controls"), an animated eyeball whose iris darts around inside
  the sclera (`EyeballLookAround`), and a set of static image faces (Jack
  Skellington, skull, commodore logo, WPI goat, robot, eyeball) - all
  available plain or candle-lit via a shared flicker helper.
- Runtime config (which faces are enabled, the rotate interval) can be
  changed and persisted to flash over the serial console — see "Runtime
  Controls" above.
- 3D-style shading, overlays, moving mouth/eyes, audio, sensors, and Wi-Fi
  control remain future work.

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