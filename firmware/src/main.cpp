#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/image_commodore.h"
#include "assets/image_eyeball.h"
#include "assets/image_jack_skellington.h"
#include "assets/image_robot.h"
#include "assets/image_skull.h"
#include "assets/image_wpi_goat.h"
#include "assets/image_wpi_goat_head.h"
#include "assets/test_word.h"
#include "audio/i2s_player.h"
#include "display/display.h"
#include "faces/bullseye_face.h"
#include "faces/candle_flicker.h"
#include "faces/candle_lit_eye_look_face.h"
#include "faces/candle_lit_image_face.h"
#include "faces/checkerboard_face.h"
#include "faces/clock_face.h"
#include "faces/countdown_face.h"
#include "faces/eye_look_face.h"
#include "faces/eye_look_motion.h"
#include "faces/face.h"
#include "faces/candle_lit_robot_look_face.h"
#include "faces/pacman_face.h"
#include "faces/robot_look_face.h"
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
// docs/rp2040-display-pinout.md). All three buttons are active-low, wired
// to GND.
constexpr int8_t PIN_NEXT_BUTTON = 16;
constexpr int8_t PIN_PAUSE_BUTTON = 17;
constexpr int8_t PIN_ORDER_BUTTON = 18;
constexpr unsigned long kButtonDebounceMs = 200;

// UART1 link to an optional ESP32 Wi-Fi bridge board (see
// docs/esp32-network-bridge.md for wiring + the bridge's own firmware).
// Left unconnected, this is just an idle UART - nothing reads it unless a
// bridge is actually wired up. The actual pins used are runtime-configurable
// (see esp32TxPin/esp32RxPin and the "esp32link" command below) - these are
// only the compiled-in defaults, used until "load" restores a saved config.
constexpr int8_t kDefaultPinEsp32Tx = 20; // -> ESP32 RX
constexpr int8_t kDefaultPinEsp32Rx = 21; // <- ESP32 TX

// The RP2040's UART1 peripheral can only be mapped to specific GPIOs: the
// SoC's fixed pin-function mux offers it in groups of 4 consecutive GPIOs
// (TX, RX, CTS, RTS in that order) - {4,5,6,7}, {12,13,14,15},
// {20,21,22,23}, {28,29,...} - so only the first two pins of each group
// are ever valid TX/RX choices; e.g. GP22 is UART1's CTS pin, not a second
// RX option, and GP27 belongs to UART0, not UART1, at all. Given GP0-15
// are taken by the display bus and GP16-18 by the buttons, GP20/GP21 (the
// defaults) end up as the only genuinely free legal pair on this board -
// GP28/29 would also be legal, but GP29 is conventionally reserved for
// VSYS sensing on official Pico boards. Getting this wrong doesn't just
// fail quietly: requesting an invalid UART1 pin from the SDK reliably
// hard-faults the whole chip before USB even finishes enumerating.
bool isValidUart1Pins(int8_t tx, int8_t rx) {
  constexpr int8_t kValidTx[] = {4, 12, 20, 28};
  for (int8_t validTx : kValidTx) {
    if (tx == validTx && rx == validTx + 1) {
      return true;
    }
  }
  return false;
}

// Ambient WS2812 strip, powered from its own switched 5V rail (not the
// Pico's) - only the data line comes from a GPIO. Adjust to match however
// many LEDs are actually wired up.
constexpr int8_t PIN_LED_DATA = 15;
constexpr uint16_t kLedCount = 12;
constexpr unsigned long kDefaultAutoRotateMs = 6000;
constexpr unsigned long kDefaultBrightnessPercent = 115;
} // namespace

Display display;

// Drives the ambient LED strip independently of whichever face is showing
// (plenty of faces - Checkerboard, PacMan, the plain StaticImageFaces -
// aren't candle-lit at all) so the strip always breathes, matching
// whatever the on-screen candle-lit faces look like when one is active.
// Shares CandleFlicker's static brightness, so the brightness button
// affects the strip too; ticks at the same 0.09f/update() rate every other
// CandleFlicker instance does, so it stays phase-synced with any
// currently-active candle-lit face's own flicker instead of drifting.
CandleFlicker ledFlicker;
Adafruit_NeoPixel leds(kLedCount, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);

TriangleFace triangleFace;
TriangleFace triangleFaceAnimated(/*animated=*/true);
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
// Transient overlay for button feedback (see showStatusMessage) - never
// part of the normal rotation, so it's always left disabled in
// kDefaultFaceEnabled and jumped to directly by index instead.
TextFace statusMessageFace("");
PacManFace pacManFace;
TropicalFace tropicalFace;
EyeLookFace eyeLookFace;
RobotLookFace robotLookFace;
CountdownFace countdownFace;
ClockFace clockFace;

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
CandleLitRobotLookFace robotLookCandleFace;

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
                 &robotLookFace,
                 &jackSkellingtonCandleFace,
                 &skullCandleFace,
                 &commodoreCandleFace,
                 &wpiGoatCandleFace,
                 &wpiGoatHeadCandleFace,
                 &robotCandleFace,
                 &eyeballCandleFace,
                 &eyeLookCandleFace,
                 &robotLookCandleFace,
                 &happyHalloweenFace,
                 &booFace,
                 // Placed here (not next to &triangleFace/&countdownFace's
                 // own natural spots above) so every other face's
                 // saved-config bit position stays unchanged - only
                 // configurableTextFace/statusMessageFace shift, and they
                 // recompute their own fixed index from kFaceCount so
                 // that's harmless. Same reasoning each time a face gets
                 // added from here on.
                 &triangleFaceAnimated,
                 &countdownFace,
                 &clockFace,
                 &configurableTextFace,
                 &statusMessageFace};
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
                           "RobotLookAround",
                           "JackSkellingtonCandleLit",
                           "SkullCandleLit",
                           "CommodoreCandleLit",
                           "WpiGoatCandleLit",
                           "WpiGoatHeadCandleLit",
                           "RobotCandleLit",
                           "EyeballCandleLit",
                           "EyeballLookAroundCandleLit",
                           "RobotLookAroundCandleLit",
                           "HappyHalloweenText",
                           "BooText",
                           "TriangleFaceAnimated",
                           "Countdown",
                           "Clock",
                           "ConfigurableText",
                           "StatusMessage"};
