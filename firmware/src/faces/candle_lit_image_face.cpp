#include "candle_lit_image_face.h"

namespace {
constexpr int16_t kStripHeight = 16;
} // namespace

void CandleLitImageFace::begin(Arduino_GFX *gfx) {}

void CandleLitImageFace::update() { _flicker.update(); }

void CandleLitImageFace::draw(Arduino_GFX *gfx) {
  int16_t x0 = (gfx->width() - _width) / 2;
  int16_t y0 = (gfx->height() - _height) / 2;

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
        stripBuffer[sy * _width + x] = _flicker.tint(src, _preserveRed);
      }
    }
    gfx->draw16bitRGBBitmap(x0, y0 + rowOffset, stripBuffer, _width,
                            stripHeight);
  }
}
