#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "assets/image_commodore.h"
#include "assets/image_eyeball.h"
#include "assets/image_jack_skellington.h"
#include "assets/image_robot.h"
#include "assets/image_skull.h"
#include "assets/image_wpi_goat.h"
#include "assets/image_wpi_goat_head.h"
#include "display/display.h"
#include "faces/bullseye_face.h"
#include "faces/candle_flicker.h"
#include "faces/candle_lit_eye_look_face.h"
#include "faces/candle_lit_image_face.h"
#include "faces/checkerboard_face.h"
#include "faces/eye_look_face.h"
#include "faces/eye_look_motion.h"
#include "faces/face.h"
#include "faces/pacman_face.h"
#include "faces/static_image_face.h"
#include "faces/test_pattern_face.h"
#include "faces/text_face.h"
#include "faces/triangle_face.h"
#include "faces/tropical_face.h"
#include "fonts/FreeMono8pt7b.h"
#include "fonts/FreeSansBold10pt7b.h"
#include "fonts/FreeSerifBoldItalic12pt7b.h"
#include "prefs.h"

namespace {
// Any unused GPIO works here; GP0-12 are all taken by the display bus (see
// docs/rp2040-display-pinout.md). Button is active-low, wired to GND.
constexpr int8_t PIN_NEXT_BUTTON = 13;
constexpr unsigned long kButtonDebounceMs = 200;
constexpr unsigned long kDefaultAutoRotateMs = 6000;
constexpr unsigned long kDefaultBrightnessPercent = 115;
} // namespace

Display display;

TriangleFace triangleFace;
TestPatternFace testPatternFace;
CheckerboardFace checkerboardFace;
BullseyeFace bullseyeFace;
StaticImageFace jackSkellingtonFace(image_jack_skellington,
                                    image_jack_skellington_width,
                                    image_jack_skellington_height);
StaticImageFace skullFace(image_skull, image_skull_width, image_skull_height);
StaticImageFace commodoreFace(image_commodore, image_commodore_width,
                              image_commodore_height);
StaticImageFace wpiGoatFace(image_wpi_goat, image_wpi_goat_width,
                            image_wpi_goat_height);
StaticImageFace wpiGoatHeadFace(image_wpi_goat_head, image_wpi_goat_head_width,
                                image_wpi_goat_head_height);
StaticImageFace robotFace(image_robot, image_robot_width, image_robot_height);
StaticImageFace eyeballFace(image_eyeball, image_eyeball_width,
                            image_eyeball_height);
TextFace happyHalloweenFace("Happy", "Halloween");
TextFace booFace("Boo!");
TextFace configurableTextFace("Set my text!");
PacManFace pacManFace;
TropicalFace tropicalFace;
EyeLookFace eyeLookFace;

CandleLitImageFace jackSkellingtonCandleFace(image_jack_skellington,
                                             image_jack_skellington_width,
                                             image_jack_skellington_height);
CandleLitImageFace skullCandleFace(image_skull, image_skull_width,
                                   image_skull_height);
// preserveRed=false: these logos are mostly red/crimson themselves, so
// keeping literal red would leave most of the image a flat static red
// instead of a pumpkin-style orange/brown gradient (see CandleFlicker::
// tint()).
CandleLitImageFace commodoreCandleFace(image_commodore, image_commodore_width,
                                       image_commodore_height,
                                       /*preserveRed=*/false);
CandleLitImageFace wpiGoatCandleFace(image_wpi_goat, image_wpi_goat_width,
                                     image_wpi_goat_height,
                                     /*preserveRed=*/false);
CandleLitImageFace wpiGoatHeadCandleFace(image_wpi_goat_head,
                                         image_wpi_goat_head_width,
                                         image_wpi_goat_head_height,
                                         /*preserveRed=*/false);
CandleLitImageFace robotCandleFace(image_robot, image_robot_width,
                                   image_robot_height);
CandleLitImageFace eyeballCandleFace(image_eyeball, image_eyeball_width,
                                     image_eyeball_height);
CandleLitEyeLookFace eyeLookCandleFace;

Face *faces[] = {&triangleFace,
                 &testPatternFace,
                 &checkerboardFace,
                 &bullseyeFace,
                 &pacManFace,
                 &tropicalFace,
                 &jackSkellingtonFace,
                 &skullFace,
                 &commodoreFace,
                 &wpiGoatFace,
                 &wpiGoatHeadFace,
                 &robotFace,
                 &eyeballFace,
                 &eyeLookFace,
                 &jackSkellingtonCandleFace,
                 &skullCandleFace,
                 &commodoreCandleFace,
                 &wpiGoatCandleFace,
                 &wpiGoatHeadCandleFace,
                 &robotCandleFace,
                 &eyeballCandleFace,
                 &eyeLookCandleFace,
                 &happyHalloweenFace,
                 &booFace,
                 &configurableTextFace};
