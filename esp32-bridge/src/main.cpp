// Optional ESP32 Wi-Fi bridge for the Jabberin' Jack pumpkin controller.
//
// Links up to the RP2040 (see jack_link.h for the two supported transports
// and docs/esp32-network-bridge.md for wiring) and reuses its existing
// text-command serial protocol (the same one documented in the top-level
// README's "Runtime Controls" section) - this board doesn't reimplement
// any pumpkin logic, it just relays that protocol over Wi-Fi and serves a
// phone-friendly control page for it.
//
// First boot (or after a long-press/erased config) opens a temporary
// "JabberinJack-Setup" access point with a captive portal for picking the
// home Wi-Fi network; after that it connects on its own and advertises
// itself over mDNS as http://jabberinjack.local/.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <time.h>

#include "assets/test_word.h"
#include "audio/i2s_player.h"
#include "jack_link.h"
#include "web_ui.h"

namespace {
constexpr const char *kHostname = "jabberinjack"; // -> jabberinjack.local
constexpr const char *kSetupApName = "JabberinJack-Setup";
constexpr const char *kSetupApPassword = "pumpkin123"; // >= 8 chars for WPA2

// How long to wait for the RP2040 to finish replying to a command before
// giving up, and how long a gap between bytes counts as "done replying"
// (a handful of characters arrive in well under a millisecond over either
// transport, so 150ms of silence comfortably means the reply is over, not
// that another line is still coming).
constexpr unsigned long kReplyTimeoutMs = 1500;
constexpr unsigned long kReplyQuietGapMs = 150;

// How often to ask the RP2040 whether it has audio queued for this
// board's speaker (see the RP2040's "audio esp-tone"/"audio esp-voice"
// commands and pendingEsp32Audio) - frequent enough that a trigger feels
// responsive, infrequent enough not to spam the link or matter if a
// request happens to land while a query is already in flight (nothing
// actually runs concurrently here; see checkForRp2040AudioTrigger()).
constexpr unsigned long kAudioTriggerPollMs = 500;

// The RP2040 has no battery-backed RTC (see its Countdown face's class
// comment) - this board is the only source of truth for "what day is it",
// via NTP. Retried aggressively until the first sync actually lands (SNTP
// hasn't necessarily resolved yet right after Wi-Fi connects), then just
// often enough afterward to notice a real day rolling over without
// spamming the link over what's otherwise unchanging information.
constexpr unsigned long kTimeSyncRetryMs = 10UL * 1000UL;       // 10s
constexpr unsigned long kTimeSyncIntervalMs = 30UL * 60UL * 1000UL; // 30min
// getLocalTime() blocks up to this long waiting for SNTP if it hasn't
// resolved yet - short enough not to stall the web server/audio-trigger
// poll noticeably on a cycle where it fails.
constexpr uint32_t kTimeSyncTimeoutMs = 2000;
// No timezone/DST handling (see setup()'s configTime() call) - the
// Countdown face only ever needs a plain calendar date, and this board has
// no UI yet to ask the person which timezone they're in, so this is
// deliberately UTC's own date rather than guessing. That can make the
// countdown off by one for a matter of hours right around local midnight,
// which is a fair trade against the complexity of a real timezone picker
// for what's just a decorative day-count.
} // namespace

WebServer server(80);

void drainJackLink() {
  Stream &link = jackLink();
  while (link.available()) {
    link.read();
  }
}

// Sends `cmd` (without a trailing newline - this adds it) to the RP2040 and
// collects whatever it prints back, stripping '\r' but keeping '\n' between
// lines (some commands like "list" reply with several). Returns "" if
// nothing came back within kReplyTimeoutMs.
String sendCommandAndRead(const String &cmd) {
  Stream &link = jackLink();
  drainJackLink();
  link.print(cmd);
  link.print('\n');

  String reply;
  unsigned long start = millis();
  unsigned long lastByte = start;
  while (millis() - start < kReplyTimeoutMs) {
    if (link.available()) {
      // Drain everything already sitting in the UART's buffer back-to-back,
      // with no yield() in between - a longer reply (e.g. "status" or
      // "list" listing every face) arrives as hundreds of bytes in well
      // under a millisecond of actual wire time, but yield()ing after each
      // one anyway hands time to the Wi-Fi/network stack often enough to
      // blow well past kReplyTimeoutMs before ever finishing, truncating
      // the reply. yield() still runs below, just only while genuinely
      // idle-waiting for more.
      char c = (char)link.read();
      lastByte = millis();
      if (c != '\r') {
        reply += c;
      }
      continue;
    }
    if (reply.length() > 0 && millis() - lastByte > kReplyQuietGapMs) {
      break;
    }
    yield();
  }
  return reply;
}