constexpr size_t kFaceCount = sizeof(faces) / sizeof(faces[0]);
// enabledMask packs one bit per face; a wider mask type or a bitset would be
// needed past 32 faces.
static_assert(kFaceCount <= 32, "faceEnabled no longer fits a uint32_t mask");
// Fixed indices for the faces jumped to directly by index rather than
// reached through the normal rotation (see the "text"/"font"/"countdown"
// commands and showStatusMessage).
constexpr size_t kCountdownFaceIndex = kFaceCount - 4;
constexpr size_t kClockFaceIndex = kFaceCount - 3;
constexpr size_t kConfigurableTextFaceIndex = kFaceCount - 2;
constexpr size_t kStatusMessageFaceIndex = kFaceCount - 1;

// Test pattern is noisy right now (see project notes) - off by default,
// but stays in the rotation/menu so it's a one-command toggle to check.
const bool kDefaultFaceEnabled[kFaceCount] = {
    true,  // TriangleFace
    false, // TestPatternFace
    false, // Checkerboard
    false, // Bullseye
    true,  // PacMan
    false, // Tropical
    false, // JackSkellington
    true,  // Skull
    false, // Commodore
    true,  // WpiGoat
    false, // WpiGoatHeadOnly
    false, // Robot
    false, // Eyeball
    true,  // EyeballLookAround
    true,  // RobotLookAround
    true,  // JackSkellingtonCandleLit
    false, // SkullCandleLit
    false, // CommodoreCandleLit
    false, // WpiGoatCandleLit
    true,  // WpiGoatHeadCandleLit
    false, // RobotCandleLit
    false, // EyeballCandleLit
    true,  // EyeballLookAroundCandleLit
    true,  // RobotLookAroundCandleLit
    true, // HappyHalloweenText
    true, // BooText
    false, // TriangleFaceAnimated - opt-in, so the mouth doesn't suddenly
           // start flapping in rotation until asked for
    false, // Countdown - opt-in, and useless in rotation until the ESP32
           // bridge has actually synced a date at least once anyway
    false, // Clock - opt-in, same reasoning as Countdown (useless until
           // the ESP32 bridge has synced a time at least once)
    true, // ConfigurableText
    false // StatusMessage
  };
bool faceEnabled[kFaceCount];

// Fonts ConfigurableTextFace can be switched between over serial (see the
// "font" command). The plain entries get TextFace's nearest-neighbor zoom,
// which is what gives the chunky pixelated look; TextFace also supports a
// "smooth" bilinearly-resampled style (see its setFont()) for anti-aliased
// edges - same letterforms, different texture. That style's own options
// are commented out below rather than removed: they looked fine on this
// pumpkin (see the memory fixes this took to get there), but are held back
// as unproven on the larger pumpkin until that gets its own pass, without
// losing the rendering support itself - just uncomment a line to bring
// one back.
struct FontOption {
  const char *name;
  const GFXfont *font;
  bool smooth;
};
const FontOption kFontOptions[] = {
    {"sans", &FreeSansBold10pt7b, false}, // default
    // {"sans-smooth", &FreeSansBold10pt7b, true},
    {"mono", &FreeMono8pt7b, false},
    // {"mono-smooth", &FreeMono8pt7b, true},
    {"serif", &FreeSerifBoldItalic12pt7b, false},
    // {"serif-smooth", &FreeSerifBoldItalic12pt7b, true},
};
constexpr size_t kFontOptionCount =
    sizeof(kFontOptions) / sizeof(kFontOptions[0]);
size_t currentFontIndex = 0; // index into kFontOptions; matches the ctor
                             // default TextFace uses (FreeSansBold10pt7b)

size_t currentFace = 0;
unsigned long autoRotateMs = kDefaultAutoRotateMs;
// Remembers the interval to resume at when the pause/play button turns
// rotation back on - otherwise resuming would forget a custom "rotate <ms>"
// interval and jump back to kDefaultAutoRotateMs instead.
unsigned long savedRotateMs = kDefaultAutoRotateMs;
// false = advance through enabled faces in list order (default); true =
// advance to a random enabled face each time.
bool randomOrder = false;

// Runtime-configurable ESP32 bridge link pins (see the "esp32link"
// command) - deliberately not compile-time constants, since getting these
// right in practice means matching whatever's actually been wired up,
// which is exactly the kind of thing worth changing without a reflash.
int8_t esp32TxPin = kDefaultPinEsp32Tx;
int8_t esp32RxPin = kDefaultPinEsp32Rx;

// Queues audio for the ESP32 bridge's own speaker to play (see the
// "audio esp-tone"/"audio esp-voice" commands and "audiotrigger" below).
// The RP2040 has no way to push data to the ESP32 unprompted without
// risking corrupting whatever it might currently be asking over the same
// link (e.g. a "status" reply mid-flight), so instead this just sets a
// flag the ESP32 polls for on its own schedule via "audiotrigger", which
// atomically reads and clears it - the same request/response shape as
// every other command here, just with the roles of "who wanted this"
// reversed.
enum class Esp32AudioTrigger { None, Tone, Voice };
Esp32AudioTrigger pendingEsp32Audio = Esp32AudioTrigger::None;

unsigned long lastAutoRotateMs = 0;

// How long a button-press status message (see showStatusMessage) stays on
// screen before the display returns to whatever face was showing before it.
constexpr unsigned long kStatusMessageMs = 1500;
// The face to return to once the current status message times out; only
// meaningful while statusMessageUntilMs != 0.
size_t faceBeforeStatusMessage = 0;
// 0 when no status message is showing; otherwise the millis() deadline to
// return to faceBeforeStatusMessage.
unsigned long statusMessageUntilMs = 0;

