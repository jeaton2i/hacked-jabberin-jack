#include <Arduino.h>

#include "display/display.h"
#include "faces/test_pattern_face.h"
#include "faces/triangle_face.h"

Display display;

// Swap this back to `TestPatternFace face;` when checking display wiring.
TriangleFace face;
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Jabberin' Jack firmware starting");

  display.begin();
  Serial.println("Display initialized");
  face.begin(display.gfx());
  Serial.println("Face initialized: TriangleFace");
}

void loop() {
  face.update();
  face.draw(display.gfx());

  unsigned long now = millis();
  if (now - lastHeartbeat >= 1000) {
    lastHeartbeat = now;
    Serial.println("Heartbeat: main loop running");
  }

  delay(16); // ~60 fps target
}
