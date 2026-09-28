#include "prefs.h"

#include <EEPROM.h>
#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x4A4B3105; // "JK" + format version 5

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
  uint32_t checksum;
};

uint32_t checksumOf(const StoredPrefs &prefs) {
  uint32_t sum = prefs.magic ^ prefs.enabledMask ^ prefs.rotateMs ^
                prefs.brightnessPercent ^ prefs.randomOrder ^
                prefs.esp32TxPin ^ prefs.esp32RxPin ^ prefs.holidayMonth ^
                prefs.holidayDay;
  // holidayName folded in 4 bytes at a time (kPrefsHolidayNameLength is a
  // multiple of 4) - same "just XOR everything" scheme as every other
  // field above, extended to a byte array.
  static_assert(kPrefsHolidayNameLength % 4 == 0,
               "holidayName must be a multiple of 4 bytes for this loop");
  const uint8_t *name =
      reinterpret_cast<const uint8_t *>(prefs.holidayName);
  for (size_t i = 0; i < kPrefsHolidayNameLength; i += 4) {
    uint32_t chunk;
    memcpy(&chunk, name + i, sizeof(chunk));
    sum ^= chunk;
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
  stored.checksum = checksumOf(stored);

  EEPROM.begin(sizeof(StoredPrefs));
  EEPROM.put(0, stored);
  EEPROM.commit();
  EEPROM.end();
}
