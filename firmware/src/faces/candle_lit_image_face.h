#pragma once

#include "candle_flicker.h"
#include "face.h"

// Recolors any non-black pixel of a pre-converted image with the current
// candle flicker color each frame, instead of the image's original
// colors - looks lit/carved from behind, like TriangleFace's pumpkin,
// applied to arbitrary bitmap content. Reuses the same image arrays
// StaticImageFace draws verbatim.
class CandleLitImageFace : public Face {
public:
  CandleLitImageFace(const uint16_t *image, int16_t width, int16_t height)
      : _image(image), _width(width), _height(height) {}

  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  const uint16_t *_image;
  int16_t _width;
  int16_t _height;
  CandleFlicker _flicker;
};
