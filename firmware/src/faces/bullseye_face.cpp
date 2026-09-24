#include "bullseye_face.h"

#include <math.h>

namespace {
constexpr int16_t kRingWidth = 10;
constexpr int16_t kStripHeight = 16;
} // namespace

void BullseyeFace::begin(Arduino_GFX *gfx) {
  int16_t w = gfx->width();
  int16_t h = gfx->height();
  float cx = w / 2.0f;
  float cy = h / 2.0f;

  // Composed into a RAM buffer and blitted through draw16bitRGBBitmap
  // (see CheckerboardFace - the same reliable path, not ~40000
  // individual pixel/rect draws).
  static uint16_t stripBuffer[220 * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < h; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > h) {
      stripHeight = h - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      bool edgeRow = (y == 0 || y == h - 1);
      for (int16_t x = 0; x < w; x++) {
        uint16_t color;
        if (edgeRow || x == 0 || x == w - 1) {
          color = RGB565_RED;
        } else {
          float dx = x - cx;
          float dy = y - cy;
          float dist = sqrtf(dx * dx + dy * dy);
          int16_t ring = (int16_t)(dist / kRingWidth);
          color = (ring % 2 == 0) ? RGB565_WHITE : RGB565_BLACK;
        }
        stripBuffer[sy * w + x] = color;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, w, stripHeight);
  }
}
