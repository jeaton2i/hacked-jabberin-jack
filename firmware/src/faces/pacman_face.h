#pragma once

#include "candle_flicker.h"
#include "face.h"

// A carved-pumpkin-style Pac-Man (candle-lit, mouth opening/closing) that
// travels right to left eating a row of dots and one larger power pill.
// A ghost chases Pac-Man until the power pill is eaten, then flees for
// the rest of that pass.
class PacManFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  static constexpr int kDotCount = 8;

private:
  void resetRound();

  CandleFlicker _flicker;
  float _x = 0.0f;
  float _mouthPhase = 0.0f;
  bool _dotEaten[kDotCount] = {};
  float _dotX[kDotCount] = {};

  bool _pillEaten = false;
  float _ghostX = 0.0f;
  bool _ghostFleeing = false;
};
