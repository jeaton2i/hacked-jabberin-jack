#pragma once

#include "eye_look_motion.h"
#include "face.h"

// Eyeball image whose iris/pupil disc darts around within the sclera to
// look around - see EyeLookMotion for the actual motion/compositing logic,
// shared with CandleLitEyeLookFace. The sclera/veins here are truly static
// (no candle flicker), so after the first full-panel draw in begin(), each
// frame only needs to touch the small region the iris could possibly reach.
class EyeLookFace : public Face {
public:
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  EyeLookMotion _motion;
};
