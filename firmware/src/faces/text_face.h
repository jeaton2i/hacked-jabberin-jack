#pragma once

#include "candle_flicker.h"
#include "face.h"

// Renders up to kMaxLines lines of text, automatically sized so every
// corner of the rendered text stays within targetRadius pixels of the
// panel's center (see docs/rp2040-display-pinout.md and the bullseye test
// pattern for why - only a roughly circular area near the center is
// actually visible through the pumpkin's cutout). Mirrored to render
// correctly on this panel, and recolored every frame with the same candle
// flicker used elsewhere.
class TextFace : public Face {
public:
  static constexpr size_t kMaxLineLength = 48;
  static constexpr uint8_t kMaxLines = 4;

  explicit TextFace(const char *line1, const char *line2 = nullptr,
                    const char *line3 = nullptr, const char *line4 = nullptr,
                    int16_t targetRadius = 75);
  ~TextFace() override;
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  // Replaces the displayed text (up to kMaxLines lines; null/empty
  // trailing arguments are simply dropped) and re-renders immediately
  // (used by main.cpp's "text" serial command for ConfigurableTextFace).
  // Safe to call before begin() too - the new text just takes effect once
  // begin() does run.
  void setText(const char *line1, const char *line2 = nullptr,
              const char *line3 = nullptr, const char *line4 = nullptr);

  // Swaps the font and pixelated-vs-smooth rendering style, and re-renders
  // immediately (used by main.cpp's "font" serial command). Safe to call
  // before begin() too, same as setText().
  void setFont(const GFXfont *font, bool smooth);

private:
  void render();

  char _lines[kMaxLines][kMaxLineLength];
  uint8_t _lineCount = 1;
  int16_t _targetRadius;
  const GFXfont *_font;
  bool _smooth = false;
  Arduino_GFX *_gfx = nullptr;
  CandleFlicker _flicker;
  uint8_t *_renderedMask = nullptr; // 1 bit/pixel - pixelated path
  uint8_t *_alphaMask = nullptr;    // 1 byte/pixel - smooth path; lazily
                                    // allocated only if a smooth font is
                                    // actually ever selected
  int16_t _width = 0;
  int16_t _height = 0;
};
