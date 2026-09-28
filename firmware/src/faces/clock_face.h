#pragma once

#include <stdint.h>

#include "candle_flicker.h"
#include "face.h"

// Shows the current time as HH:MM in blocky seven-segment "LED" digits
// (with a colon that blinks once per second) instead of through a font -
// drawn directly from segment geometry rather than rendering text, so
// unlike TextFace this never needs an off-screen canvas at all.
//
// The RP2040 has no battery-backed RTC (see CountdownFace's class comment
// for the same story) - it only finds out the current time via
// setSyncedTime(), fed by the optional ESP32 bridge once it has NTP time
// (see main.cpp's "setclock" command and the bridge's own periodic sync),
// and keeps ticking on its own between syncs using millis() elapsed since
// then. Until that's happened at least once since power-up, this shows a
// "--:--" placeholder instead of a wrong time.
class ClockFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  // Feeds in the actual current wall-clock time, as reported by the ESP32
  // bridge's "setclock" command (paired with "settime" for the date - see
  // docs/esp32-network-bridge.md).
  void setSyncedTime(uint8_t hour, uint8_t minute, uint8_t second);

  // 24-hour by default. In 12-hour mode, midnight/noon show as 12 (not 0)
  // and the leading hour digit is left blank (no lit segments) rather than
  // showing "0", same as most real digital clocks - e.g. "1:05", not
  // "01:05".
  void setUse12Hour(bool use12Hour);

  bool synced() const { return _synced; }
  bool use12Hour() const { return _use12Hour; }

private:
  // Recomputes the currently-displayed H:M (or the "--:--" placeholder)
  // from the last sync + elapsed time, and rebuilds _renderedMask if
  // what's actually displayed (digits or colon blink state) changed -
  // called from update() every frame, but cheap to call when nothing
  // changed since it bails out before doing any real work in that case.
  void refreshDisplay();

  // Rebuilds _renderedMask for the given displayed hour/minute (ignored
  // when !_synced) and colon state.
  void render(uint8_t displayHour, uint8_t displayMinute, bool colonOn);

  bool _synced = false;
  bool _use12Hour = false;
  uint8_t _syncedHour = 0;
  uint8_t _syncedMinute = 0;
  uint8_t _syncedSecond = 0;
  unsigned long _syncMillis = 0; // millis() at the moment of the last sync

  // Sentinels so the very first refreshDisplay() call always renders -
  // 255 can never match a real hour/minute (0-23/0-59).
  uint8_t _lastDisplayedHour = 255;
  uint8_t _lastDisplayedMinute = 255;
  bool _lastColonOn = true;
  bool _everRendered = false;

  CandleFlicker _flicker;
  Arduino_GFX *_gfx = nullptr;
  uint8_t *_renderedMask = nullptr; // 1 bit/pixel, lazily allocated
  int16_t _width = 0;
  int16_t _height = 0;
};