// Simple active-low debounce, same shape as the single-button logic this
// replaced: a press only re-arms kButtonDebounceMs after the *previous
// registered* press, not after every raw edge.
struct DebouncedButton {
  int8_t pin;
  int lastState = HIGH;
  unsigned long lastChangeMs = 0;
};
DebouncedButton nextButton{PIN_NEXT_BUTTON};
DebouncedButton pauseButton{PIN_PAUSE_BUTTON};
DebouncedButton orderButton{PIN_ORDER_BUTTON};

bool buttonPressed(DebouncedButton &button, unsigned long now) {
  int state = digitalRead(button.pin);
  bool pressed = state == LOW && button.lastState == HIGH &&
                now - button.lastChangeMs > kButtonDebounceMs;
  if (pressed) {
    button.lastChangeMs = now;
  }
  button.lastState = state;
  return pressed;
}

// Long enough for "text " plus all TextFace::kMaxLines lines at their full
// TextFace::kMaxLineLength, plus the "|" separators between them and a
// terminating '\0'. This used to only fit two lines' worth despite "text"
// documenting support for up to kMaxLines - anything longer than that got
// silently cut off mid-command (see pollSerialPort()'s bounds check
// below), which for a cut landing mid-line meant whatever partial bytes
// were left got parsed and displayed as garbage instead of just being
// truncated cleanly.
constexpr size_t kSerialLineCapacity =
    5 /* "text " */ +
    TextFace::kMaxLines * (TextFace::kMaxLineLength - 1) /* line contents */
    + (TextFace::kMaxLines - 1) /* "|" separators */ + 1 /* '\0' */;
struct SerialPort {
  Stream *stream;
  char line[kSerialLineCapacity];
  uint8_t length = 0;
};
SerialPort usbPort{&Serial};
SerialPort espPort{&Serial2};

// Whichever port's line is currently being handled - sets where
// handleSerialCommand()/printFaceList()/printHelp()/printFontList() send
// their replies, so a command from the ESP32 bridge gets its reply over
// UART1 instead of USB.
Stream *cmdOut = &Serial;

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

// kPrefsHolidayNameLength (prefs.h) is duplicated rather than shared with
// CountdownFace::kMaxHolidayNameLength (see prefs.h's comment on why) -
// this catches the two ever drifting apart, in the one file that already
// includes both headers.
static_assert(kPrefsHolidayNameLength == CountdownFace::kMaxHolidayNameLength,
             "prefs.h's holiday name buffer no longer matches "
             "CountdownFace's");
static_assert(kPrefsTextLineLength == TextFace::kMaxLineLength,
             "prefs.h's text line buffer no longer matches TextFace's");
static_assert(kPrefsTextLineCount == TextFace::kMaxLines,
             "prefs.h's text line count no longer matches TextFace's");

// Snapshots everything "save" and "esp32link" persist to flash from its
// current live state - shared so the two call sites can't drift apart as
// fields get added.
Prefs buildCurrentPrefs() {
  Prefs prefs;
  prefs.enabledMask = currentEnabledMask();
  prefs.rotateMs = autoRotateMs;
  prefs.brightnessPercent = (uint32_t)(CandleFlicker::brightness() * 100.0f);
  prefs.randomOrder = (uint32_t)(randomOrder ? 1 : 0);
  prefs.esp32TxPin = (uint32_t)esp32TxPin;
  prefs.esp32RxPin = (uint32_t)esp32RxPin;
  strncpy(prefs.holidayName, countdownFace.holidayName(),
         sizeof(prefs.holidayName) - 1);
  prefs.holidayName[sizeof(prefs.holidayName) - 1] = '\0';
  prefs.holidayMonth = countdownFace.holidayMonth();
  prefs.holidayDay = countdownFace.holidayDay();
  for (uint8_t i = 0; i < TextFace::kMaxLines; i++) {
    if (i < configurableTextFace.lineCount()) {
      strncpy(prefs.textLines[i], configurableTextFace.line(i),
             sizeof(prefs.textLines[i]) - 1);
      prefs.textLines[i][sizeof(prefs.textLines[i]) - 1] = '\0';
    } else {
      prefs.textLines[i][0] = '\0';
    }
  }
  prefs.textFontIndex = (uint32_t)currentFontIndex;
  prefs.clockUse12Hour = clockFace.use12Hour() ? 1 : 0;
  return prefs;
}

// Applies a loaded Prefs' ConfigurableText fields - shared by setup() and
// the "load" command so they can't drift apart. Doesn't call selectFace();
// unlike the "text"/"font" serial commands, loading saved config shouldn't
// jump the display to ConfigurableText on its own.
void applyTextPrefs(const Prefs &prefs) {
  const char *lines[TextFace::kMaxLines] = {nullptr, nullptr, nullptr,
                                            nullptr};
  for (uint8_t i = 0; i < TextFace::kMaxLines; i++) {
    if (prefs.textLines[i][0] != '\0') {
      lines[i] = prefs.textLines[i];
    }
  }
  configurableTextFace.setText(lines[0], lines[1], lines[2], lines[3]);

  if (prefs.textFontIndex < kFontOptionCount) {
    currentFontIndex = (size_t)prefs.textFontIndex;
    configurableTextFace.setFont(kFontOptions[currentFontIndex].font,
                                 kFontOptions[currentFontIndex].smooth);
  }
  clockFace.setUse12Hour(prefs.clockUse12Hour != 0);
}

// Initializes the ESP32 bridge UART using the current esp32TxPin/
// esp32RxPin. Called exactly once, from setup(), after esp32TxPin/RxPin
// have their final boot-time values (compiled-in defaults, then possibly
// overridden by loadPrefs()). Deliberately NOT called again afterward to
// apply a later change (see the "esp32link" command) - tearing down and
// restarting this UART mid-session (Serial2.end() + setTX/setRX + begin())
// reliably hangs the RP2040 on this core, for reasons not yet root-caused.
// Changing esp32TxPin/esp32RxPin at runtime therefore only takes effect
// after the next power cycle, not immediately.
void beginEsp32Link() {
  Serial2.setTX(esp32TxPin);
  Serial2.setRX(esp32RxPin);
  // arduino-pico's SerialUART defaults to a 32-byte RX FIFO (see its
  // _fifoSize) - too small for this link's own traffic: any single command
  // longer than that (a multi-line "text"/"countdown" easily is) arrives
  // faster than pollSerial() drains it between loop() iterations, so the
  // FIFO wraps and the tail of that command gets overwritten by whatever's
  // received next (observed as e.g. a "text" line's end getting spliced
  // with a later "audiotrigger" poll - not a race between those two
  // commands at all, just this buffer being too small for either alone).
  // Sized comfortably above kSerialLineCapacity, the longest single line
  // this link ever needs to carry intact.
  Serial2.setFIFOSize(256);
  Serial2.begin(115200);
}

