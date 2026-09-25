#pragma once

#include "candle_flicker.h"
#include "face.h"
#include "robot_face_motion.h"

// RobotLookFace's animated eyes/smile, candle-lit like CandleLitImageFace:
// every pixel is retinted by the current flicker color each frame, so
// (unlike RobotLookFace) the whole panel has to be redrawn every frame
// regardless of where the eyes currently are - the flicker itself is
// constantly changing everywhere, not just around the eyes.
class CandleLitRobotLookFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  RobotGazeMotion _motion;
  CandleFlicker _flicker;
};
