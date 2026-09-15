#pragma once

#include <Arduino_GFX_Library.h>

// Common interface for anything drawn full-screen: test patterns, static
// faces, animated faces, and alternative face styles all implement this so
// main.cpp can swap between them without caring which one it has.
class Face {
public:
  virtual ~Face() = default;
  virtual void begin(Arduino_GFX *gfx) = 0;
  virtual void update() = 0; // advance any animation state
  virtual void draw(Arduino_GFX *gfx) = 0;
};
