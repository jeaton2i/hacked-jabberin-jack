#pragma once

#include "face.h"

// Concentric 10px rings centered on the panel, plus a red border, for
// mapping out the visible circular area more precisely than the
// checkerboard's square grid can.
class BullseyeFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override {}
  void draw(Arduino_GFX *gfx) override {}
};