const char *faceNames[] = {"TriangleFace",
                           "TestPatternFace",
                           "Checkerboard",
                           "Bullseye",
                           "PacMan",
                           "Tropical",
                           "JackSkellington",
                           "Skull",
                           "Commodore",
                           "WpiGoat",
                           "WpiGoatHeadOnly",
                           "Robot",
                           "Eyeball",
                           "EyeballLookAround",
                           "JackSkellingtonCandleLit",
                           "SkullCandleLit",
                           "CommodoreCandleLit",
                           "WpiGoatCandleLit",
                           "WpiGoatHeadCandleLit",
                           "RobotCandleLit",
                           "EyeballCandleLit",
                           "EyeballLookAroundCandleLit",
                           "HappyHalloweenText",
                           "BooText",
                           "ConfigurableText"};
constexpr size_t kFaceCount = sizeof(faces) / sizeof(faces[0]);
// enabledMask packs one bit per face; a wider mask type or a bitset would be
// needed past 32 faces.
static_assert(kFaceCount <= 32, "faceEnabled no longer fits a uint32_t mask");

// Test pattern is noisy right now (see project notes) - off by default,
// but stays in the rotation/menu so it's a one-command toggle to check.
const bool kDefaultFaceEnabled[kFaceCount] = {
    true,  false, true, true, true, true, true, true,  true, true, true,
    true,  true,  true, true, true, true, true, true,  true, true, true,
    true,  true,  true};
bool faceEnabled[kFaceCount];

// Fonts ConfigurableTextFace can be switched between over serial (see the
// "font" command). The plain entries get TextFace's nearest-neighbor zoom,
// which is what gives the chunky pixelated look; the "-smooth" entries
// bilinearly resample instead for anti-aliased edges - same letterforms,
// different texture.
struct FontOption {
  const char *name;
  const GFXfont *font;
  bool smooth;
};
const FontOption kFontOptions[] = {
    {"sans", &FreeSansBold10pt7b, false}, // default
    {"sans-smooth", &FreeSansBold10pt7b, true},
    {"mono", &FreeMono8pt7b, false},
    {"mono-smooth", &FreeMono8pt7b, true},
    {"serif", &FreeSerifBoldItalic12pt7b, false},
    {"serif-smooth", &FreeSerifBoldItalic12pt7b, true},
};
constexpr size_t kFontOptionCount =
    sizeof(kFontOptions) / sizeof(kFontOptions[0]);
size_t currentFontIndex = 0; // index into kFontOptions; matches the ctor
                             // default TextFace uses (FreeSansBold10pt7b)

size_t currentFace = 0;
unsigned long autoRotateMs = kDefaultAutoRotateMs;

unsigned long lastButtonChangeMs = 0;
unsigned long lastAutoRotateMs = 0;
int lastButtonState = HIGH;

// Long enough for "text " plus two full TextFace::kMaxLineLength lines and
// the "|" that separates them.
char serialLine[104];
uint8_t serialLineLength = 0;

uint32_t currentEnabledMask() {
  uint32_t mask = 0;
  for (size_t i = 0; i < kFaceCount; i++) {
    if (faceEnabled[i]) {
      mask |= (1UL << i);
    }
  }
  return mask;
}

void applyEnabledMask(uint32_t mask) {
  for (size_t i = 0; i < kFaceCount; i++) {
    faceEnabled[i] = (mask & (1UL << i)) != 0;
  }
}

void resetToDefaults() {
  memcpy(faceEnabled, kDefaultFaceEnabled, sizeof(faceEnabled));
  autoRotateMs = kDefaultAutoRotateMs;
  CandleFlicker::setBrightness(kDefaultBrightnessPercent / 100.0f);
}

void selectFace(size_t index) {
  currentFace = index;
  // Always clear the whole panel between faces. Faces only redraw their
  // own shapes each frame (not a full clear, to avoid flicker), so
  // anything left over from the previous face would otherwise persist
  // wherever the new face doesn't happen to draw.
  display.gfx()->fillScreen(RGB565_BLACK);
  faces[currentFace]->begin(display.gfx());
  Serial.print("Displaying face: ");
  Serial.println(faceNames[currentFace]);
}

// Advances to the next *enabled* face, wrapping around. Also used by
// auto-rotate, which is why it resets the auto-rotate clock itself -
// manual and automatic advances both re-arm the same 20s timer.
void advanceFace() {
  size_t next = currentFace;
  for (size_t i = 0; i < kFaceCount; i++) {
    next = (next + 1) % kFaceCount;
    if (faceEnabled[next]) {
      break;
    }
  }
  selectFace(next);
  lastAutoRotateMs = millis();
}

