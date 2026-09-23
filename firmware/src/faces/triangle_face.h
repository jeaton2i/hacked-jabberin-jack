#pragma once

#include "face.h"

// Initial goal #2: basic triangle-face pumpkin, static for now. Fire-like
// illumination and 3D-style shading are follow-on work once this shape is
// confirmed against the real panel.
class TriangleFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  float _flickerPhase = 0.0f;
  float _illumination = 0.82f;
};
