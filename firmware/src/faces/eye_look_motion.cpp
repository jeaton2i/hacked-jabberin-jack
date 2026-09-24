#include "eye_look_motion.h"

#include <Arduino.h>
#include <math.h>

#include "../assets/image_eyeball.h"
#include "../assets/image_eyeball_bg.h"

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;

// Measured directly from image_eyeball's pixels (see tools/ - threshold the
// near-black pupil and blue-dominant iris ring to find their bounds). The
// iris+pupil disc's resting center and outer radius, with a couple of
// pixels of sclera fringe folded in so the crop blends at its edge instead
// of showing a hard seam.
constexpr float kIrisRestX = 105.5f;
constexpr float kIrisRestY = 81.5f;
constexpr float kIrisRadius = 56.0f;

// How far the iris may drift from its resting position - wider side to
// side than up/down, like a wary glance rather than a bobblehead. Checked
// against the sclera disc's outer edge (center ~(109.5, 87.5), radius
// ~87.5) at every angle to confirm the disc never pokes past it.
constexpr float kMaxOffsetX = 24.0f;
constexpr float kMaxOffsetY = 9.0f;

// Glances vary from a small shift to a full dart to the side, instead of
// (almost) always being a big swing - a mix reads calmer than constant
// maximum-effort movement.
constexpr float kMinTargetFraction = 0.35f;

// Time constant (ms) for easing toward the target: after this many ms,
// ~63% of the remaining distance has been closed. Keyed off real elapsed
// time (see update()) rather than a fixed fraction per call, so the glide
// takes the same real time regardless of the actual update() call rate.
// (24ms was tuned assuming a ~16ms main loop; the real loop turned out to
// run more like ~75ms/frame - at that rate 24ms closes ~96% of the
// distance in a single frame, i.e. a teleport, not a glide. This is long
// enough to visibly span several real frames instead.)
constexpr float kEasingTimeConstantMs = 160.0f;
constexpr unsigned long kMinHoldMs = 500;
constexpr unsigned long kMaxHoldMs = 1200;
// Occasionally freezes for a beat, as if listening for something, instead
// of constantly darting - reads as nervous rather than just twitchy.
constexpr unsigned long kLongHoldMinMs = 1500;
constexpr unsigned long kLongHoldMaxMs = 2500;
constexpr int kLongHoldPercentChance = 15;

// The art has a soft shadow ring around the iris that doesn't match the
// sclera tone elsewhere it gets relocated to; blending over a few pixels at
// the boundary instead of a hard cutoff turns that mismatch into a gradient
// instead of a visible seam.
constexpr float kFeather = 6.0f;

// Every pixel the moving iris disc (plus feather) could possibly touch,
// centered on its resting position.
constexpr float kMaxReachX = kMaxOffsetX + kIrisRadius + kFeather;
constexpr float kMaxReachY = kMaxOffsetY + kIrisRadius + kFeather;

// Integer versions of the above for sample()'s hot per-pixel path: the
// RP2040 (Cortex-M0+) has no hardware FPU, so float multiplies/adds are
// emulated in software and were the actual cause of a ~4fps effective
// frame rate when done per pixel, tens of thousands of times a frame (see
// EyeLookMotion::update() for where the per-frame float->int rounding now
// happens instead). A pixel of imprecision from rounding kIrisRestX/Y here
// is invisible; the frame-rate difference isn't.
constexpr int16_t kIrisRestXInt = 106; // round(105.5)
constexpr int16_t kIrisRestYInt = 82;  // round(81.5)
constexpr int32_t kInnerSqInt =
    (int32_t)((kIrisRadius - kFeather) * (kIrisRadius - kFeather));
constexpr int32_t kOuterSqInt =
    (int32_t)((kIrisRadius + kFeather) * (kIrisRadius + kFeather));

// When EyeLookMotion::debugLogging() is on (see main.cpp's "debug" serial
// command; off by default), prints each dart's target/hold choice and
// occasionally flags an unusually slow frame - useful for checking the
// motion is doing what it's tuned to without needing to eyeball timing
// off the physical display.
// A frame this much slower than the ~16ms the main loop budgets for is
// worth flagging - if it's happening, hold durations (measured in ticks,
// not milliseconds) run longer in real time than their tick count implies.
constexpr unsigned long kSlowFrameThresholdMs = 30;
// Rate-limit slow-frame logging itself - if every frame is slow, printing
// every single one would make that worse, not just report it.
constexpr unsigned long kSlowFrameLogIntervalMs = 1000;

