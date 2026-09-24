#pragma once

#include "candle_flicker.h"
#include "face.h"

// Renders one or two lines of text, automatically sized so every corner of
// the rendered text stays within targetRadius pixels of the panel's
// center (see docs/rp2040-display-pinout.md and the bullseye test pattern
// for why - only a roughly circular area near the center is actually
// visible through the pumpkin's cutout). Mirrored to render correctly on
// this panel, and recolored every frame with the same candle flicker used
// elsewhere.
class TextFace : public Face {
public:
  static constexpr size_t kMaxLineLength = 48;

  explicit TextFace(const char *line1, const char *line2 = nullptr,
                    int16_t targetRadius = 75);
  ~TextFace() override;
  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  // Replaces the displayed text and re-renders immediately (used by
  // main.cpp's "text" serial command for ConfigurableTextFace). Safe to
  // call before begin() too - the new text just takes effect once begin()
  // does run. line2 may be null/empty for a single centered line.
  void setText(const char *line1, const char *line2 = nullptr);

  // Swaps the font and re-renders immediately (used by main.cpp's "font"
  // serial command). Safe to call before begin() too, same as setText().
  void setFont(const GFXfont *font);

private:
  void render();

  char _line1[kMaxLineLength];
  char _line2[kMaxLineLength];
  bool _hasLine2 = false;
  int16_t _targetRadius;
  const GFXfont *_font;
  Arduino_GFX *_gfx = nullptr;
  CandleFlicker _flicker;
  uint8_t *_renderedMask = nullptr; // 1 bit/pixel - see text_face.cpp
  int16_t _width = 0;
  int16_t _height = 0;
};
