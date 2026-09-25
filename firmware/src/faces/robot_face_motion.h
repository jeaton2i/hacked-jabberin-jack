#pragma once

#include <stdint.h>

// Shared geometry + "look around" motion for the two animated Robot faces
// (RobotLookFace, CandleLitRobotLookFace). image_robot's eyes are a solid
// pupil disc inside a solid black socket disc - no texture to resample like
// EyeLookMotion's eyeball art - so the pupils are just redrawn as flat
// circles at an animated offset. Also carries the mouth's "smile" shape,
// replacing image_robot's baked-in flat bar. Geometry measured directly off
// image_robot's pixels (connected-component bounding boxes of the black
// sockets/mouth bar and white pupils, via an ad hoc script - not checked
// in), not eyeballed from the source jpg, since convert_image_to_rgb565.py
// letterboxes and mirrors the art before it reaches the panel.
enum class RobotOverride : uint8_t { kNone, kWhite, kBlack };

// kNone means "leave it to the caller's own source image" - only kWhite/
// kBlack are overrides. Callers map those to whatever their face variant's
// actual white/black means (literal RGB565, or CandleFlicker::tint()'d).
RobotOverride classifyRobotEyePixel(int16_t x, int16_t y, int16_t offsetX,
                                    int16_t offsetY);
RobotOverride classifyRobotMouthPixel(int16_t x, int16_t y);

class RobotGazeMotion {
public:
  void reset();
  void update();
  int16_t offsetX() const { return _offsetXInt; }
  int16_t offsetY() const { return _offsetYInt; }

  // Panel-space box both eye sockets occupy - callers can skip straight to
  // their own source image everywhere outside it instead of calling
  // classifyRobotEyePixel() at all.
  static int16_t boxX0();
  static int16_t boxX1();
  static int16_t boxY0();
  static int16_t boxY1();

private:
  void pickNewTarget();

  float _offsetX = 0.0f;
  float _offsetY = 0.0f;
  float _targetX = 0.0f;
  float _targetY = 0.0f;
  unsigned long _holdUntilMillis = 0;
  unsigned long _lastUpdateMillis = 0;

  // Rounded once per update(), not per pixel - the RP2040 has no hardware
  // FPU (see EyeLookMotion for the ~4fps bug this pattern is avoiding).
  int16_t _offsetXInt = 0;
  int16_t _offsetYInt = 0;
};
