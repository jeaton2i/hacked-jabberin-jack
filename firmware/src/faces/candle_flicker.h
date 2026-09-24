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

  // Depth-carved look: instead of a flat silhouette, a source-image
  // pixel's own brightness controls how much candle light comes through,
  // like variable pumpkin wall thickness - bright/white areas are cut all
  // the way through (full flicker color), darker but non-black areas are
  // left thicker (dim glow), true black is untouched background.
  //
  // preserveRed keeps red source pixels (eyes, accents) a distinct solid
  // red instead of folding them into the same orange as everything else -
  // otherwise they wash out and disappear into the body. Pass false for
  // source art that's mostly red itself (a school-color logo, say), where
  // that would instead flatten most of the image to static solid red
  // instead of the varying, flickering pumpkin-orange every other pixel
  // gets - i.e. every pixel maps to a shade of orange/brown, none held
  // out as literal red.
  uint16_t tint(uint16_t src, bool preserveRed = true) const {
    if (src == 0) {
      return RGB565_BLACK;
    }
    uint8_t r8 = ((src >> 11) & 0x1F) << 3;
    uint8_t g8 = ((src >> 5) & 0x3F) << 2;
    uint8_t b8 = (src & 0x1F) << 3;
    bool isRed = preserveRed && r8 > 100 && r8 > (uint16_t)g8 * 2 &&
                r8 > (uint16_t)b8 * 2;
    if (isRed) {
      return RGB565_RED;
    }
    constexpr uint8_t kDimFloor = 60; // out of 255 - thin "wall" still
                                      // glows a little rather than going
                                      // fully black
    uint8_t brightness = r8 > g8 ? r8 : g8;
    if (b8 > brightness) {
      brightness = b8;
    }
    if (brightness < kDimFloor) {
      brightness = kDimFloor;
    }
    uint16_t candle = color();
    uint8_t candleR5 = (candle >> 11) & 0x1F;
    uint8_t candleG6 = (candle >> 5) & 0x3F;
    uint8_t outR5 = (uint16_t)candleR5 * brightness / 255;
    uint8_t outG6 = (uint16_t)candleG6 * brightness / 255;
    return (outR5 << 11) | (outG6 << 5);
  }

private:
  float _phase = 0.0f;
  float _illumination = 0.82f;
};
