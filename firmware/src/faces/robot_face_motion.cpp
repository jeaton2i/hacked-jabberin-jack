#include "robot_face_motion.h"

#include <Arduino.h>
#include <math.h>

namespace {
// Eye sockets: solid black discs, concentric with the (also solid) white
// pupil disc at rest.
constexpr int16_t kEyeLeftX = 85, kEyeRightX = 134, kEyeY = 95;
constexpr int16_t kSocketRadius = 15;
constexpr int16_t kPupilRadius = 6;
constexpr int32_t kSocketRadiusSq = (int32_t)kSocketRadius * kSocketRadius;
constexpr int32_t kPupilRadiusSq = (int32_t)kPupilRadius * kPupilRadius;

// How far the pupil may drift from the socket's center - socket radius
// minus pupil radius, with a couple of pixels to spare so it never pokes
// out at any angle.
constexpr int16_t kMaxGazeReach = 8;

constexpr int16_t kBoxMargin = 2;
constexpr int16_t kBoxX0 = kEyeLeftX - kSocketRadius - kBoxMargin;
constexpr int16_t kBoxX1 = kEyeRightX + kSocketRadius + kBoxMargin;
constexpr int16_t kBoxY0 = kEyeY - kSocketRadius - kBoxMargin;
constexpr int16_t kBoxY1 = kEyeY + kSocketRadius + kBoxMargin;

// Same time-based-easing reasoning as EyeLookMotion: keyed off real
// elapsed ms so the glide/hold feel the same regardless of the actual
// update() call rate.
constexpr float kEasingTimeConstantMs = 160.0f;
constexpr unsigned long kMinHoldMs = 700;
constexpr unsigned long kMaxHoldMs = 1800;
constexpr float kMinTargetFraction = 0.35f;

constexpr float kPi = 3.14159265359f;

// Mouth: replaces image_robot's flat bar with a curved "smile" band of the
// same thickness, bowed down in the middle and rising to the original
// bar's height at each end - see classifyRobotMouthPixel().
constexpr int16_t kMouthCenterY = 127;
constexpr int16_t kMouthLeftX = 89, kMouthRightX = 131;
constexpr int16_t kMouthThickness = 7;
constexpr float kSmileDepth = 9.0f;
} // namespace

RobotOverride classifyRobotEyePixel(int16_t x, int16_t y, int16_t offsetX,
                                    int16_t offsetY) {
  const int16_t centers[2] = {kEyeLeftX, kEyeRightX};
  for (int16_t cx : centers) {
    int32_t dx = x - cx, dy = y - kEyeY;
    if (dx * dx + dy * dy > kSocketRadiusSq) {
      continue;
    }
    int32_t pdx = x - (cx + offsetX), pdy = y - (kEyeY + offsetY);
    return (pdx * pdx + pdy * pdy <= kPupilRadiusSq) ? RobotOverride::kWhite
                                                     : RobotOverride::kBlack;
  }
  return RobotOverride::kNone;
}

RobotOverride classifyRobotMouthPixel(int16_t x, int16_t y) {
  // The curve only ever sags *down* from kMouthCenterY (sinf(...) above is
  // never negative), so the band never reaches above it - bounding y here
  // too, not just x, matters because this is called for every pixel in the
  // panel (see RobotLookFace::begin()): without it, every row in this x
  // range - including the antenna, well above the face plate - would get
  // forced to white/black instead of only the few rows near the mouth.
  constexpr int16_t kBandTop = kMouthCenterY - kMouthThickness / 2 - 1;
  constexpr int16_t kBandBottom =
      kMouthCenterY + (int16_t)kSmileDepth + kMouthThickness / 2 + 1;
  if (x < kMouthLeftX || x > kMouthRightX || y < kBandTop || y > kBandBottom) {
    return RobotOverride::kNone;
  }
  float progress = (float)(x - kMouthLeftX) / (float)(kMouthRightX - kMouthLeftX);
  // Lowest (largest y) at the center, rising back to kMouthCenterY at each
  // end - a "smile" arc, not a frown (see face_preview.py's smile_arc_y for
  // the same shape used the other way, as a jack-o-lantern's mouth edges).
  float curveY = kMouthCenterY + kSmileDepth * sinf(kPi * progress);
  float half = kMouthThickness / 2.0f;
  if ((float)y < curveY - half || (float)y > curveY + half) {
    return RobotOverride::kWhite;
  }
  return RobotOverride::kBlack;
}

void RobotGazeMotion::reset() {
  randomSeed(micros());
  _offsetX = _offsetY = _targetX = _targetY = 0.0f;
  unsigned long now = millis();
  _holdUntilMillis = now + kMinHoldMs;
  _lastUpdateMillis = now;
  _offsetXInt = _offsetYInt = 0;
}

void RobotGazeMotion::pickNewTarget() {
  unsigned long now = millis();
  float angle = random(0, 360) * (kPi / 180.0f);
  float fraction = kMinTargetFraction +
                  (1.0f - kMinTargetFraction) * (random(0, 101) / 100.0f);
  _targetX = cosf(angle) * kMaxGazeReach * fraction;
  _targetY = sinf(angle) * kMaxGazeReach * fraction;
  _holdUntilMillis = now + random(kMinHoldMs, kMaxHoldMs + 1);
}

void RobotGazeMotion::update() {
  unsigned long now = millis();
  unsigned long elapsedMs = now - _lastUpdateMillis;
  _lastUpdateMillis = now;

  if ((long)(now - _holdUntilMillis) >= 0) {
    pickNewTarget();
  }

  float alpha = 1.0f - expf(-(float)elapsedMs / kEasingTimeConstantMs);
  _offsetX += (_targetX - _offsetX) * alpha;
  _offsetY += (_targetY - _offsetY) * alpha;
  _offsetXInt = (int16_t)lroundf(_offsetX);
  _offsetYInt = (int16_t)lroundf(_offsetY);
}

int16_t RobotGazeMotion::boxX0() { return kBoxX0; }
int16_t RobotGazeMotion::boxX1() { return kBoxX1; }
int16_t RobotGazeMotion::boxY0() { return kBoxY0; }
int16_t RobotGazeMotion::boxY1() { return kBoxY1; }
