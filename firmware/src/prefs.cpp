#include "prefs.h"

#include <EEPROM.h>

namespace {

constexpr uint32_t kMagic = 0x4A4B3102; // "JK" + format version 2

struct StoredPrefs {
  uint32_t magic;
  uint32_t enabledMask;
  uint32_t rotateMs;
  uint32_t brightnessPercent;
  uint32_t checksum;
};

uint32_t checksumOf(const StoredPrefs &prefs) {
  return prefs.magic ^ prefs.enabledMask ^ prefs.rotateMs ^
         prefs.brightnessPercent;
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
  return true;
}

void savePrefs(const Prefs &prefs) {
  StoredPrefs stored;
  stored.magic = kMagic;
  stored.enabledMask = prefs.enabledMask;
  stored.rotateMs = prefs.rotateMs;
  stored.brightnessPercent = prefs.brightnessPercent;
  stored.checksum = checksumOf(stored);

  EEPROM.begin(sizeof(StoredPrefs));
  EEPROM.put(0, stored);
  EEPROM.commit();
  EEPROM.end();
}
