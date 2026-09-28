// UART transport: a plain 2-wire link to the RP2040's dedicated UART1 (see
// docs/esp32-network-bridge.md). Works on any classic ESP32 dev board.
//
// Self-excluded by JACK_LINK_UART (set in platformio.ini's `esp32dev` env
// only) rather than left out of the build via PlatformIO's build_src_filter,
// since the esp32s3_usbhost env's ESP-IDF-based build ignores that option
// and would otherwise compile this alongside usb_host_link.cpp and hit a
// duplicate-symbol link error.
#ifdef JACK_LINK_UART

#include "jack_link.h"

#include <HardwareSerial.h>
#include <Preferences.h>

namespace {
// Compiled-in defaults, used until something different has been saved via
// jackLinkSetPins(). Assigned via the ESP32's GPIO matrix (any pin can
// route to UART2, unlike the RP2040's fixed alt-function mux). Deliberately
// avoiding GPIO34-39 for RX: that whole range has no pull-up/pull-down
// circuitry in silicon at all (not just "disabled by default" - physically
// absent), so an idle or momentarily-disconnected line there floats
// completely undefined instead of settling high, which is exactly the kind
// of thing that lets it pick up crosstalk from an adjacent wire and misread
// it as real data. GPIO32 is a normal bidirectional pin with real pull
// resistors, and (like GPIO33) isn't one of the ESP32's boot strapping pins.
constexpr int kDefaultRxPin = 32; // <- RP2040 GP20 (its UART1 TX)
constexpr int kDefaultTxPin = 33; // -> RP2040 GP21 (its UART1 RX)
constexpr unsigned long kJackBaud = 115200;

bool isInputOnlyPin(int pin) { return pin >= 34 && pin <= 39; }

Preferences prefs;
int currentRxPin = kDefaultRxPin;
int currentTxPin = kDefaultTxPin;

} // namespace

// Applies currentRxPin/currentTxPin - called exactly once, from
// jackLinkBegin(). Deliberately NOT called again later to apply a changed
// pin live: the RP2040 side of this same bridge (see its beginEsp32Link())
// found that tearing down and restarting a UART peripheral mid-session
// reliably hangs it, and there's no reason to trust this core's
// HardwareSerial to be any more forgiving of the same pattern. A changed
// pin is instead applied by persisting it and restarting the whole board
// (see jackLinkSetPins() and its caller, handleApiLinkPinsPost() in
// main.cpp), which re-runs this exactly the same way a normal boot does.
void applyPins() {
  // Belt-and-suspenders with uart_set_pin()'s own default: hold the line
  // at a defined idle-high whenever the RP2040 isn't actively driving it,
  // rather than letting it float.
  pinMode(currentRxPin, INPUT_PULLUP);
  Serial2.begin(kJackBaud, SERIAL_8N1, currentRxPin, currentTxPin);
}

void jackLinkBegin() {
  prefs.begin("jacklink", /*readOnly=*/false);
  currentRxPin = prefs.getInt("rxPin", kDefaultRxPin);
  currentTxPin = prefs.getInt("txPin", kDefaultTxPin);
  applyPins();
}

Stream &jackLink() { return Serial2; }

bool jackLinkReady() { return true; }

bool jackLinkSupportsPinConfig() { return true; }

void jackLinkGetPins(int *rxPin, int *txPin) {
  *rxPin = currentRxPin;
  *txPin = currentTxPin;
}

// Persists the new pins; does NOT apply them live (see applyPins()'s
// comment) - the caller is responsible for restarting the board once it's
// done anything else it needs to (e.g. sending an HTTP response) first.
bool jackLinkSetPins(int rxPin, int txPin) {
  if (rxPin == txPin || isInputOnlyPin(rxPin)) {
    return false;
  }
  prefs.putInt("rxPin", rxPin);
  prefs.putInt("txPin", txPin);
  return true;
}

#endif // JACK_LINK_UART