void resetToDefaults() {
  memcpy(faceEnabled, kDefaultFaceEnabled, sizeof(faceEnabled));
  autoRotateMs = kDefaultAutoRotateMs;
  randomOrder = false;
  esp32TxPin = kDefaultPinEsp32Tx;
  esp32RxPin = kDefaultPinEsp32Rx;
  CandleFlicker::setBrightness(kDefaultBrightnessPercent / 100.0f);
  countdownFace.setHoliday("Halloween", 10, 31);
  currentFontIndex = 0;
  configurableTextFace.setText("Set my text!");
  configurableTextFace.setFont(kFontOptions[currentFontIndex].font,
                               kFontOptions[currentFontIndex].smooth);
  clockFace.setUse12Hour(false);
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

// Briefly overlays a button-press confirmation (e.g. "Rotate: On" / interval,
// "Face Order: Random") on statusMessageFace, then automatically restores
// whatever face was showing beforehand - see the loop() check on
// statusMessageUntilMs. Only remembers faceBeforeStatusMessage on the initial
// call (not on a second button press while a message is already showing), so
// a run of quick presses still restores the face from before the *first* one.
void showStatusMessage(const char *line1, const char *line2 = nullptr) {
  if (statusMessageUntilMs == 0) {
    faceBeforeStatusMessage = currentFace;
  }
  statusMessageFace.setText(line1, line2);
  selectFace(kStatusMessageFaceIndex);
  statusMessageUntilMs = millis() + kStatusMessageMs;
}

// Advances to another *enabled* face - the next one in list order, wrapping
// around, or (when randomOrder is set) a random enabled face other than the
// current one. Also used by auto-rotate, which is why it resets the
// auto-rotate clock itself - manual and automatic advances both re-arm the
// same timer.
void advanceFace() {
  size_t next = currentFace;
  if (randomOrder) {
    size_t choices = 0;
    for (size_t i = 0; i < kFaceCount; i++) {
      if (faceEnabled[i] && i != currentFace) {
        choices++;
      }
    }
    if (choices > 0) {
      size_t pick = random(choices);
      for (size_t i = 0; i < kFaceCount; i++) {
        if (faceEnabled[i] && i != currentFace) {
          if (pick == 0) {
            next = i;
            break;
          }
          pick--;
        }
      }
    }
  } else {
    for (size_t i = 0; i < kFaceCount; i++) {
      next = (next + 1) % kFaceCount;
      if (faceEnabled[next]) {
        break;
      }
    }
  }
  selectFace(next);
  lastAutoRotateMs = millis();
}

// Pauses auto-rotate if it's running, or resumes it at whatever interval
// was running before it was paused (see savedRotateMs).
void toggleRotation() {
  char intervalLine[24];
  if (autoRotateMs > 0) {
    savedRotateMs = autoRotateMs;
    autoRotateMs = 0;
    Serial.println("Auto-rotate: paused");
  } else {
    autoRotateMs = savedRotateMs;
    lastAutoRotateMs = millis();
    Serial.print("Auto-rotate: resumed at ");
    Serial.print(autoRotateMs);
    Serial.println(" ms");
  }
  snprintf(intervalLine, sizeof(intervalLine), "%lu ms", savedRotateMs);
  showStatusMessage(autoRotateMs > 0 ? "Rotate: On" : "Rotate: Paused",
                    intervalLine);
}

void toggleOrderMode() {
  randomOrder = !randomOrder;
  Serial.print("Face order: ");
  Serial.println(randomOrder ? "random" : "in-order");
  showStatusMessage("Face Order:", randomOrder ? "Random" : "In-Order");
}

// RGB565 -> the 8-bit-per-channel color NeoPixel wants. CandleFlicker's
// color is always blue=0, so this only ever produces warm orange tones.
void updateLeds() {
  ledFlicker.update();
  uint16_t rgb565 = ledFlicker.color();
  uint8_t r8 = ((rgb565 >> 11) & 0x1F) << 3;
  uint8_t g8 = ((rgb565 >> 5) & 0x3F) << 2;
  uint8_t b8 = (rgb565 & 0x1F) << 3;
  uint32_t color = leds.Color(r8, g8, b8);
  for (uint16_t i = 0; i < kLedCount; i++) {
    leds.setPixelColor(i, color);
  }
  leds.show();
}

void printFaceList() {
  for (size_t i = 0; i < kFaceCount; i++) {
    cmdOut->print(i);
    cmdOut->print(": ");
    cmdOut->print(faceNames[i]);
    cmdOut->println(faceEnabled[i] ? " [on]" : " [off]");
  }
  cmdOut->print("Rotate interval: ");
  if (autoRotateMs == 0) {
    cmdOut->println("off");
  } else {
    cmdOut->print(autoRotateMs);
    cmdOut->println(" ms");
  }
}

// Minimal JSON string escaping for embedding arbitrary user-entered text
// (ConfigurableText's lines) into printStatusJson()'s output - without
// this, a line containing '"' or '\' would produce invalid JSON.
void printJsonEscaped(const char *s) {
  for (const char *p = s; *p; p++) {
    switch (*p) {
    case '"':
      cmdOut->print("\\\"");
      break;
    case '\\':
      cmdOut->print("\\\\");
      break;
    case '\n':
      cmdOut->print("\\n");
      break;
    default:
      cmdOut->print(*p);
    }
  }
}

// Machine-readable snapshot of everything the "list"/"brightness"/"order"/
// "font" commands otherwise report only in human-oriented text - used by the
// optional ESP32 bridge (see docs/esp32-network-bridge.md) so it doesn't
// have to parse those instead. statusMessageFace is deliberately left out
// of "faces": it's an internal overlay, not something remote UIs should be
// able to toggle into the rotation.
void printStatusJson() {
  cmdOut->print("{\"currentFace\":");
  cmdOut->print(currentFace);
  cmdOut->print(",\"rotateMs\":");
  cmdOut->print(autoRotateMs);
  cmdOut->print(",\"brightnessPercent\":");
  cmdOut->print((int)(CandleFlicker::brightness() * 100.0f + 0.5f));
  cmdOut->print(",\"order\":\"");
  cmdOut->print(randomOrder ? "random" : "in-order");
  cmdOut->print("\",\"font\":\"");
  cmdOut->print(kFontOptions[currentFontIndex].name);
  cmdOut->print("\",\"debugLogging\":");
  cmdOut->print(EyeLookMotion::debugLogging() ? "true" : "false");
  cmdOut->print(",\"esp32TxPin\":");
  cmdOut->print(esp32TxPin);
  cmdOut->print(",\"esp32RxPin\":");
  cmdOut->print(esp32RxPin);
  cmdOut->print(",\"countdown\":{\"holidayName\":\"");
  cmdOut->print(countdownFace.holidayName());
  cmdOut->print("\",\"holidayMonth\":");
  cmdOut->print(countdownFace.holidayMonth());
  cmdOut->print(",\"holidayDay\":");
  cmdOut->print(countdownFace.holidayDay());
  cmdOut->print(",\"synced\":");
  cmdOut->print(countdownFace.synced() ? "true" : "false");
  cmdOut->print(",\"daysUntil\":");
  cmdOut->print(countdownFace.daysUntil());
  cmdOut->print("}");

  cmdOut->print(",\"clock\":{\"synced\":");
  cmdOut->print(clockFace.synced() ? "true" : "false");
  cmdOut->print("}");

  cmdOut->print(",\"fonts\":[");
  for (size_t i = 0; i < kFontOptionCount; i++) {
    if (i > 0) {
      cmdOut->print(",");
    }
    cmdOut->print("\"");
    cmdOut->print(kFontOptions[i].name);
    cmdOut->print("\"");
  }

  // ConfigurableText's current lines - lets remote UIs (and the "text"
  // command's own users) see what's actually stored/rendered, rather than
  // only ever being able to overwrite it blind.
  cmdOut->print("],\"text\":[");
  for (uint8_t i = 0; i < configurableTextFace.lineCount(); i++) {
    if (i > 0) {
      cmdOut->print(",");
    }
    cmdOut->print("\"");
    printJsonEscaped(configurableTextFace.line(i));
    cmdOut->print("\"");
  }

  cmdOut->print("],\"faces\":[");
  bool first = true;
  for (size_t i = 0; i < kFaceCount; i++) {
    if (i == kStatusMessageFaceIndex) {
      continue;
    }
    if (!first) {
      cmdOut->print(",");
    }
    first = false;
    cmdOut->print("{\"i\":");
    cmdOut->print(i);
    cmdOut->print(",\"name\":\"");
    cmdOut->print(faceNames[i]);
    cmdOut->print("\",\"enabled\":");
    cmdOut->print(faceEnabled[i] ? "true" : "false");
    cmdOut->print("}");
  }
  cmdOut->println("]}");
}

void printHelp() {
  cmdOut->println("Serial commands:");
  cmdOut->println("  <enter>     - advance to next enabled face");
  cmdOut->println("  list        - list faces with on/off state + rotate interval");
  cmdOut->println("  goto <n>    - jump directly to face n (index from 'list'), regardless of its on/off state");
  cmdOut->println("  status      - print machine-readable JSON status (see printStatusJson)");
  cmdOut->println("  <n>         - toggle face n on/off");
  cmdOut->println("  text <l1>[|l2|l3|l4] - set ConfigurableText (up to 4 lines) and show it");
  cmdOut->println("  font [name] - list/set ConfigurableText's font (see 'font' with no name)");
  cmdOut->println("  rotate <ms> - set auto-rotate interval (0 disables)");
  cmdOut->println("  brightness [percent] - show/set candle brightness (100=original)");
  cmdOut->println("  order [random|in-order] - show/set face advance order");
  cmdOut->println("  esp32link [tx rx] - show, or set + save + reboot to apply, the ESP32 bridge UART's GPIO pins");
  cmdOut->println("  countdown [<month> <day> <name>] - show, or set, the Countdown face's target date/holiday name (name may contain one '|' to split it across 2 lines)");
  cmdOut->println("  settime <year> <month> <day> - feed today's actual date to the Countdown face (meant for the ESP32 bridge's NTP sync)");
  cmdOut->println("  setclock <hour 0-23> <minute> <second> - feed the current wall-clock time to the Clock face (meant for the ESP32 bridge's NTP sync)");
  cmdOut->println("  clockformat [12|24] - show, or set, whether the Clock face displays 12- or 24-hour time (default 24)");
  cmdOut->println("  audio <tone|voice> - play the synthesized test tone, or the test speech clip, over this board's own I2S output");
  cmdOut->println("  audio <esp-tone|esp-voice> - queue the same for the ESP32 bridge's speaker instead (see \"audiotrigger\")");
  cmdOut->println("  audiotrigger - read + clear the pending ESP32 audio queue (polled by the bridge, not meant for humans)");
  cmdOut->println("  save        - save current faces + rotate interval + brightness + order + ESP32 link pins to flash");
  cmdOut->println("  load        - reload saved config from flash");
  cmdOut->println("  reset       - restore compiled-in defaults (not saved)");
  cmdOut->println("  debug       - toggle EyeLook motion logging (off by default)");
  cmdOut->println("  help        - show this message");
  cmdOut->println("Buttons: next (GP16), pause/play rotate (GP17), toggle random/in-order (GP18)");
}

void printFontList(size_t currentFontIndex) {
  cmdOut->println("Available fonts:");
  for (size_t i = 0; i < kFontOptionCount; i++) {
    cmdOut->print("  ");
    cmdOut->print(kFontOptions[i].name);
    cmdOut->println(i == currentFontIndex ? " (current)" : "");
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
// convenience); "list"/"help" print info; "goto" jumps straight to a face
// by index regardless of its enabled state; a bare number instead toggles
// that face's on/off state; "rotate", "brightness", "order", "esp32link",
// "save",
// "load", and "reset" manage persisted config; "text" sets
// ConfigurableTextFace's message; "font" lists/sets its font; "audio"
// plays a test tone or speech clip over this board's own I2S output, or
// queues one for the ESP32 bridge's speaker instead ("audiotrigger" is how
// the bridge polls for that); "debug" toggles EyeLookMotion's diagnostic
// logging (see printHelp for details).
void handleSerialCommand(const char *line) {
  if (line[0] == '\0') {
    advanceFace();
    return;
  }
  if (strcmp(line, "list") == 0) {
    printFaceList();
    return;
  }
  if (strncmp(line, "goto", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
    const char *arg = skipSpaces(line + 4);
    char *end;
    long index = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || index < 0 ||
        (size_t)index >= kFaceCount) {
      cmdOut->println("Usage: goto <n> (face index from 'list')");
      return;
    }
    // Unlike auto-rotate/advance, jumps directly regardless of the
    // target's enabled state - handy for looking at one specific face
    // without having to toggle everything else off first.
    selectFace((size_t)index);
    return;
  }
  if (strcmp(line, "status") == 0) {
    printStatusJson();
    return;
  }
  if (strcmp(line, "help") == 0) {
    printHelp();
    return;
  }
  if (strcmp(line, "save") == 0) {
    savePrefs(buildCurrentPrefs());
    cmdOut->println("Saved current faces + rotate interval + brightness + "
                    "order + ESP32 link pins + countdown holiday to flash");
    return;
  }
  if (strcmp(line, "load") == 0) {
    Prefs prefs;
    if (loadPrefs(prefs)) {
      applyEnabledMask(prefs.enabledMask);
      autoRotateMs = prefs.rotateMs;
      CandleFlicker::setBrightness(prefs.brightnessPercent / 100.0f);
      randomOrder = prefs.randomOrder != 0;
      countdownFace.setHoliday(prefs.holidayName, (uint8_t)prefs.holidayMonth,
                               (uint8_t)prefs.holidayDay);
      applyTextPrefs(prefs);
      if (isValidUart1Pins((int8_t)prefs.esp32TxPin,
                          (int8_t)prefs.esp32RxPin)) {
        esp32TxPin = (int8_t)prefs.esp32TxPin;
        esp32RxPin = (int8_t)prefs.esp32RxPin;
      } else {
        esp32TxPin = kDefaultPinEsp32Tx;
        esp32RxPin = kDefaultPinEsp32Rx;
        cmdOut->println("Saved ESP32 link pins aren't a valid UART1 pair - "
                        "ignoring, defaults staged instead");
      }
      cmdOut->println("Loaded saved config from flash (ESP32 link pins "
                      "take effect after the next power cycle)");
    } else {
      cmdOut->println("No valid saved config in flash");
    }
    return;
  }
  if (strcmp(line, "reset") == 0) {
    resetToDefaults();
    cmdOut->println("Restored compiled-in defaults (not saved)");
    return;
  }
  if (strcmp(line, "debug") == 0) {
    bool enabled = !EyeLookMotion::debugLogging();
    EyeLookMotion::setDebugLogging(enabled);
    cmdOut->println(enabled ? "EyeLook motion logging: on"
                           : "EyeLook motion logging: off");
    return;
  }
  if (strncmp(line, "brightness", 10) == 0 &&
      (line[10] == '\0' || line[10] == ' ')) {
    const char *arg = skipSpaces(line + 10);
    if (arg[0] == '\0') {
      cmdOut->print("Brightness: ");
      cmdOut->print((int)(CandleFlicker::brightness() * 100.0f + 0.5f));
      cmdOut->println("%");
      return;
    }
    char *end;
    long percent = strtol(arg, &end, 10);
    if (end != arg && *end == '\0' && percent >= 0) {
      CandleFlicker::setBrightness(percent / 100.0f);
      cmdOut->print("Brightness set to ");
      cmdOut->print(percent);
      cmdOut->println("%");
    } else {
      cmdOut->println("Usage: brightness <percent>");
    }
    return;
  }
  if (strncmp(line, "order", 5) == 0 && (line[5] == '\0' || line[5] == ' ')) {
    const char *arg = skipSpaces(line + 5);
    if (arg[0] == '\0') {
      cmdOut->print("Face order: ");
      cmdOut->println(randomOrder ? "random" : "in-order");
      return;
    }
    if (strcmp(arg, "random") == 0) {
      randomOrder = true;
    } else if (strcmp(arg, "in-order") == 0) {
      randomOrder = false;
    } else {
      cmdOut->println("Usage: order [random|in-order]");
      return;
    }
    cmdOut->print("Face order set to: ");
    cmdOut->println(randomOrder ? "random" : "in-order");
    return;
  }
  if (strncmp(line, "esp32link", 9) == 0 &&
      (line[9] == '\0' || line[9] == ' ')) {
    const char *arg = skipSpaces(line + 9);
    if (arg[0] == '\0') {
      cmdOut->print("ESP32 link pins: tx=");
      cmdOut->print(esp32TxPin);
      cmdOut->print(" rx=");
      cmdOut->println(esp32RxPin);
      return;
    }
    char *end;
    long tx = strtol(arg, &end, 10);
    const char *rxArg = (end == arg) ? end : skipSpaces(end);
    long rx = strtol(rxArg, &end, 10);
    if (end == rxArg || tx < 0 || tx > 28 || rx < 0 || rx > 28 ||
        !isValidUart1Pins((int8_t)tx, (int8_t)rx)) {
      cmdOut->println("Usage: esp32link <tx> <rx> - must be one of these "
                      "pairs (RP2040 UART1's only valid TX/RX pin "
                      "choices): 4 5, 12 13, 20 21, 28 29");
      return;
    }
    esp32TxPin = (int8_t)tx;
    esp32RxPin = (int8_t)rx;
    // Changing this UART's pins only actually takes effect on the next
    // beginEsp32Link() call, which only ever runs once at boot (see its own
    // comment) - so unlike every other setting here, this one is pointless
    // without also saving *and* rebooting, or the new pins would just be
    // discarded the moment anything else reboots the board.
    savePrefs(buildCurrentPrefs());
    cmdOut->print("ESP32 link pins set to: tx=");
    cmdOut->print(tx);
    cmdOut->print(" rx=");
    cmdOut->print(rx);
    cmdOut->println(" - saved current config and rebooting to apply...");
    cmdOut->flush();
    delay(50);
    rp2040.reboot();
    return;
  }
  if (strncmp(line, "countdown", 9) == 0 &&
      (line[9] == '\0' || line[9] == ' ')) {
    const char *arg = skipSpaces(line + 9);
    if (arg[0] == '\0') {
      cmdOut->print("Countdown target: ");
      cmdOut->print(countdownFace.holidayName());
      cmdOut->print(" (");
      cmdOut->print(countdownFace.holidayMonth());
      cmdOut->print("/");
      cmdOut->print(countdownFace.holidayDay());
      cmdOut->print("), ");
      cmdOut->println(countdownFace.synced()
                          ? "date synced"
                          : "not synced yet (needs the ESP32 bridge)");
      return;
    }
    char *end;
    long month = strtol(arg, &end, 10);
    const char *dayArg = (end == arg) ? end : skipSpaces(end);
    long day = strtol(dayArg, &end, 10);
    const char *name = (end == dayArg) ? end : skipSpaces(end);
    if (end == dayArg || name[0] == '\0' || month < 1 || month > 12 ||
        day < 1 || day > 31) {
      cmdOut->println("Usage: countdown <month> <day> <holiday name> "
                      "(name may contain one '|' to split it across 2 lines)");
      return;
    }
    countdownFace.setHoliday(name, (uint8_t)month, (uint8_t)day);
    // Jump straight to it, same as "text"/"font" do for ConfigurableText -
    // immediate visual feedback for what was just set.
    selectFace(kCountdownFaceIndex);
    cmdOut->print("Countdown target set to: ");
    cmdOut->print(countdownFace.holidayName());
    cmdOut->print(" (");
    cmdOut->print(month);
    cmdOut->print("/");
    cmdOut->print(day);
    cmdOut->println(")");
    return;
  }
  if (strncmp(line, "settime", 7) == 0 &&
      (line[7] == '\0' || line[7] == ' ')) {
    // Meant to be sent by the ESP32 bridge once it has a real date from
    // NTP (see docs/esp32-network-bridge.md) - nothing stops a human from
    // using it directly too, e.g. to test the Countdown face without an
    // ESP32 attached at all.
    const char *arg = skipSpaces(line + 7);
    char *end;
    long year = strtol(arg, &end, 10);
    const char *monthArg = (end == arg) ? end : skipSpaces(end);
    long month = strtol(monthArg, &end, 10);
    const char *dayArg = (end == monthArg) ? end : skipSpaces(end);
    long day = strtol(dayArg, &end, 10);
    if (end == dayArg || year < 2000 || year > 9999 || month < 1 ||
        month > 12 || day < 1 || day > 31) {
      cmdOut->println("Usage: settime <year> <month> <day>");
      return;
    }
    countdownFace.setSyncedDate((uint16_t)year, (uint8_t)month, (uint8_t)day);
    cmdOut->print("Date synced: ");
    cmdOut->print(year);
    cmdOut->print("-");
    cmdOut->print(month);
    cmdOut->print("-");
    cmdOut->println(day);
    return;
  }
  if (strncmp(line, "setclock", 8) == 0 &&
      (line[8] == '\0' || line[8] == ' ')) {
    // Paired with "settime" above (see its comment) but feeds the Clock
    // face's wall-clock time instead of the Countdown face's date -
    // separate command since they're unrelated faces with unrelated
    // state, sent together by the ESP32 bridge's own periodic sync.
    const char *arg = skipSpaces(line + 8);
    char *end;
    long hour = strtol(arg, &end, 10);
    const char *minuteArg = (end == arg) ? end : skipSpaces(end);
    long minute = strtol(minuteArg, &end, 10);
    const char *secondArg = (end == minuteArg) ? end : skipSpaces(end);
    long second = strtol(secondArg, &end, 10);
    if (end == secondArg || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 59) {
      cmdOut->println("Usage: setclock <hour 0-23> <minute> <second>");
      return;
    }
    clockFace.setSyncedTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second);
    cmdOut->print("Clock synced: ");
    cmdOut->print(hour);
    cmdOut->print(":");
    cmdOut->print(minute);
    cmdOut->print(":");
    cmdOut->println(second);
    return;
  }
  if (strncmp(line, "clockformat", 11) == 0 &&
      (line[11] == '\0' || line[11] == ' ')) {
    const char *arg = skipSpaces(line + 11);
    if (arg[0] == '\0') {
      cmdOut->print("Clock format: ");
      cmdOut->println(clockFace.use12Hour() ? "12" : "24");
      return;
    }
    if (strcmp(arg, "12") == 0) {
      clockFace.setUse12Hour(true);
    } else if (strcmp(arg, "24") == 0) {
      clockFace.setUse12Hour(false);
    } else {
      cmdOut->println("Usage: clockformat [12|24]");
      return;
    }
    cmdOut->print("Clock format set to: ");
    cmdOut->println(arg);
    return;
  }
  if (strncmp(line, "audio", 5) == 0 && (line[5] == '\0' || line[5] == ' ')) {
    const char *arg = skipSpaces(line + 5);
    if (strcmp(arg, "tone") == 0) {
      I2sPlayer::playTestTone();
      cmdOut->println("Played test tone");
    } else if (strcmp(arg, "voice") == 0) {
      I2sPlayer::playClip(test_word, test_word_length, test_word_sample_rate);
      cmdOut->println("Played test voice clip");
    } else if (strcmp(arg, "esp-tone") == 0) {
      pendingEsp32Audio = Esp32AudioTrigger::Tone;
      cmdOut->println("Queued for the ESP32 bridge to play: tone");
    } else if (strcmp(arg, "esp-voice") == 0) {
      pendingEsp32Audio = Esp32AudioTrigger::Voice;
      cmdOut->println("Queued for the ESP32 bridge to play: voice");
    } else {
      cmdOut->println("Usage: audio <tone|voice|esp-tone|esp-voice>");
    }
    return;
  }
  if (strcmp(line, "audiotrigger") == 0) {
    // Meant to be polled by the ESP32 bridge, not typed by a human (see
    // pendingEsp32Audio's comment) - reads and clears in one step so a
    // trigger only ever fires once even if polled again before the ESP32
    // gets around to acting on it.
    switch (pendingEsp32Audio) {
    case Esp32AudioTrigger::Tone:
      cmdOut->println("tone");
      break;
    case Esp32AudioTrigger::Voice:
      cmdOut->println("voice");
      break;
    default:
      cmdOut->println("none");
      break;
    }
    pendingEsp32Audio = Esp32AudioTrigger::None;
    return;
  }
  if (strncmp(line, "text", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
    const char *arg = skipSpaces(line + 4);
    if (arg[0] == '\0') {
      cmdOut->println("Usage: text <line1>[|line2[|line3[|line4]]]");
      return;
    }
    // Copied out (rather than split in place) since `line`/`arg` alias the
    // owning SerialPort's line buffer, and setText()'s arguments all need
    // to stay valid for the duration of the call.
    char buf[kSerialLineCapacity];
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
    selectFace(kConfigurableTextFaceIndex);
    cmdOut->print("ConfigurableText set to: ");
    for (uint8_t i = 0; i < lineCount; i++) {
      if (i > 0) {
        cmdOut->print(" / ");
      }
      cmdOut->print(lines[i]);
    }
    cmdOut->println();
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
        selectFace(kConfigurableTextFaceIndex);
        cmdOut->print("ConfigurableText font set to: ");
        cmdOut->println(kFontOptions[i].name);
        return;
      }
    }
    cmdOut->print("Unknown font: ");
    cmdOut->println(arg);
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
      cmdOut->print("Rotate interval set to ");
      if (autoRotateMs == 0) {
        cmdOut->println("off");
      } else {
        cmdOut->print(autoRotateMs);
        cmdOut->println(" ms");
      }
    } else {
      cmdOut->println("Usage: rotate <ms>");
    }
    return;
  }
  char *end;
  long index = strtol(line, &end, 10);
  if (end != line && *end == '\0' && index >= 0 &&
      (size_t)index < kFaceCount) {
    faceEnabled[index] = !faceEnabled[index];
    cmdOut->print(faceNames[index]);
    cmdOut->println(faceEnabled[index] ? ": enabled" : ": disabled");
    return;
  }
  advanceFace();
}