// Minimal JSON string escaping for wrapping an arbitrary RP2040 reply into
// the {"reply": "..."} envelope /api/command returns. RP2040 status text
// only ever contains printable ASCII plus the newlines between list lines.
String jsonEscapeString(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    default:
      out += c;
    }
  }
  return out;
}

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

// Passes the RP2040's own "status" JSON straight through - it's already in
// the shape the web UI wants (see printStatusJson() in the RP2040 firmware),
// so there's nothing for this bridge to parse or reassemble.
void handleApiStatus() {
  if (!jackLinkReady()) {
    server.send(503, "application/json",
               "{\"error\":\"no pumpkin controller connected\"}");
    return;
  }
  String reply = sendCommandAndRead("status");
  reply.trim();
  if (reply.length() == 0) {
    server.send(504, "application/json",
               "{\"error\":\"no response from pumpkin controller\"}");
    return;
  }
  server.send(200, "application/json", reply);
}

// Forwards the POST body verbatim as one command line (only the text up to
// the first newline, if the body somehow contains more than one) and
// relays back whatever the RP2040 replied with.
void handleApiCommand() {
  if (!jackLinkReady()) {
    server.send(503, "application/json",
               "{\"ok\":false,\"error\":\"no pumpkin controller connected\"}");
    return;
  }
  String cmd = server.arg("plain");
  int newline = cmd.indexOf('\n');
  if (newline >= 0) {
    cmd = cmd.substring(0, newline);
  }
  cmd.trim();

  String reply = sendCommandAndRead(cmd);
  String json = "{\"ok\":true,\"reply\":\"" + jsonEscapeString(reply) + "\"}";
  server.send(200, "application/json", json);
}

// Reports this bridge's own link GPIO pins (distinct from anything in the
// RP2040's "status" JSON, which knows nothing about the ESP32 side) so the
// web UI can show/edit them without a reflash - see jack_link.h.
void handleApiLinkPinsGet() {
  int rxPin, txPin;
  jackLinkGetPins(&rxPin, &txPin);
  String json = String("{\"supported\":") +
                (jackLinkSupportsPinConfig() ? "true" : "false") +
                ",\"rxPin\":" + rxPin + ",\"txPin\":" + txPin + "}";
  server.send(200, "application/json", json);
}

void handleApiLinkPinsPost() {
  if (!jackLinkSupportsPinConfig()) {
    server.send(400, "application/json",
               "{\"ok\":false,\"error\":\"this build's link has no GPIO "
               "pins to configure\"}");
    return;
  }
  if (!server.hasArg("rxPin") || !server.hasArg("txPin")) {
    server.send(400, "application/json",
               "{\"ok\":false,\"error\":\"missing rxPin/txPin\"}");
    return;
  }
  int rxPin = server.arg("rxPin").toInt();
  int txPin = server.arg("txPin").toInt();
  if (!jackLinkSetPins(rxPin, txPin)) {
    server.send(400, "application/json",
               "{\"ok\":false,\"error\":\"invalid pins (must be distinct, "
               "and rxPin can't be one of GPIO34-39)\"}");
    return;
  }
  String json = "{\"ok\":true,\"rxPin\":" + String(rxPin) +
                ",\"txPin\":" + String(txPin) + ",\"restarting\":true}";
  server.send(200, "application/json", json);
  // New pins only actually take effect on the next jackLinkBegin() call
  // (see uart_link.cpp's applyPins() comment for why it can't just be
  // re-applied live) - restart to get there, once the response above has
  // actually gone out over Wi-Fi.
  server.client().flush();
  delay(300);
  ESP.restart();
}

// Polls the RP2040 for audio it wants played on this board's own speaker
// (see its "audio esp-tone"/"audio esp-voice" commands) and plays it
// locally if so - see kAudioTriggerPollMs's comment for why polling
// rather than the RP2040 pushing this unprompted. A stale/no-reply link
// (jackLinkReady() false, or a timed-out empty reply) is silently treated
// the same as "none" - nothing to play, try again next cycle.
void checkForRp2040AudioTrigger() {
  if (!jackLinkReady()) {
    return;
  }
  String reply = sendCommandAndRead("audiotrigger");
  reply.trim();
  if (reply == "tone") {
    I2sPlayer::playTestTone();
  } else if (reply == "voice") {
    I2sPlayer::playClip(test_word, test_word_length, test_word_sample_rate);
  }
}

