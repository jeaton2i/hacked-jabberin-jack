#pragma once

#include "candle_flicker.h"
#include "face.h"

// A carved palm-tree-and-waves scene: the fronds fan out and sway in a
// gentle wind, and the water is a few horizontal bands that travel like
// real waves. Loosely modeled on a "tropical sunset" pumpkin stencil -
// sun behind a leaning palm, wavy shoreline below - not a literal copy.
class TropicalFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  CandleFlicker _flicker;
  float _windPhase = 0.0f;
  float _wavePhase = 0.0f;
};
