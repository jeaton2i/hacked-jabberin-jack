#pragma once

#include <stdint.h>

// One-shot diagnostic that talks to the parallel bus directly (bypassing
// Arduino_GFX) to read the ILI9225's read-only Driver Code register, which
// should report 0x9225 on genuine ILI9225 silicon. Use this to confirm the
// panel/controller identity and that RD is wired correctly, independent of
// whether the Arduino_GFX driver is working.
uint16_t probeIli9225DriverCode();
