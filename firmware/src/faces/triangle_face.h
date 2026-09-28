#pragma once

#include "candle_flicker.h"
#include "face.h"

// Initial goal #2: basic triangle-face pumpkin. Fire-like illumination
// comes from the shared CandleFlicker; each carved opening (eyes, nose,
// mouth) also gets its own radial glow - a cooler, darker red-orange rim
// right at the cut edge brightening to a hot yellow-white core through
// most of the interior, like looking through the cut at a light source -
// rather than filled with one flat color. See kGlowBandFraction's comment
// in triangle_face.cpp.
class TriangleFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  CandleFlicker _flicker;
};
