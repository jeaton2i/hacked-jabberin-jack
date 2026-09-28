#pragma once

#include "candle_flicker.h"
#include "face.h"

// Initial goal #2: basic triangle-face pumpkin. Fire-like illumination
// comes from the shared CandleFlicker; each carved opening (eyes, nose,
// mouth) is also shaded top-to-bottom as if lit by a candle low in the
// pumpkin body, rather than filled with one flat color - see
// verticalShadeScale() in triangle_face.cpp.
class TriangleFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  CandleFlicker _flicker;
};
