#include "prefs.h"

#include <EEPROM.h>
#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x4A4B3108; // "JK" + format version 8
                                       // (added the Clock face's 12/24-hour
                                       // format flag)

struct StoredPrefs {
  uint32_t magic;
  uint32_t enabledMask;
  uint32_t rotateMs;
  uint32_t brightnessPercent;
  uint32_t randomOrder;
  uint32_t esp32TxPin;
  uint32_t esp32RxPin;
  char holidayName[kPrefsHolidayNameLength];
  uint32_t holidayMonth;
  uint32_t holidayDay;
  char textLines[kPrefsTextLineCount][kPrefsTextLineLength];
  uint32_t textFontIndex;
  uint32_t clockUse12Hour;
  uint32_t checksum;
};

// Folds `len` bytes at `data` into `sum`, 4 at a time - `len` must be a
// multiple of 4 (true of every char-array field this is used on).
uint32_t foldBytes(uint32_t sum, const void *data, size_t len) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
  for (size_t i = 0; i < len; i += 4) {
    uint32_t chunk;
    memcpy(&chunk, bytes + i, sizeof(chunk));
    sum ^= chunk;
  }
  return sum;
}

uint32_t checksumOf(const StoredPrefs &prefs) {
  uint32_t sum = prefs.magic ^ prefs.enabledMask ^ prefs.rotateMs ^
                prefs.brightnessPercent ^ prefs.randomOrder ^
                prefs.esp32TxPin ^ prefs.esp32RxPin ^ prefs.holidayMonth ^
                prefs.holidayDay ^ prefs.textFontIndex ^
                prefs.clockUse12Hour;
  static_assert(kPrefsHolidayNameLength % 4 == 0,
               "holidayName must be a multiple of 4 bytes for this loop");
  static_assert(kPrefsTextLineLength % 4 == 0,
               "textLines rows must be a multiple of 4 bytes for this loop");
  sum = foldBytes(sum, prefs.holidayName, kPrefsHolidayNameLength);
  for (size_t i = 0; i < kPrefsTextLineCount; i++) {
    sum = foldBytes(sum, prefs.textLines[i], kPrefsTextLineLength);
  }
  return sum;
}

} // namespace

bool loadPrefs(Prefs &out) {
  EEPROM.begin(sizeof(StoredPrefs));
  StoredPrefs stored;
  EEPROM.get(0, stored);
  EEPROM.end();

  if (stored.magic != kMagic || stored.checksum != checksumOf(stored)) {
    return false;
  }

  out.enabledMask = stored.enabledMask;
  out.rotateMs = stored.rotateMs;
  out.brightnessPercent = stored.brightnessPercent;
  out.randomOrder = stored.randomOrder;
  out.esp32TxPin = stored.esp32TxPin;
  out.esp32RxPin = stored.esp32RxPin;
  memcpy(out.holidayName, stored.holidayName, sizeof(out.holidayName));
  out.holidayName[sizeof(out.holidayName) - 1] = '\0';
  out.holidayMonth = stored.holidayMonth;
  out.holidayDay = stored.holidayDay;
  for (size_t i = 0; i < kPrefsTextLineCount; i++) {
    memcpy(out.textLines[i], stored.textLines[i], sizeof(out.textLines[i]));
    out.textLines[i][sizeof(out.textLines[i]) - 1] = '\0';
  }
  out.textFontIndex = stored.textFontIndex;
  out.clockUse12Hour = stored.clockUse12Hour;
  return true;
}

void savePrefs(const Prefs &prefs) {
  StoredPrefs stored;
  stored.magic = kMagic;
  stored.enabledMask = prefs.enabledMask;
  stored.rotateMs = prefs.rotateMs;
  stored.brightnessPercent = prefs.brightnessPercent;
  stored.randomOrder = prefs.randomOrder;
  stored.esp32TxPin = prefs.esp32TxPin;
  stored.esp32RxPin = prefs.esp32RxPin;
  memcpy(stored.holidayName, prefs.holidayName, sizeof(stored.holidayName));
  stored.holidayName[sizeof(stored.holidayName) - 1] = '\0';
  stored.holidayMonth = prefs.holidayMonth;
  stored.holidayDay = prefs.holidayDay;
  for (size_t i = 0; i < kPrefsTextLineCount; i++) {
    memcpy(stored.textLines[i], prefs.textLines[i],
          sizeof(stored.textLines[i]));
    stored.textLines[i][sizeof(stored.textLines[i]) - 1] = '\0';
  }
  stored.textFontIndex = prefs.textFontIndex;
  stored.clockUse12Hour = prefs.clockUse12Hour;
  stored.checksum = checksumOf(stored);

  EEPROM.begin(sizeof(StoredPrefs));
  EEPROM.put(0, stored);
  EEPROM.commit();
  EEPROM.end();
}
