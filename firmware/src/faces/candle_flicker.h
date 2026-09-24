#pragma once

#include <Arduino_GFX_Library.h>
#include <math.h>

// Shared warm, flickering "lit from behind by a candle" color, used by
// anything that should breathe/flicker together: TriangleFace, flickering
// text, and candle-lit image faces.
class CandleFlicker {
public:
  void update() {
    constexpr float kTwoPi = 2.0f * 3.14159265359f;
    _phase += 0.09f;
    if (_phase >= kTwoPi) {
      _phase -= kTwoPi;
    }
    _illumination = 0.82f + 0.10f * sinf(_phase) +
                    0.05f * sinf(_phase * 2.37f) +
                    0.03f * sinf(_phase * 5.11f);
  }

  uint16_t color() const {
    uint8_t red = 255.0f * _illumination;
    uint8_t green = 145.0f * _illumination;
    return RGB565(red, green, 0);
  }

private:
  float _phase = 0.0f;
  float _illumination = 0.82f;
};
