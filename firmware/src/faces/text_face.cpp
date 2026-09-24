#include "text_face.h"

#include <math.h>
#include <string.h>

#include "fonts/FreeSansBold10pt7b.h"

namespace {
constexpr int16_t kLineSpacing = 26;
constexpr int16_t kStripHeight = 16;

// The rendered text is cached as a 1-bit-per-pixel "is this ink" mask
// rather than a full 16bpp panel-sized buffer, since two TextFace
// instances each holding a full-color copy would burn ~150KB of RAM
// between them for no benefit - all that's needed per pixel is on/off.
bool maskGet(const uint8_t *mask, int32_t index) {
  return (mask[index >> 3] >> (index & 7)) & 1;
}

void maskSet(uint8_t *mask, int32_t index) {
  mask[index >> 3] |= (1 << (index & 7));
}
} // namespace

TextFace::TextFace(const char *line1, const char *line2, int16_t targetRadius)
    : _line1(line1), _line2(line2), _targetRadius(targetRadius) {}

TextFace::~TextFace() { delete[] _renderedMask; }

void TextFace::begin(Arduino_GFX *gfx) {
  _width = gfx->width();
  _height = gfx->height();
  bool twoLines = _line2 != nullptr;

  // Rendered once (not per-frame): render at native font size onto an
  // off-screen canvas, measure the real ink bounding boxes, then compute
  // the largest zoom that keeps every corner within _targetRadius of
  // center. The zoomed+mirrored result is cached as a non-zero-means-ink
  // mask (see draw()) so per-frame flicker only has to recolor + blit, not
  // re-render text or recompute the fit.
  Arduino_Canvas canvas(_width, _height, gfx);
  canvas.begin(GFX_SKIP_OUTPUT_BEGIN);
  canvas.fillScreen(RGB565_BLACK);
  canvas.setFont(&FreeSansBold10pt7b);
  canvas.setTextWrap(false);
  canvas.setTextColor(RGB565_WHITE); // placeholder; draw() recolors it

  int16_t bx, by1 = 0, by2 = 0;
  uint16_t w1, h1, w2 = 0, h2 = 0;
  canvas.getTextBounds(_line1, 0, 0, &bx, &by1, &w1, &h1);
  if (twoLines) {
    canvas.getTextBounds(_line2, 0, 0, &bx, &by2, &w2, &h2);
  }

  int16_t halfSpacing = twoLines ? kLineSpacing / 2 : 0;
  int16_t baseline1 = _height / 2 - halfSpacing;
  int16_t baseline2 = _height / 2 + halfSpacing;

  canvas.setCursor((_width - w1) / 2, baseline1);
  canvas.print(_line1);
  if (twoLines) {
    canvas.setCursor((_width - w2) / 2, baseline2);
    canvas.print(_line2);
  }

  float maxNativeRadius = 1.0f; // avoid div-by-zero
  auto considerLine = [&](uint16_t w, uint16_t h, int16_t by,
                          int16_t baseline) {
    float halfW = w / 2.0f;
    float top = baseline + by - _height / 2.0f;
    float bottom = top + h;
    float rTop = sqrtf(halfW * halfW + top * top);
    float rBottom = sqrtf(halfW * halfW + bottom * bottom);
    maxNativeRadius = fmaxf(maxNativeRadius, fmaxf(rTop, rBottom));
  };
  considerLine(w1, h1, by1, baseline1);
  if (twoLines) {
    considerLine(w2, h2, by2, baseline2);
  }
  float zoom = _targetRadius / maxNativeRadius;

  uint16_t *source = canvas.getFramebuffer();
  int32_t maskBytes = ((int32_t)_width * _height + 7) / 8;
  if (!_renderedMask) {
    _renderedMask = new uint8_t[maskBytes];
  }
  memset(_renderedMask, 0, maskBytes);
  int16_t cx = _width / 2;
  int16_t cy = _height / 2;
  for (int16_t y = 0; y < _height; y++) {
    int16_t srcY = cy + (int16_t)((y - cy) / zoom);
    for (int16_t x = 0; x < _width; x++) {
      int16_t srcX = cx + (int16_t)((x - cx) / zoom);
      int16_t mirroredSrcX = _width - 1 - srcX; // see StaticImageFace
      if (mirroredSrcX >= 0 && mirroredSrcX < _width && srcY >= 0 &&
          srcY < _height &&
          source[(int32_t)srcY * _width + mirroredSrcX] != 0) {
        maskSet(_renderedMask, (int32_t)y * _width + x);
      }
    }
  }
}

void TextFace::update() { _flicker.update(); }

void TextFace::draw(Arduino_GFX *gfx) {
  if (!_renderedMask) {
    return;
  }
  uint16_t color = _flicker.color();

  static uint16_t stripBuffer[220 * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < _height; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > _height) {
      stripHeight = _height - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      for (int16_t x = 0; x < _width; x++) {
        bool ink = maskGet(_renderedMask, (int32_t)y * _width + x);
        stripBuffer[sy * _width + x] = ink ? color : RGB565_BLACK;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, _width, stripHeight);
  }
}
