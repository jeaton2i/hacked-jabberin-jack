# RP2040 Display Wiring

This is the pin map used by the `rp2040-doom-LCD` example for the Jabberin'
Jack hardware. The firmware uses an 8-bit parallel ILI9225 interface, not SPI.
Verify the ribbon adapter's connector orientation before applying power.

## Current firmware map

The firmware creates an `Arduino_RPiPicoPAR8` bus. Its eight data lines are
fixed to `GP0` through `GP7`, matching the Doom project's driver.

| Display signal | RP2040 GPIO | Pico header pin | Current code |
| --- | ---: | ---: | --- |
| D0 | GP0 | 1 | Parallel data bit 0 |
| D1 | GP1 | 2 | Parallel data bit 1 |
| D2 | GP2 | 4 | Parallel data bit 2 |
| D3 | GP3 | 5 | Parallel data bit 3 |
| D4 | GP4 | 6 | Parallel data bit 4 |
| D5 | GP5 | 7 | Parallel data bit 5 |
| D6 | GP6 | 9 | Parallel data bit 6 |
| D7 | GP7 | 10 | Parallel data bit 7 |
| RD | GP8 | 11 | Read strobe, held inactive high |
| WR | GP9 | 12 | Write strobe |
| DC / RS | GP10 | 14 | Data/command |
| RST / RESET | GP11 | 15 | Display reset |
| CS | GP12 | 16 | Chip select |
| VCC | 3V3(OUT) | 36 | Only if the display adapter is 3.3 V |
| GND | GND | Any GND pin | Common ground |
| BL / LED | TBD | TBD | Do not connect until identified |

The matching constants are in `firmware/src/display/display.cpp`. The display
driver is `Arduino_ILI9225` with 220x176 geometry and rotation 1, matching the
Doom example's landscape orientation.

## Before powering the display

1. Find the adapter labels or schematic for `VCC`, `GND`, `D0` through `D7`,
   `RD`, `WR`, `CS`, `DC`, `RST`, and `BL`. Confirm connector orientation.
2. Confirm the adapter accepts 3.3 V logic. The RP2040 GPIOs are not 5 V
   tolerant.
3. Connect grounds first and keep the display supply disconnected while
   checking continuity and pin orientation.
4. Connect the eight data lines and control lines according to the table.
5. Do not drive `BL` directly from a GPIO until its voltage and current
   requirements are known; use the adapter's supported supply or a transistor
   driver if required.

## What is confirmed

- The RP2040 firmware builds and runs its USB serial heartbeat.
- `GP0` through `GP12` match the Doom example's hardcoded display driver.
- The firmware now uses the Arduino_GFX parallel ILI9225 driver.

## What is not confirmed

- The projector adapter's connector orientation and pin numbering must still
   be checked against the physical adapter.
- Backlight wiring, display power requirements, and required ILI9225 rotation.