int16_t clampCoord(int16_t v, int16_t hi) {
  if (v < 0) {
    return 0;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

uint16_t blend(uint16_t a, uint16_t b, float t) {
  uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint8_t r = ar + (br - ar) * t;
  uint8_t g = ag + (bg - ag) * t;
  uint8_t bl = ab + (bb - ab) * t;
  return (r << 11) | (g << 5) | bl;
}
} // namespace

bool EyeLookMotion::s_debugLogging = false;

void EyeLookMotion::reset() {
  randomSeed(micros());
  _offsetX = 0.0f;
  _offsetY = 0.0f;
  _targetX = 0.0f;
  _targetY = 0.0f;
  unsigned long now = millis();
  _holdUntilMillis = now + kMinHoldMs;
  _lastPickMillis = now;
  _lastUpdateMillis = now;
}

void EyeLookMotion::pickNewTarget() {
  unsigned long now = millis();
  unsigned long heldForMs = now - _lastPickMillis;
  _lastPickMillis = now;

  float angle = random(0, 360) * (PI / 180.0f);
  float fraction = kMinTargetFraction +
                   (1.0f - kMinTargetFraction) * (random(0, 101) / 100.0f);
  float fromX = _offsetX, fromY = _offsetY;
  _targetX = cosf(angle) * kMaxOffsetX * fraction;
  _targetY = sinf(angle) * kMaxOffsetY * fraction;

  bool longHold = random(0, 100) < kLongHoldPercentChance;
  unsigned long holdMs = longHold ? random(kLongHoldMinMs, kLongHoldMaxMs + 1)
                                  : random(kMinHoldMs, kMaxHoldMs + 1);
  _holdUntilMillis = now + holdMs;

  if (debugLogging()) {
    // heldForMs is how long the *previous* hold actually lasted, for
    // comparing against what it was supposed to be.
    Serial.print("[");
    Serial.print(now);
    Serial.print("ms] EyeLook: from=(");
    Serial.print(fromX, 1);
    Serial.print(",");
    Serial.print(fromY, 1);
    Serial.print(") to=(");
    Serial.print(_targetX, 1);
    Serial.print(",");
    Serial.print(_targetY, 1);
    Serial.print(") holdMs=");
    Serial.print(holdMs);
    Serial.print(longHold ? " (long)" : " (short)");
    Serial.print(" prevHoldActualMs=");
    Serial.println(heldForMs);
  }
}

void EyeLookMotion::update() {
  unsigned long now = millis();
  unsigned long elapsedMs = now - _lastUpdateMillis;
  _lastUpdateMillis = now;

  if (debugLogging() && elapsedMs > kSlowFrameThresholdMs &&
      now - _lastSlowFrameLogMillis >= kSlowFrameLogIntervalMs) {
    _lastSlowFrameLogMillis = now;
    Serial.print("[");
    Serial.print(now);
    Serial.print("ms] EyeLook: slow frame, ");
    Serial.print(elapsedMs);
    Serial.println("ms since previous update() (further slow frames in "
                    "the next second won't be re-logged)");
  }

  if ((long)(now - _holdUntilMillis) >= 0) {
    pickNewTarget();
  }

  // Time-based easing: converge toward the target at a fixed *rate*, not
  // a fixed fraction per update() call - so it looks equally snappy
  // whether update() is actually being called at 60fps or 15fps, instead
  // of the dart (and, if ticks instead of ms drove the hold timer too,
  // the pauses) quietly taking longer in real time on a slower frame rate.
  float alpha = 1.0f - expf(-(float)elapsedMs / kEasingTimeConstantMs);
  _offsetX += (_targetX - _offsetX) * alpha;
  _offsetY += (_targetY - _offsetY) * alpha;

  // Round once per frame, not per pixel - see the comment above kInnerSqInt.
  _centerXInt = (int16_t)lroundf(kIrisRestX + _offsetX);
  _centerYInt = (int16_t)lroundf(kIrisRestY + _offsetY);
}

uint16_t EyeLookMotion::sample(int16_t x, int16_t y) const {
  int16_t dx = x - _centerXInt;
  int16_t dy = y - _centerYInt;
  int32_t distSq = (int32_t)dx * dx + (int32_t)dy * dy;
  // image_eyeball has the iris permanently baked in at its *resting*
  // position - using it for "background" here would mean the resting iris
  // silently reappears the moment the animated one moves far enough away
  // to stop covering it (this was the actual cause of the reported
  // "leftover iris" artifacting, not a display/timing issue - see
  // image_eyeball_bg.h, which has that area filled in with plausible
  // sclera instead).
  uint16_t background = image_eyeball_bg[(int32_t)y * kPanelWidth + x];

  if (distSq >= kOuterSqInt) {
    return background;
  }
  // Resample the same relative offset from the iris's *resting* position,
  // i.e. the original unmodified iris/pupil artwork, just relocated.
  int16_t srcX = clampCoord(kIrisRestXInt + dx, kPanelWidth - 1);
  int16_t srcY = clampCoord(kIrisRestYInt + dy, kPanelHeight - 1);
  uint16_t iris = image_eyeball[(int32_t)srcY * kPanelWidth + srcX];
  if (distSq <= kInnerSqInt) {
    return iris;
  }
  // Linear in distSq rather than distance itself (which would need a
  // sqrt) - the feather band is only a few pixels wide, so the slightly
  // different falloff curve this implies isn't visible.
  float t = (float)(distSq - kInnerSqInt) / (float)(kOuterSqInt - kInnerSqInt);
  return blend(iris, background, t);
}

int16_t EyeLookMotion::boxX0() {
  return clampCoord((int16_t)(kIrisRestX - kMaxReachX), kPanelWidth - 1);
}
int16_t EyeLookMotion::boxX1() {
  return clampCoord((int16_t)(kIrisRestX + kMaxReachX) + 1, kPanelWidth - 1);
}
int16_t EyeLookMotion::boxY0() {
  return clampCoord((int16_t)(kIrisRestY - kMaxReachY), kPanelHeight - 1);
}
int16_t EyeLookMotion::boxY1() {
  return clampCoord((int16_t)(kIrisRestY + kMaxReachY) + 1, kPanelHeight - 1);
}