// Plays audio on this board's own speaker directly, independent of the
// RP2040 entirely - lets the web UI (or a script) test this board's I2S
// wiring without needing the RP2040 attached at all.
void handleApiLocalAudio() {
  String kind = server.arg("plain");
  kind.trim();
  if (kind == "tone") {
    I2sPlayer::playTestTone();
  } else if (kind == "voice") {
    I2sPlayer::playClip(test_word, test_word_length, test_word_sample_rate);
  } else {
    server.send(400, "text/plain", "Usage: tone|voice");
    return;
  }
  server.send(200, "text/plain", "OK");
}

// Feeds the RP2040's Countdown face today's actual date via "settime" (see
// its class comment on why it can't know this on its own). Returns true
// only on an actual successful sync - the caller uses that to switch from
// the aggressive kTimeSyncRetryMs retry cadence to the relaxed
// kTimeSyncIntervalMs one, only once there's an initial date to be stale
// relative to.
bool syncTimeToRp2040() {
  if (!jackLinkReady()) {
    return false;
  }
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, kTimeSyncTimeoutMs)) {
    return false; // SNTP hasn't resolved yet (or no internet) - retry later
  }
  // tm_year is years since 1900, tm_mon is 0-11 - see <time.h>. Sized
  // generously (not just for the realistic date range) so -Werror=
  // format-truncation can't flag this as possibly-truncating based on
  // int's own theoretical range.
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "settime %d %d %d", timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1, timeinfo.tm_mday);
  sendCommandAndRead(cmd);
  return true;
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

void setup() {
  Serial.begin(115200);
  Serial.println("Jabberin' Jack ESP32 bridge starting");

  jackLinkBegin();

  WiFi.setHostname(kHostname);
  WiFiManager wifiManager;
  // Falls back to the "JabberinJack-Setup" AP/captive-portal whenever there's
  // no saved network or it can't be reached - no hardcoded SSID/password in
  // this firmware to begin with.
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect(kSetupApName, kSetupApPassword)) {
    Serial.println("Wi-Fi setup timed out with nothing connected; "
                   "rebooting to try again");
    delay(1000);
    ESP.restart();
  }
  Serial.print("Wi-Fi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // UTC (0 offset, no DST) - see kTimeSyncTimeoutMs's comment on why this
  // doesn't attempt real timezone handling. Starts the SNTP client;
  // doesn't block here, syncTimeToRp2040()'s getLocalTime() calls (from
  // loop()) are what actually wait for it to resolve.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS ready: http://%s.local/\n", kHostname);
  } else {
    Serial.println("mDNS setup failed (Wi-Fi network may not support it); "
                   "the IP address above still works");
  }

  I2sPlayer::begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/command", HTTP_POST, handleApiCommand);
  server.on("/api/link-pins", HTTP_GET, handleApiLinkPinsGet);
  server.on("/api/link-pins", HTTP_POST, handleApiLinkPinsPost);
  server.on("/api/local-audio", HTTP_POST, handleApiLocalAudio);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  static unsigned long lastAudioTriggerPollMs = 0;
  if (now - lastAudioTriggerPollMs >= kAudioTriggerPollMs) {
    lastAudioTriggerPollMs = now;
    checkForRp2040AudioTrigger();
  }

  static unsigned long lastTimeSyncMs = 0;
  static bool timeEverSynced = false;
  unsigned long timeSyncIntervalMs =
      timeEverSynced ? kTimeSyncIntervalMs : kTimeSyncRetryMs;
  if (now - lastTimeSyncMs >= timeSyncIntervalMs) {
    lastTimeSyncMs = now;
    if (syncTimeToRp2040()) {
      timeEverSynced = true;
    }
  }
}

#ifdef JACK_LINK_USB_HOST
// The esp32s3_usbhost env's espidf+arduino hybrid framework doesn't
// auto-generate the usual Arduino app_main() shim the way a plain
// `framework = arduino` build does - ESP-IDF's own startup code
// (components/freertos/port/port_common.c) calls this directly as the
// real entry point, so without it the link fails with "undefined
// reference to `app_main'". The esp32dev (UART) env doesn't need this;
// its plain Arduino framework already provides one.
extern "C" void app_main(void) {
  initArduino();
  setup();
  for (;;) {
    loop();
  }
}
#endif
