#include "text_face.h"

#include <math.h>
#include <string.h>

#include "fonts/FreeSansBold10pt7b.h"

namespace {
constexpr int16_t kStripHeight = 16;

// The pixelated path caches a 1-bit-per-pixel "is this ink" mask rather
// than a full 16bpp panel-sized buffer, since two TextFace instances each
// holding a full-color copy would burn ~150KB of RAM between them for no
// benefit - all that's needed per pixel is on/off. The smooth path needs
// a full 8-bit alpha per pixel to actually be smooth, so it costs more
// RAM - see _alphaMask's comment in text_face.h for why that's only ever
// allocated for an instance that actually uses it.
bool maskGet(const uint8_t *mask, int32_t index) {
  return (mask[index >> 3] >> (index & 7)) & 1;
}

void maskSet(uint8_t *mask, int32_t index) {
  mask[index >> 3] |= (1 << (index & 7));
}

uint16_t scaleColor(uint16_t color, uint8_t alpha) {
  if (alpha == 255) {
    return color;
  }
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (uint16_t)r * alpha / 255;
  g = (uint16_t)g * alpha / 255;
  b = (uint16_t)b * alpha / 255;
  return (r << 11) | (g << 5) | b;
}
} // namespace

TextFace::TextFace(const char *line1, const char *line2, const char *line3,
                   const char *line4, int16_t targetRadius)
    : _targetRadius(targetRadius), _font(&FreeSansBold10pt7b) {
  setText(line1, line2, line3, line4);
}

TextFace::~TextFace() {
  delete[] _renderedMask;
  delete[] _alphaMask;
}

void TextFace::setText(const char *line1, const char *line2,
                       const char *line3, const char *line4) {
  const char *provided[kMaxLines] = {line1, line2, line3, line4};
  _lineCount = 0;
  for (uint8_t i = 0; i < kMaxLines; i++) {
    if (provided[i] && provided[i][0] != '\0') {
      strncpy(_lines[_lineCount], provided[i], kMaxLineLength - 1);
      _lines[_lineCount][kMaxLineLength - 1] = '\0';
      _lineCount++;
    }
  }
  if (_lineCount == 0) {
    _lines[0][0] = '\0';
    _lineCount = 1;
  }
  if (_gfx) {
    render();
  }
}

void TextFace::setFont(const GFXfont *font, bool smooth) {
  _font = font;
  _smooth = smooth;
  // Only one of these is ever in use at a time (see draw()), but until now
  // both stayed permanently allocated forever the first time each was ever
  // touched, wasting whichever one the current font style isn't using -
  // the alpha mask alone is already ~38KB (one byte/pixel, versus the
  // pixelated mask's one bit/pixel) on top of everything else already
  // resident. Free the one this switch just made irrelevant.
  if (_smooth && _renderedMask) {
    delete[] _renderedMask;
    _renderedMask = nullptr;
  } else if (!_smooth && _alphaMask) {
    delete[] _alphaMask;
    _alphaMask = nullptr;
  }
  if (_gfx) {
    render();
  }
}

void TextFace::begin(Arduino_GFX *gfx) {
  _gfx = gfx;
  render();
}

