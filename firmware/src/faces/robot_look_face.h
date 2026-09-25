#pragma once

#include "face.h"
#include "robot_face_motion.h"

// image_robot with animated "look around" eyes and a smile in place of the
// flat mouth bar. Like EyeLookFace, the smile and the rest of the image
// never change color after begin() draws them once, so draw() only has to
// redraw the small box the eyes move within instead of the whole panel.
class RobotLookFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  RobotGazeMotion _motion;
};
