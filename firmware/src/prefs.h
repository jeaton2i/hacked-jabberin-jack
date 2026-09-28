#pragma once

#include <stdint.h>

// Persisted configuration: which faces are enabled, the auto-rotate
// interval, the candle brightness scale, the face advance order, and the
// ESP32 bridge link's GPIO pins. Stored in the RP2040's emulated EEPROM (a
// 4KB flash sector reserved by the arduino-pico core, independent of any
// filesystem), so it survives power cycles without needing a LittleFS
// partition.
struct Prefs {
  uint32_t enabledMask;
  uint32_t rotateMs;
  uint32_t brightnessPercent; // CandleFlicker::brightness() * 100
  uint32_t randomOrder;      // 0 = in-order, 1 = random
  uint32_t esp32TxPin;
  uint32_t esp32RxPin;
};

// Loads saved prefs from flash into `out`. Returns false (leaving `out`
// untouched) if nothing valid has been saved yet.
bool loadPrefs(Prefs &out);

// Writes `prefs` to flash immediately. Blocks for a few ms while the flash
// sector erases and reprograms.
void savePrefs(const Prefs &prefs);