void TextFace::render() {
  _width = _gfx->width();
  _height = _gfx->height();

  // Rendered once per setText()/setFont()/begin() call (not per-frame):
  // render at native font size onto a temporary off-screen canvas, measure
  // the real ink bounding boxes, then compute the largest zoom that keeps
  // every corner within _targetRadius of center. The zoomed+mirrored
  // result is cached (see draw()) so per-frame flicker only has to
  // recolor + blit, not re-render text or recompute the fit.
  //
  // Using Arduino_Canvas_Mono (1 bit/pixel, ~5KB at 220x176) rather than
  // Arduino_Canvas (16bpp, ~76KB) for this temporary canvas - text is only
  // ever drawn pure white on black here anyway, so the color canvas was
  // pure waste. That ~76KB, transient as it was, was still enough to
  // crowd out the smooth path's ~38KB alpha mask on the RP2040's ~260KB
  // SRAM (most already spoken for by static globals) and reliably crash
  // the board; even after that got fixed by copying into a compact mask
  // before freeing the color canvas, the *color* canvas's own transient
  // ~76KB was still large enough to occasionally fail elsewhere once
  // several TextFace instances' masks were resident at once - which
  // showed up as silently-blank/stale faces rather than a crash, since
  // failed allocations bail out gracefully instead of rendering. Skipping
  // the color canvas entirely removes that pressure at its source rather
  // than just handling its failure better.
  int32_t nativeMaskBytes = ((int32_t)_width * _height + 7) / 8;
  uint8_t *nativeInk = new (std::nothrow) uint8_t[nativeMaskBytes];
  if (!nativeInk) {
    return;
  }
  memset(nativeInk, 0, nativeMaskBytes);

  float zoom;
  {
    Arduino_Canvas_Mono canvas(_width, _height, _gfx);
    if (!canvas.begin(GFX_SKIP_OUTPUT_BEGIN)) {
      // Framebuffer malloc failed. Bail out rather than read/write through
      // the null framebuffer below - whatever was already on screen just
      // stays put instead of updating.
      delete[] nativeInk;
      return;
    }
    canvas.fillScreen(RGB565_BLACK);
    canvas.setFont(_font);
    canvas.setTextWrap(false);
    canvas.setTextColor(RGB565_WHITE); // placeholder; draw() recolors it

    int16_t bx;
    int16_t by[kMaxLines];
    uint16_t w[kMaxLines], h[kMaxLines];
    for (uint8_t i = 0; i < _lineCount; i++) {
      canvas.getTextBounds(_lines[i], 0, 0, &bx, &by[i], &w[i], &h[i]);
    }

    // Each font bakes in its own natural line-to-line distance (yAdvance) -
    // using a single fixed spacing for every font left a small/tight font
    // like the mono one looking oddly loose (its yAdvance is little more
    // than half the sans/serif fonts', but all three used to get the same
    // gap regardless).
    int16_t lineSpacing = (int16_t)pgm_read_byte(&_font->yAdvance);
    int16_t totalSpan = (int16_t)(_lineCount - 1) * lineSpacing;
    int16_t firstBaseline = _height / 2 - totalSpan / 2;
    int16_t baseline[kMaxLines];
    for (uint8_t i = 0; i < _lineCount; i++) {
      baseline[i] = firstBaseline + i * lineSpacing;
      canvas.setCursor((_width - w[i]) / 2, baseline[i]);
      canvas.print(_lines[i]);
    }

    float maxNativeRadius = 1.0f; // avoid div-by-zero
    for (uint8_t i = 0; i < _lineCount; i++) {
      float halfW = w[i] / 2.0f;
      float top = baseline[i] + by[i] - _height / 2.0f;
      float bottom = top + h[i];
      float rTop = sqrtf(halfW * halfW + top * top);
      float rBottom = sqrtf(halfW * halfW + bottom * bottom);
      maxNativeRadius = fmaxf(maxNativeRadius, fmaxf(rTop, rBottom));
    }
    zoom = _targetRadius / maxNativeRadius;

    // Arduino_Canvas_Mono packs its own framebuffer 1 bit/pixel too, but
    // row-padded to a byte boundary (see its writePixelPreclipped()) -
    // slightly different layout from nativeInk's flat packing below, so
    // copy bit-by-bit via its own indexing rather than assuming they line
    // up.
    uint8_t *monoSource = canvas.getFramebuffer();
    int16_t monoRowBytes = (_width + 7) / 8;
    for (int16_t y = 0; y < _height; y++) {
      for (int16_t x = 0; x < _width; x++) {
        int32_t monoPos = (int32_t)y * monoRowBytes + (x / 8);
        if (monoSource[monoPos] & (0x80 >> (x & 7))) {
          maskSet(nativeInk, (int32_t)y * _width + x);
        }
      }
    }
  } // canvas destructor frees its framebuffer here

  int16_t cx = _width / 2;
  int16_t cy = _height / 2;

  if (_smooth) {
    // Bilinearly resample the (hard-edged, binary) native rendering
    // instead of rounding to the nearest source pixel - that's what
    // turns a blocky nearest-neighbor zoom into smooth, anti-aliased
    // edges. Stored as a 0-255 alpha per pixel rather than 1 bit.
    int32_t maskBytes = (int32_t)_width * _height;
    if (!_alphaMask) {
      _alphaMask = new (std::nothrow) uint8_t[maskBytes];
      if (!_alphaMask) {
        delete[] nativeInk;
        return; // leave whatever was already rendered/on-screen alone
      }
    }
    for (int16_t y = 0; y < _height; y++) {
      float srcYf = cy + (y - cy) / zoom;
      for (int16_t x = 0; x < _width; x++) {
        float srcXf = cx + (x - cx) / zoom;
        float mirroredSrcXf = (_width - 1) - srcXf; // see StaticImageFace

        int16_t x0 = (int16_t)floorf(mirroredSrcXf);
        int16_t y0 = (int16_t)floorf(srcYf);
        float tx = mirroredSrcXf - x0;
        float ty = srcYf - y0;
        auto at = [&](int16_t xx, int16_t yy) -> float {
          if (xx < 0 || xx >= _width || yy < 0 || yy >= _height) {
            return 0.0f;
          }
          return maskGet(nativeInk, (int32_t)yy * _width + xx) ? 255.0f
                                                               : 0.0f;
        };
        float v00 = at(x0, y0), v10 = at(x0 + 1, y0);
        float v01 = at(x0, y0 + 1), v11 = at(x0 + 1, y0 + 1);
        float top = v00 + (v10 - v00) * tx;
        float bot = v01 + (v11 - v01) * tx;
        _alphaMask[(int32_t)y * _width + x] =
            (uint8_t)(top + (bot - top) * ty);
      }
    }
  } else {
    int32_t maskBytes = ((int32_t)_width * _height + 7) / 8;
    if (!_renderedMask) {
      _renderedMask = new (std::nothrow) uint8_t[maskBytes];
      if (!_renderedMask) {
        delete[] nativeInk;
        return;
      }
    }
    memset(_renderedMask, 0, maskBytes);
    for (int16_t y = 0; y < _height; y++) {
      int16_t srcY = cy + (int16_t)((y - cy) / zoom);
      for (int16_t x = 0; x < _width; x++) {
        int16_t srcX = cx + (int16_t)((x - cx) / zoom);
        int16_t mirroredSrcX = _width - 1 - srcX; // see StaticImageFace
        if (mirroredSrcX >= 0 && mirroredSrcX < _width && srcY >= 0 &&
            srcY < _height &&
            maskGet(nativeInk, (int32_t)srcY * _width + mirroredSrcX)) {
          maskSet(_renderedMask, (int32_t)y * _width + x);
        }
      }
    }
  }
  delete[] nativeInk;
}

void TextFace::update() { _flicker.update(); }

void TextFace::draw(Arduino_GFX *gfx) {
  if (!_renderedMask && !_alphaMask) {
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
        uint16_t out;
        if (_smooth) {
          uint8_t alpha = _alphaMask[(int32_t)y * _width + x];
          out = alpha == 0 ? RGB565_BLACK : scaleColor(color, alpha);
        } else {
          bool ink = maskGet(_renderedMask, (int32_t)y * _width + x);
          out = ink ? color : RGB565_BLACK;
        }
        stripBuffer[sy * _width + x] = out;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, _width, stripHeight);
  }
}
