#pragma once

#include "face.h"

// Alternating black/white grid plus a red border marking the panel's true
// edges, for lining up how much of the display is actually visible once
// mounted behind the pumpkin's cutout.
class CheckerboardFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override {}
  void draw(Arduino_GFX *gfx) override {}
};
