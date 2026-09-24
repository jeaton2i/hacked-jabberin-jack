#include "eye_look_face.h"

#include <string.h>

#include "../assets/image_eyeball.h"

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr int16_t kStripHeight = 16;
} // namespace

void EyeLookFace::begin(Arduino_GFX *gfx) {
  _motion.reset();

  // Draw the full static image once - the sclera/veins never change again,
  // so draw() below only ever has to touch the small area around the iris
  // instead of re-sending the whole panel every frame.
  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    memcpy(stripBuffer, image_eyeball + (int32_t)rowOffset * kPanelWidth,
          (size_t)stripHeight * kPanelWidth * sizeof(uint16_t));
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}

void EyeLookFace::update() { _motion.update(); }

void EyeLookFace::draw(Arduino_GFX *gfx) {
  int16_t boxX0 = EyeLookMotion::boxX0();
  int16_t boxX1 = EyeLookMotion::boxX1();
  int16_t boxY0 = EyeLookMotion::boxY0();
  int16_t boxY1 = EyeLookMotion::boxY1();
  int16_t boxWidth = boxX1 - boxX0 + 1;

  // Only the region the iris could possibly reach gets redrawn/re-sent each
  // frame - everywhere else is still showing the untouched pixels from
  // begin()'s one-time full draw. Smaller, less frequent bus transfers also
  // means less time each frame where a slow/partial write could be visibly
  // caught mid-update.
  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = boxY0; rowOffset <= boxY1;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight - 1 > boxY1) {
      stripHeight = boxY1 - rowOffset + 1;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      uint16_t *dstRow = stripBuffer + (int32_t)sy * boxWidth;
      for (int16_t x = boxX0; x <= boxX1; x++) {
        dstRow[x - boxX0] = _motion.sample(x, y);
      }
    }
    gfx->draw16bitRGBBitmap(boxX0, rowOffset, stripBuffer, boxWidth,
                            stripHeight);
  }
}
