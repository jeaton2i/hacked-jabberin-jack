#pragma once

#include "candle_flicker.h"
#include "face.h"

// Initial goal #2: basic triangle-face pumpkin. Fire-like illumination
// comes from the shared CandleFlicker; each carved opening (eyes, nose,
// mouth) also shows a visible wall of pumpkin flesh around its cut edge,
// brightening to the candle's own actual color through most of the
// interior, rather than filled with one flat color - see kWallWidthPx's
// comment in triangle_face.cpp.
//
// The `animated` constructor flag additionally flaps the mouth open and
// closed on its own, as if talking - see pickNewJawTarget()/updateJaw()
// below. Registered as a second, separately toggleable face
// ("TriangleFaceAnimated" in main.cpp) rather than always-on, so the two
// looks can be switched between/compared independently.
class TriangleFace : public Face {
public:
  explicit TriangleFace(bool animated = false) : _animated(animated) {}

  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  void pickNewJawTarget();
  void updateJaw();

  const bool _animated;
  CandleFlicker _flicker;

  // Procedural "talking" jaw animation (only advanced when _animated - see
  // update()): no relation to any real audio (see main.cpp's audio
  // playback) - just a standalone, irregular open/close pattern meant to
  // look like idle chatter, a placeholder for eventually driving this from
  // actual playback amplitude instead. 0 = resting (mouth at its
  // thinnest), 1 = wide open.
  float _jawOpenness = 1.0f;
  float _jawTarget = 1.0f;
  // Absolute millis() deadline for the current hold, and easing keyed off
  // real elapsed time rather than a fixed fraction per update() call - see
  // EyeLookMotion::update()'s comment for why (same idiom, reused here).
  unsigned long _jawHoldUntilMillis = 0;
  unsigned long _lastJawUpdateMillis = 0;
};
