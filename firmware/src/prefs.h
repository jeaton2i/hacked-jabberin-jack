#pragma once

#include <stdint.h>

// Persisted configuration: which faces are enabled and the auto-rotate
// interval. Stored in the RP2040's emulated EEPROM (a 4KB flash sector
// reserved by the arduino-pico core, independent of any filesystem), so it
// survives power cycles without needing a LittleFS partition.
struct Prefs {
  uint32_t enabledMask;
  uint32_t rotateMs;
};

// Loads saved prefs from flash into `out`. Returns false (leaving `out`
// untouched) if nothing valid has been saved yet.
bool loadPrefs(Prefs &out);

// Writes `prefs` to flash immediately. Blocks for a few ms while the flash
// sector erases and reprograms.
void savePrefs(const Prefs &prefs);
