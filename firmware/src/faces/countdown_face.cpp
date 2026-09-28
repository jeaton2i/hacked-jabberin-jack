#include "countdown_face.h"

#include <stdio.h>
#include <string.h>

namespace {

// Public-domain days-since-epoch <-> calendar-date conversion (Howard
// Hinnant, http://howardhinnant.github.io/date_algorithms.html) - handles
// leap years and varying month lengths correctly without pulling in a
// full <time.h>/timezone-aware localtime() stack for what's otherwise
// just "how many days between these two dates". Deliberately not
// calendar-validated (see setHoliday()'s comment) - out-of-range d/m just
// resolve to some date via this same arithmetic rather than being
// rejected.
int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

// Days from (todayYear/Month/Day) until the next occurrence - this year,
// or next if this year's has already passed - of targetMonth/targetDay.
int32_t daysUntilNext(uint16_t todayYear, uint8_t todayMonth,
                      uint8_t todayDay, uint8_t targetMonth,
                      uint8_t targetDay) {
  int32_t today = daysFromCivil(todayYear, todayMonth, todayDay);
  int32_t thisYear = daysFromCivil(todayYear, targetMonth, targetDay);
  int32_t target =
      thisYear >= today ? thisYear
                       : daysFromCivil(todayYear + 1, targetMonth, targetDay);
  return target - today;
}

} // namespace

CountdownFace::CountdownFace() : _textFace("Countdown", "Sync required") {
  strncpy(_holidayName, "Halloween", sizeof(_holidayName) - 1);
  _holidayName[sizeof(_holidayName) - 1] = '\0';
}

void CountdownFace::begin(Arduino_GFX *gfx) {
  refreshText();
  _textFace.begin(gfx);
}

void CountdownFace::update() { _textFace.update(); }

void CountdownFace::draw(Arduino_GFX *gfx) { _textFace.draw(gfx); }

void CountdownFace::setHoliday(const char *name, uint8_t month, uint8_t day) {
  strncpy(_holidayName, name, sizeof(_holidayName) - 1);
  _holidayName[sizeof(_holidayName) - 1] = '\0';
  _holidayMonth = month;
  _holidayDay = day;
  _lastDisplayedDays = -1; // force a re-render even if the day count
                          // happens to come out the same as before
  refreshText();
}

void CountdownFace::setSyncedDate(uint16_t year, uint8_t month, uint8_t day) {
  _synced = true;
  _syncedYear = year;
  _syncedMonth = month;
  _syncedDay = day;
  refreshText();
}

void CountdownFace::refreshText() {
  // Splits the holiday name on '|' (see setHoliday()'s comment) into up to
  // two lines - nameLine2 stays null for a plain single-line name.
  // TextFace::setText() drops null/empty args wherever they fall (not
  // just trailing ones), so passing a null nameLine2 below just leaves
  // this rendered as one line, no special-casing needed here.
  char nameBuf[kMaxHolidayNameLength];
  strncpy(nameBuf, _holidayName, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = '\0';
  const char *nameLine1 = nameBuf;
  const char *nameLine2 = nullptr;
  char *pipe = strchr(nameBuf, '|');
  if (pipe) {
    *pipe = '\0';
    nameLine2 = pipe + 1;
  }

  if (!_synced) {
    _textFace.setText("Sync required", nameLine1, nameLine2,
                      "(needs ESP32 bridge)");
    return;
  }

  int32_t days = daysUntilNext(_syncedYear, _syncedMonth, _syncedDay,
                               _holidayMonth, _holidayDay);
  if (days == _lastDisplayedDays) {
    return; // nothing actually changed - see the header's comment on why
            // this skips re-rendering
  }
  _lastDisplayedDays = days;

  if (days <= 0) {
    _textFace.setText("Today is", nameLine1, nameLine2);
  } else if (days == 1) {
    _textFace.setText("Tomorrow is", nameLine1, nameLine2);
  } else {
    char line1[24];
    snprintf(line1, sizeof(line1), "%ld days until", (long)days);
    _textFace.setText(line1, nameLine1, nameLine2);
  }
}