void printFaceList() {
  for (size_t i = 0; i < kFaceCount; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(faceNames[i]);
    Serial.println(faceEnabled[i] ? " [on]" : " [off]");
  }
  Serial.print("Rotate interval: ");
  if (autoRotateMs == 0) {
    Serial.println("off");
  } else {
    Serial.print(autoRotateMs);
    Serial.println(" ms");
  }
}

void printHelp() {
  Serial.println("Serial commands:");
  Serial.println("  <enter>     - advance to next enabled face");
  Serial.println("  list        - list faces with on/off state + rotate interval");
  Serial.println("  <n>         - toggle face n on/off");
  Serial.println("  text <l1>[|l2|l3|l4] - set ConfigurableText (up to 4 lines) and show it");
  Serial.println("  font [name] - list/set ConfigurableText's font (see 'font' with no name)");
  Serial.println("  rotate <ms> - set auto-rotate interval (0 disables)");
  Serial.println("  brightness [percent] - show/set candle brightness (100=original)");
  Serial.println("  save        - save current faces + rotate interval + brightness to flash");
  Serial.println("  load        - reload saved config from flash");
  Serial.println("  reset       - restore compiled-in defaults (not saved)");
  Serial.println("  debug       - toggle EyeLook motion logging (off by default)");
  Serial.println("  help        - show this message");
}

void printFontList(size_t currentFontIndex) {
  Serial.println("Available fonts:");
  for (size_t i = 0; i < kFontOptionCount; i++) {
    Serial.print("  ");
    Serial.print(kFontOptions[i].name);
    Serial.println(i == currentFontIndex ? " (current)" : "");
  }
}

// Skips leading spaces, returning a pointer into `line`.
const char *skipSpaces(const char *line) {
  while (*line == ' ') {
    line++;
  }
  return line;
}

