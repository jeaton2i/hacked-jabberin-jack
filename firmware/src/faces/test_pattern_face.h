#pragma once

#include "face.h"

// Initial goal #1: simple test pattern mode. Cycles color bars so you can
// confirm the panel, wiring, and orientation are correct before building
// anything more elaborate on top.
class TestPatternFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  uint8_t _frame = 0;
};
