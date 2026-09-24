#include "candle_lit_eye_look_face.h"

#include "../assets/image_eyeball.h"

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr int16_t kStripHeight = 16;
} // namespace

void CandleLitEyeLookFace::begin(Arduino_GFX *gfx) { _motion.reset(); }

void CandleLitEyeLookFace::update() {
  _motion.update();
  _flicker.update();
}

void CandleLitEyeLookFace::draw(Arduino_GFX *gfx) {
  int16_t boxX0 = EyeLookMotion::boxX0();
  int16_t boxX1 = EyeLookMotion::boxX1();
  int16_t boxY0 = EyeLookMotion::boxY0();
  int16_t boxY1 = EyeLookMotion::boxY1();

  // The flicker retints every pixel every frame, so unlike EyeLookFace the
  // full panel has to be re-sent regardless; the box below only saves the
  // (relatively expensive) iris-distance test outside the area the iris
  // could ever reach - everywhere else is straight to the source pixel.
  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      bool rowInBox = y >= boxY0 && y <= boxY1;
      for (int16_t x = 0; x < kPanelWidth; x++) {
        uint16_t raw;
        if (rowInBox && x >= boxX0 && x <= boxX1) {
          raw = _motion.sample(x, y);
        } else {
          raw = image_eyeball[(int32_t)y * kPanelWidth + x];
        }
        stripBuffer[sy * kPanelWidth + x] = _flicker.tint(raw);
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}
