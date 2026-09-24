#pragma once

#include <stdint.h>

// Shared "look around" motion + pixel sampling for the eyeball image's
// iris/pupil disc: darts to a new offset at irregular intervals and eases
// toward it, then answers what color the source eyeball image should show
// at a given panel position given the current offset (either the static
// sclera/veins, or the relocated iris/pupil disc, blended at the boundary).
// Used by both the plain and candle-lit animated eye faces so the motion
// feel and the disc geometry only need tuning in one place.
class EyeLookMotion {
public:
  void reset();
  void update();

  // Raw (untinted) RGB565 color the source eyeball image shows at panel
  // position (x, y) given the current iris offset.
  uint16_t sample(int16_t x, int16_t y) const;

  // Panel-space bounding box containing every pixel sample() can possibly
  // treat as anything other than the source image unchanged - callers can
  // skip straight to the source image outside it instead of calling
  // sample() at all.
  static int16_t boxX0();
  static int16_t boxX1();
  static int16_t boxY0();
  static int16_t boxY1();

  // Runtime on/off switch for the "from/to/hold" and "slow frame" logging
  // in update()/pickNewTarget() - off by default. See main.cpp's "debug"
  // serial command.
  static void setDebugLogging(bool enabled) { s_debugLogging = enabled; }
  static bool debugLogging() { return s_debugLogging; }

private:
  void pickNewTarget();

  float _offsetX = 0.0f;
  float _offsetY = 0.0f;
  float _targetX = 0.0f;
  float _targetY = 0.0f;
  // Absolute millis() deadline for the current hold, and easing keyed off
  // real elapsed time rather than a fixed fraction per update() call - so
  // both feel the same regardless of how fast update() actually gets
  // called, instead of quietly assuming a ~60fps main loop that may not
  // hold (see EyeLookMotion::update()).
  unsigned long _holdUntilMillis = 0;

  // Rounded to integers once per update() instead of per pixel - the
  // RP2040 has no hardware FPU, so redoing float math (as sample() used
  // to) tens of thousands of times per frame was the actual cause of a
  // ~4fps effective frame rate. sample() only ever does integer math now.
  int16_t _centerXInt = 0;
  int16_t _centerYInt = 0;

  // Debug-log bookkeeping only - see debugLogging() above.
  unsigned long _lastPickMillis = 0;
  unsigned long _lastUpdateMillis = 0;
  unsigned long _lastSlowFrameLogMillis = 0;

  static bool s_debugLogging;
};
