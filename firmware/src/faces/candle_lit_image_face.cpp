#include "candle_lit_image_face.h"

namespace {
constexpr int16_t kStripHeight = 16;
constexpr uint8_t kDimFloor = 60; // out of 255 - thin "wall" still glows a
                                  // little rather than going fully black
} // namespace

void CandleLitImageFace::begin(Arduino_GFX *gfx) {}

void CandleLitImageFace::update() { _flicker.update(); }

void CandleLitImageFace::draw(Arduino_GFX *gfx) {
  uint16_t color = _flicker.color();
  uint8_t candleR5 = (color >> 11) & 0x1F;
  uint8_t candleG6 = (color >> 5) & 0x3F;
  int16_t x0 = (gfx->width() - _width) / 2;
  int16_t y0 = (gfx->height() - _height) / 2;

  // Depth-carved look: instead of a flat silhouette, each pixel's own
  // brightness in the source image controls how much candle light comes
  // through, like variable pumpkin wall thickness - bright/white areas
  // are cut all the way through (full flicker color), darker but
  // non-black areas are left thicker (dim glow), true black is untouched
  // background.
  static uint16_t stripBuffer[220 * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < _height; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > _height) {
      stripHeight = _height - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      for (int16_t x = 0; x < _width; x++) {
        uint16_t src = _image[(int32_t)y * _width + x];
        uint16_t out = RGB565_BLACK;
        if (src != 0) {
          uint8_t r8 = ((src >> 11) & 0x1F) << 3;
          uint8_t g8 = ((src >> 5) & 0x3F) << 2;
          uint8_t b8 = (src & 0x1F) << 3;
          // Red source pixels (eyes, accents) stay a distinct solid red
          // instead of being folded into the same orange as everything
          // else - otherwise they wash out and disappear into the body.
          bool isRed = r8 > 100 && r8 > (uint16_t)g8 * 2 &&
                      r8 > (uint16_t)b8 * 2;
          if (isRed) {
            out = RGB565_RED;
          } else {
            uint8_t brightness = r8 > g8 ? r8 : g8;
            if (b8 > brightness) {
              brightness = b8;
            }
            if (brightness < kDimFloor) {
              brightness = kDimFloor;
            }
            uint8_t outR5 = (uint16_t)candleR5 * brightness / 255;
            uint8_t outG6 = (uint16_t)candleG6 * brightness / 255;
            out = (outR5 << 11) | (outG6 << 5);
          }
        }
        stripBuffer[sy * _width + x] = out;
      }
    }
    gfx->draw16bitRGBBitmap(x0, y0 + rowOffset, stripBuffer, _width,
                            stripHeight);
  }
}
