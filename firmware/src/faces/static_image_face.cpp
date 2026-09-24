#include "static_image_face.h"

#include <string.h>

namespace {
// draw16bitRGBBitmap has two overloads: a `const uint16_t*` one (for
// flash-resident data, ours) and a plain `uint16_t*` one. They take
// different code paths in the underlying driver, and only the non-const
// one reliably lands pixels on this panel (matching what already works
// for the mouth buffer in TriangleFace). So each strip gets copied into
// RAM first to force the working path.
constexpr int16_t kStripHeight = 16;
uint16_t stripBuffer[220 * kStripHeight];
} // namespace

void StaticImageFace::begin(Arduino_GFX *gfx) {
  int16_t x = (gfx->width() - _width) / 2;
  int16_t y = (gfx->height() - _height) / 2;

  for (int16_t rowOffset = 0; rowOffset < _height; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > _height) {
      stripHeight = _height - rowOffset;
    }
    memcpy(stripBuffer, _image + rowOffset * _width,
          stripHeight * _width * sizeof(uint16_t));
    gfx->draw16bitRGBBitmap(x, y + rowOffset, stripBuffer, _width,
                            stripHeight);
  }
}
