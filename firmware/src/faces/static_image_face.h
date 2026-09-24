#pragma once

#include "face.h"

// Blits a pre-converted, full-panel-sized RGB565 bitmap once and leaves it
// on screen. See tools/convert_image_to_rgb565.py for how the source images
// are turned into the arrays this takes.
class StaticImageFace : public Face {
public:
  StaticImageFace(const uint16_t *image, int16_t width, int16_t height)
      : _image(image), _width(width), _height(height) {}

  void begin(Arduino_GFX *gfx) override;
  void update() override {}
  void draw(Arduino_GFX *gfx) override {}

private:
  const uint16_t *_image;
  int16_t _width;
  int16_t _height;
};
