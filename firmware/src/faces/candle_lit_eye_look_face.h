#pragma once

#include "candle_flicker.h"
#include "eye_look_motion.h"
#include "face.h"

// EyeLookFace's iris-darts-around motion, candle-lit like
// CandleLitImageFace: every pixel is retinted by the current flicker color
// each frame, so (unlike EyeLookFace) the whole panel has to be redrawn
// every frame regardless of where the iris currently is - the flicker
// itself is constantly changing everywhere, not just around the iris.
class CandleLitEyeLookFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  EyeLookMotion _motion;
  CandleFlicker _flicker;
};