void pollSerialPort(SerialPort &port) {
  while (port.stream->available()) {
    char c = port.stream->read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      port.line[port.length] = '\0';
      cmdOut = port.stream;
      handleSerialCommand(port.line);
      port.length = 0;
    } else if (port.length < sizeof(port.line) - 1) {
      port.line[port.length++] = c;
    }
  }
}

void pollSerial() {
  pollSerialPort(usbPort);
  pollSerialPort(espPort);
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
    randomOrder = prefs.randomOrder != 0;
    countdownFace.setHoliday(prefs.holidayName, (uint8_t)prefs.holidayMonth,
                             (uint8_t)prefs.holidayDay);
    applyTextPrefs(prefs);
    // Validated before ever reaching beginEsp32Link() below: requesting an
    // invalid UART1 pin from the SDK hard-faults the whole chip before USB
    // even finishes enumerating, which would otherwise turn one bad saved
    // value into a boot loop with no way to reach it to fix it again.
    if (isValidUart1Pins((int8_t)prefs.esp32TxPin, (int8_t)prefs.esp32RxPin)) {
      esp32TxPin = (int8_t)prefs.esp32TxPin;
      esp32RxPin = (int8_t)prefs.esp32RxPin;
    } else {
      Serial.println("Saved ESP32 link pins aren't a valid UART1 pair; "
                     "using compiled-in defaults instead");
    }
    Serial.println("Loaded saved config from flash");
  } else {
    Serial.println("No saved config in flash; using compiled-in defaults");
  }
  // Brings up the ESP32 bridge UART - exactly once, now that esp32TxPin/
  // esp32RxPin have their final boot-time values (see beginEsp32Link()'s
  // comment for why this can't just be re-called later to apply a change).
  beginEsp32Link();
  printHelp();

  randomSeed(micros());

  pinMode(PIN_NEXT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_PAUSE_BUTTON, INPUT_PULLUP);
  pinMode(PIN_ORDER_BUTTON, INPUT_PULLUP);

  leds.begin();
  leds.show(); // all off until the first updateLeds() in loop()

  I2sPlayer::begin();

  display.begin();
  Serial.println("Display initialized");
  selectFace(0);
  lastAutoRotateMs = millis();
}

void loop() {
  pollSerial();

  unsigned long now = millis();
  if (buttonPressed(nextButton, now)) {
    advanceFace();
  }
  if (buttonPressed(pauseButton, now)) {
    toggleRotation();
  }
  if (buttonPressed(orderButton, now)) {
    toggleOrderMode();
  }

  // A status message expires back to whatever face preceded it - unless the
  // user (or auto-rotate, below) already navigated away from it, in which
  // case there's nothing left to restore.
  if (statusMessageUntilMs != 0) {
    if (currentFace != kStatusMessageFaceIndex) {
      statusMessageUntilMs = 0;
    } else if (now >= statusMessageUntilMs) {
      statusMessageUntilMs = 0;
      selectFace(faceBeforeStatusMessage);
      lastAutoRotateMs = now;
    }
  }

  if (autoRotateMs > 0 && currentFace != kStatusMessageFaceIndex &&
      now - lastAutoRotateMs >= autoRotateMs) {
    advanceFace();
  }

  faces[currentFace]->update();
  faces[currentFace]->draw(display.gfx());
  updateLeds();

  delay(16); // ~60 fps target
}
