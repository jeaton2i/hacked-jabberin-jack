#include "robot_look_face.h"

#include "../assets/image_robot.h"

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr int16_t kStripHeight = 16;
} // namespace

void RobotLookFace::begin(Arduino_GFX *gfx) {
  _motion.reset();

  // Draw the full static image once, with the smile baked in over the
  // original flat mouth bar - neither ever changes color again, so draw()
  // below only ever has to touch the small area around the eyes.
  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      for (int16_t x = 0; x < kPanelWidth; x++) {
        RobotOverride ov = classifyRobotMouthPixel(x, y);
        uint16_t color;
        if (ov == RobotOverride::kBlack) {
          color = RGB565_BLACK;
        } else if (ov == RobotOverride::kWhite) {
          color = RGB565_WHITE;
        } else {
          color = image_robot[(int32_t)y * kPanelWidth + x];
        }
        stripBuffer[sy * kPanelWidth + x] = color;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}

void RobotLookFace::update() { _motion.update(); }

void RobotLookFace::draw(Arduino_GFX *gfx) {
  int16_t boxX0 = RobotGazeMotion::boxX0();
  int16_t boxX1 = RobotGazeMotion::boxX1();
  int16_t boxY0 = RobotGazeMotion::boxY0();
  int16_t boxY1 = RobotGazeMotion::boxY1();
  int16_t boxWidth = boxX1 - boxX0 + 1;
  int16_t offsetX = _motion.offsetX();
  int16_t offsetY = _motion.offsetY();

  // Only the box both eyes could possibly reach gets redrawn/re-sent each
  // frame - everywhere else is still showing the untouched pixels from
  // begin()'s one-time full draw (see EyeLookFace, same trick).
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
        RobotOverride ov = classifyRobotEyePixel(x, y, offsetX, offsetY);
        uint16_t color;
        if (ov == RobotOverride::kWhite) {
          color = RGB565_WHITE;
        } else if (ov == RobotOverride::kBlack) {
          color = RGB565_BLACK;
        } else {
          color = image_robot[(int32_t)y * kPanelWidth + x];
        }
        dstRow[x - boxX0] = color;
      }
    }
    gfx->draw16bitRGBBitmap(boxX0, rowOffset, stripBuffer, boxWidth,
                            stripHeight);
  }
}
