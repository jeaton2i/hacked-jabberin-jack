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
  // preserveRed: see CandleFlicker::tint(). Pass false for source art
  // that's mostly red itself (a crimson school-color logo, say), so it
  // reads as a pumpkin-carved orange/brown gradient instead of mostly
  // flat static red.
  CandleLitImageFace(const uint16_t *image, int16_t width, int16_t height,
                     bool preserveRed = true)
      : _image(image), _width(width), _height(height),
        _preserveRed(preserveRed) {}

  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

private:
  const uint16_t *_image;
  int16_t _width;
  int16_t _height;
  bool _preserveRed;
  CandleFlicker _flicker;
};
