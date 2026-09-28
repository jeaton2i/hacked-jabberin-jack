#pragma once

#include <stdint.h>

#include "face.h"
#include "text_face.h"

// Stretch goal: shows "<N> days until <Holiday>", where <Holiday> and its
// target month/day are configurable (see setHoliday(), main.cpp's
// "countdown" serial command) and default to Halloween (Oct 31).
//
// The RP2040 has no battery-backed RTC, so it has no idea what today's
// actual date is on its own - it only finds out via setSyncedDate(), fed
// by the optional ESP32 bridge once that's gotten a real date over NTP
// (see main.cpp's "settime" command and the bridge's own periodic sync).
// Until that's happened at least once since the last power-up, this face
// just shows a placeholder explaining that instead of a countdown.
class CountdownFace : public Face {
public:
  static constexpr size_t kMaxHolidayNameLength = 24;

  CountdownFace();

  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  // Sets the recurring target (month/day, no year - always counts to
  // whichever occurrence is next) and its display name. name is copied
  // and truncated to fit kMaxHolidayNameLength-1 characters plus a null
  // terminator; month/day aren't calendar-validated beyond a plausible
  // range (see main.cpp's "countdown" command) - an invalid combination
  // (e.g. Feb 30) just resolves to *some* date via daysFromCivil's math
  // instead of being rejected outright.
  void setHoliday(const char *name, uint8_t month, uint8_t day);

  // Feeds in the actual current calendar date, as reported by the ESP32
  // bridge's "settime" command - see the class comment above for why this
  // is the only way this face ever finds out what day it is.
  void setSyncedDate(uint16_t year, uint8_t month, uint8_t day);

  const char *holidayName() const { return _holidayName; }
  uint8_t holidayMonth() const { return _holidayMonth; }
  uint8_t holidayDay() const { return _holidayDay; }
  bool synced() const { return _synced; }

  // -1 when not yet synced (see synced()); otherwise however many days
  // until the next occurrence of the target date, 0 on the day itself.
  // Just returns whatever refreshText() last computed - it's already kept
  // current by every setHoliday()/setSyncedDate() call.
  int32_t daysUntil() const { return _synced ? _lastDisplayedDays : -1; }

private:
  // Recomputes the displayed day count (if synced) or the "needs sync"
  // placeholder, and pushes it to _textFace - called whenever anything
  // that could change what should be on screen happens (a fresh sync, or
  // the holiday config changing), not once per frame from update(): the
  // day count only ever changes when setSyncedDate() is called again, and
  // re-rendering text needlessly every frame just wastes cycles no face
  // elsewhere in this codebase spends on genuinely static content.
  void refreshText();

  char _holidayName[kMaxHolidayNameLength];
  uint8_t _holidayMonth = 10;
  uint8_t _holidayDay = 31;

  bool _synced = false;
  uint16_t _syncedYear = 0;
  uint8_t _syncedMonth = 0;
  uint8_t _syncedDay = 0;

  // Forces refreshText()'s very first call to actually render - no real
  // day count is ever negative, so this can never accidentally match one.
  int32_t _lastDisplayedDays = -1;

  TextFace _textFace;
};
