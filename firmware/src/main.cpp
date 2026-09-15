#include <Arduino.h>

#include "display/display.h"
#include "faces/test_pattern_face.h"
#include "faces/triangle_face.h"

Display display;

// Swap this to try the next initial goal, e.g. `TriangleFace face;`.
TestPatternFace face;

void setup() {
  display.begin();
  face.begin(display.gfx());
}

void loop() {
  face.update();
  face.draw(display.gfx());
  delay(16); // ~60 fps target
}
