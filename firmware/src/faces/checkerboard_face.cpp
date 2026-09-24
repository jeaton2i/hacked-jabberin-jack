#include "checkerboard_face.h"

namespace {
constexpr int16_t kCellSize = 20;
constexpr int16_t kStripHeight = 16;
} // namespace

void CheckerboardFace::begin(Arduino_GFX *gfx) {
  int16_t w = gfx->width();
  int16_t h = gfx->height();

  // Composed into a RAM buffer and blitted through the same
  // draw16bitRGBBitmap strip path StaticImageFace and TextFace use,
  // rather than ~100 individual fillRect calls - that many small
  // direct-to-panel draws left stray leftover pixels from whatever face
  // was on screen before this one.
  static uint16_t stripBuffer[220 * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < h; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > h) {
      stripHeight = h - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      bool onBorder = (y == 0 || y == h - 1);
      for (int16_t x = 0; x < w; x++) {
        uint16_t color;
        if (onBorder || x == 0 || x == w - 1) {
          color = RGB565_RED;
        } else {
          bool isWhite = ((x / kCellSize) + (y / kCellSize)) % 2 == 0;
          color = isWhite ? RGB565_WHITE : RGB565_BLACK;
        }
        stripBuffer[sy * w + x] = color;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, w, stripHeight);
  }
}