// Serial UI: an empty line (just press enter) or any unrecognized input
// advances to the next enabled face (keeps the old "mash a key"
// convenience); "list"/"help" print info; a bare number toggles that face's
// on/off state; "rotate", "brightness", "save", "load", and "reset" manage
// persisted config; "text" sets ConfigurableTextFace's message; "font"
// lists/sets its font; "debug" toggles EyeLookMotion's diagnostic logging
// (see printHelp for details).
void handleSerialCommand(const char *line) {
  if (line[0] == '\0') {
    advanceFace();
    return;
  }
  if (strcmp(line, "list") == 0) {
    printFaceList();
    return;
  }
  if (strcmp(line, "help") == 0) {
    printHelp();
    return;
  }
  if (strcmp(line, "save") == 0) {
    Prefs prefs{currentEnabledMask(), autoRotateMs,
               (uint32_t)(CandleFlicker::brightness() * 100.0f)};
    savePrefs(prefs);
    Serial.println("Saved current faces + rotate interval + brightness to flash");
    return;
  }
  if (strcmp(line, "load") == 0) {
    Prefs prefs;
    if (loadPrefs(prefs)) {
      applyEnabledMask(prefs.enabledMask);
      autoRotateMs = prefs.rotateMs;
      CandleFlicker::setBrightness(prefs.brightnessPercent / 100.0f);
      Serial.println("Loaded saved config from flash");
    } else {
      Serial.println("No valid saved config in flash");
    }
    return;
  }
  if (strcmp(line, "reset") == 0) {
    resetToDefaults();
    Serial.println("Restored compiled-in defaults (not saved)");
    return;
  }
  if (strcmp(line, "debug") == 0) {
    bool enabled = !EyeLookMotion::debugLogging();
    EyeLookMotion::setDebugLogging(enabled);
    Serial.println(enabled ? "EyeLook motion logging: on"
                           : "EyeLook motion logging: off");
    return;
  }
  if (strncmp(line, "brightness", 10) == 0 &&
      (line[10] == '\0' || line[10] == ' ')) {
    const char *arg = skipSpaces(line + 10);
    if (arg[0] == '\0') {
      Serial.print("Brightness: ");
      Serial.print((int)(CandleFlicker::brightness() * 100.0f + 0.5f));
      Serial.println("%");
      return;
    }
    char *end;
    long percent = strtol(arg, &end, 10);
    if (end != arg && *end == '\0' && percent >= 0) {
      CandleFlicker::setBrightness(percent / 100.0f);
      Serial.print("Brightness set to ");
      Serial.print(percent);
      Serial.println("%");
    } else {
      Serial.println("Usage: brightness <percent>");
    }
    return;
  }
  if (strncmp(line, "text", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
    const char *arg = skipSpaces(line + 4);
    if (arg[0] == '\0') {
      Serial.println("Usage: text <line1>[|line2[|line3[|line4]]]");
      return;
    }
    // Copied out (rather than split in place) since `line`/`arg` alias
    // serialLine, and setText()'s arguments all need to stay valid for
    // the duration of the call.
    char buf[sizeof(serialLine)];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    const char *lines[TextFace::kMaxLines] = {nullptr, nullptr, nullptr,
                                              nullptr};
    char *cursor = buf;
    uint8_t lineCount = 0;
    while (lineCount < TextFace::kMaxLines) {
      lines[lineCount++] = cursor;
      char *pipe = strchr(cursor, '|');
      if (!pipe || lineCount == TextFace::kMaxLines) {
        break;
      }
      *pipe = '\0';
      cursor = pipe + 1;
    }

    configurableTextFace.setText(lines[0], lines[1], lines[2], lines[3]);
    // Jump straight to it so the new text is immediately visible, rather
    // than leaving it to show up whenever auto-rotate/advance next
    // happens to reach it.
    selectFace(kFaceCount - 1);
    Serial.print("ConfigurableText set to: ");
    for (uint8_t i = 0; i < lineCount; i++) {
      if (i > 0) {
        Serial.print(" / ");
      }
      Serial.print(lines[i]);
    }
    Serial.println();
    return;
  }
  if (strncmp(line, "font", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
    const char *arg = skipSpaces(line + 4);
    if (arg[0] == '\0') {
      printFontList(currentFontIndex);
      return;
    }
    for (size_t i = 0; i < kFontOptionCount; i++) {
      if (strcmp(arg, kFontOptions[i].name) == 0) {
        currentFontIndex = i;
        configurableTextFace.setFont(kFontOptions[i].font,
                                     kFontOptions[i].smooth);
        selectFace(kFaceCount - 1);
        Serial.print("ConfigurableText font set to: ");
        Serial.println(kFontOptions[i].name);
        return;
      }
    }
    Serial.print("Unknown font: ");
    Serial.println(arg);
    printFontList(currentFontIndex);
    return;
  }
  if (strncmp(line, "rotate", 6) == 0 && (line[6] == '\0' || line[6] == ' ')) {
    const char *arg = skipSpaces(line + 6);
    char *end;
    long ms = strtol(arg, &end, 10);
    if (end != arg && *end == '\0' && ms >= 0) {
      autoRotateMs = (unsigned long)ms;
      lastAutoRotateMs = millis();
      Serial.print("Rotate interval set to ");
      if (autoRotateMs == 0) {
        Serial.println("off");
      } else {
        Serial.print(autoRotateMs);
        Serial.println(" ms");
      }
    } else {
      Serial.println("Usage: rotate <ms>");
    }
    return;
  }
  char *end;
  long index = strtol(line, &end, 10);
  if (end != line && *end == '\0' && index >= 0 &&
      (size_t)index < kFaceCount) {
    faceEnabled[index] = !faceEnabled[index];
    Serial.print(faceNames[index]);
    Serial.println(faceEnabled[index] ? ": enabled" : ": disabled");
    return;
  }
  advanceFace();
}

void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      serialLine[serialLineLength] = '\0';
      handleSerialCommand(serialLine);
      serialLineLength = 0;
    } else if (serialLineLength < sizeof(serialLine) - 1) {
      serialLine[serialLineLength++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Jabberin' Jack firmware starting");

  resetToDefaults();
  Prefs prefs;
  if (loadPrefs(prefs)) {
    applyEnabledMask(prefs.enabledMask);
    autoRotateMs = prefs.rotateMs;
    CandleFlicker::setBrightness(prefs.brightnessPercent / 100.0f);
    Serial.println("Loaded saved config from flash");
  } else {
    Serial.println("No saved config in flash; using compiled-in defaults");
  }
  printHelp();

  pinMode(PIN_NEXT_BUTTON, INPUT_PULLUP);

  display.begin();
  Serial.println("Display initialized");
  selectFace(0);
  lastAutoRotateMs = millis();
}

void loop() {
  pollSerial();

  int buttonState = digitalRead(PIN_NEXT_BUTTON);
  unsigned long now = millis();
  if (buttonState == LOW && lastButtonState == HIGH &&
      now - lastButtonChangeMs > kButtonDebounceMs) {
    advanceFace();
    lastButtonChangeMs = now;
  }
  lastButtonState = buttonState;

  if (autoRotateMs > 0 && now - lastAutoRotateMs >= autoRotateMs) {
    advanceFace();
  }

  faces[currentFace]->update();
  faces[currentFace]->draw(display.gfx());

  delay(16); // ~60 fps target
}
