#include "candle_lit_robot_look_face.h"

#include "../assets/image_robot.h"

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr int16_t kStripHeight = 16;
} // namespace

void CandleLitRobotLookFace::begin(Arduino_GFX *gfx) { _motion.reset(); }

void CandleLitRobotLookFace::update() {
  _motion.update();
  _flicker.update();
}

void CandleLitRobotLookFace::draw(Arduino_GFX *gfx) {
  int16_t boxX0 = RobotGazeMotion::boxX0();
  int16_t boxX1 = RobotGazeMotion::boxX1();
  int16_t boxY0 = RobotGazeMotion::boxY0();
  int16_t boxY1 = RobotGazeMotion::boxY1();
  int16_t offsetX = _motion.offsetX();
  int16_t offsetY = _motion.offsetY();

  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      bool rowInEyeBox = y >= boxY0 && y <= boxY1;
      for (int16_t x = 0; x < kPanelWidth; x++) {
        RobotOverride ov = RobotOverride::kNone;
        if (rowInEyeBox && x >= boxX0 && x <= boxX1) {
          ov = classifyRobotEyePixel(x, y, offsetX, offsetY);
        }
        if (ov == RobotOverride::kNone) {
          ov = classifyRobotMouthPixel(x, y);
        }
        uint16_t raw;
        if (ov == RobotOverride::kWhite) {
          raw = RGB565_WHITE;
        } else if (ov == RobotOverride::kBlack) {
          raw = RGB565_BLACK;
        } else {
          raw = image_robot[(int32_t)y * kPanelWidth + x];
        }
        stripBuffer[sy * kPanelWidth + x] = _flicker.tint(raw);
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}
