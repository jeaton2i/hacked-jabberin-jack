#pragma once

#include <stddef.h>
#include <stdint.h>

// Matches CountdownFace::kMaxHolidayNameLength - duplicated as a plain
// constant rather than including countdown_face.h here, so this low-level
// storage struct doesn't need to depend on a specific face's header.
constexpr size_t kPrefsHolidayNameLength = 48;

// Persisted configuration: which faces are enabled, the auto-rotate
// interval, the candle brightness scale, the face advance order, the
// ESP32 bridge link's GPIO pins, and the Countdown face's target holiday.
// Stored in the RP2040's emulated EEPROM (a 4KB flash sector reserved by
// the arduino-pico core, independent of any filesystem), so it survives
// power cycles without needing a LittleFS partition.
//
// Deliberately NOT included here: the Countdown face's *synced* date -
// that's always transient (see CountdownFace's class comment on why the
// RP2040 can't know today's date on its own), so persisting it would just
// mean booting up believing a stale, possibly very wrong "today".
struct Prefs {
  uint32_t enabledMask;
  uint32_t rotateMs;
  uint32_t brightnessPercent; // CandleFlicker::brightness() * 100
  uint32_t randomOrder;      // 0 = in-order, 1 = random
  uint32_t esp32TxPin;
  uint32_t esp32RxPin;
  char holidayName[kPrefsHolidayNameLength];
  uint32_t holidayMonth;
  uint32_t holidayDay;
};

// Loads saved prefs from flash into `out`. Returns false (leaving `out`
// untouched) if nothing valid has been saved yet.
bool loadPrefs(Prefs &out);

// Writes `prefs` to flash immediately. Blocks for a few ms while the flash
// sector erases and reprograms.
void savePrefs(const Prefs &prefs);
