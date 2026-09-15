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
- `Arduino_GFX/` and `rp2040-doom-LCD/` — reference material only, not part
  of the build. `firmware/` pulls Arduino_GFX from the PlatformIO registry
  instead of these local copies.

## Getting Started

1. Install [PlatformIO](https://platformio.org/install/cli) (VS Code
   extension or CLI).
2. `cd firmware && pio run` to build, `pio run -t upload` to flash.
3. The default build runs `TestPatternFace` (cycling color bars) so you can
   confirm panel wiring and orientation before building anything else.
4. Once the projector panel's driver chip, resolution, and pinout are known,
   replace the placeholder ST7789 wiring in `firmware/src/display/display.cpp`.

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