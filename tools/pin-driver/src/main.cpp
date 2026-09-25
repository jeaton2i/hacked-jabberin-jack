// Throwaway output-driver test rig for the Jabberin' Jack XL's parallel LCD
// panel, using the confirmed wiring from ../../pin map (verified by
// continuity through the ribbon cable to the panel itself). Only wire this
// up once the original XL controller chip is disconnected from the panel -
// two active drivers on the same lines at once can damage both chips.
//
// Pin roles (confirmed wiring, roles inferred from a standard 8080-style
// parallel LCD bus - REB/WRB/CLE/CS/RST here play the same roles as
// RD/WR/DC(RS)/CS/RST on the base model's ILI9225):
//   FD0-FD7: 8-bit data bus (bit order per connector pin adjacency, still
//     a guess for now - if the image looks scrambled rather than blank,
//     that's a sign to revisit bit order rather than give up)
//   REB (GP4): read strobe, active low - held HIGH (inactive), we only write
//   WRB (GP5): write strobe, active low - pulsed once per byte
//   CLE (GP6): command/data select, polarity unconfirmed - toggle with 'w'
//   CS  (GP7): chip select - a real capture showed this constant HIGH
//     during active traffic, so defaulting to active-high (toggle 'q')
//   RST (GP20): reset - a real capture showed this constant LOW during
//     active traffic, so defaulting to active-high too (toggle 'e')
#include <Arduino.h>

struct Pin {
  uint8_t gpio;
  const char *label;
};

static const Pin kDataPins[8] = {
    {3, "fd0"},   // bit0
    {2, "fd1"},   // bit1
    {1, "fd2"},   // bit2
    {0, "fd3"},   // bit3
    {16, "fd4"},  // bit4
    {17, "fd5"},  // bit5
    {18, "fd6"},  // bit6
    {19, "fd7"},  // bit7
};
static const Pin kReb = {4, "reb"};
static const Pin kWrb = {5, "wrb"};
static const Pin kCle = {6, "cle"};
static const Pin kCs = {7, "cs"};
static const Pin kRst = {20, "rst"};

static bool g_cleHighIsCommand = true;  // toggle with 'w' if backwards
// A real capture of the panel's own controller showed CS sitting constant
// HIGH and RST constant LOW throughout active traffic - the opposite of
// the usual active-low convention their names suggest. Defaulting to that
// observed (active-high) polarity here instead of guessing active-low.
static bool g_csActiveLow = false;   // toggle with 'q' if backwards
static bool g_rstActiveLow = false;  // toggle with 'e' if backwards
static bool g_running = false;

static void assertCs(bool asserted) {
  digitalWrite(kCs.gpio, asserted == g_csActiveLow ? LOW : HIGH);
}

static void writeByte(uint8_t value, bool isCommand) {
  for (int b = 0; b < 8; b++) {
    digitalWrite(kDataPins[b].gpio, (value >> b) & 1 ? HIGH : LOW);
  }
  bool cleHigh = isCommand == g_cleHighIsCommand;
  digitalWrite(kCle.gpio, cleHigh ? HIGH : LOW);
  delayMicroseconds(1);
  digitalWrite(kWrb.gpio, LOW);
  delayMicroseconds(1);
  digitalWrite(kWrb.gpio, HIGH);
  delayMicroseconds(1);
}

static void setRst(bool asserted) {
  digitalWrite(kRst.gpio, asserted == g_rstActiveLow ? LOW : HIGH);
}

static void doResetPulse() {
  Serial.println("Pulsing RST...");
  setRst(true);
  delay(10);
  setRst(false);
  delay(150);
}

static void printStatus() {
  Serial.println();
  Serial.println("=== Jabberin' Jack XL parallel-LCD output-driver rig ===");
  Serial.print("CLE: ");
  Serial.print(g_cleHighIsCommand ? "HIGH" : "LOW");
  Serial.println(" = command, opposite = data (toggle with 'w')");
  Serial.print("CS: active-");
  Serial.println(g_csActiveLow ? "low" : "high");
  Serial.print("RST: active-");
  Serial.println(g_rstActiveLow ? "low" : "high");
  Serial.println("Commands:");
  Serial.println("  s  reset pulse only (watch for any flicker/blank change)");
  Serial.println("  1  reset + common init (SWRESET/SLPOUT/DISPON) + fill loop");
  Serial.println("  2  reset + raw memory-write (0x2C) + fill loop, no init cmds");
  Serial.println("  x  stop an in-progress fill loop");
  Serial.println("  w  toggle CLE polarity (command vs data level)");
  Serial.println("  q  toggle CS polarity");
  Serial.println("  e  toggle RST polarity");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
  }

  for (size_t i = 0; i < 8; i++) {
    pinMode(kDataPins[i].gpio, OUTPUT);
    digitalWrite(kDataPins[i].gpio, LOW);
  }
  pinMode(kReb.gpio, OUTPUT);
  digitalWrite(kReb.gpio, HIGH);  // inactive - we only write
  pinMode(kWrb.gpio, OUTPUT);
  digitalWrite(kWrb.gpio, HIGH);  // idle/inactive
  pinMode(kCle.gpio, OUTPUT);
  digitalWrite(kCle.gpio, LOW);
  pinMode(kCs.gpio, OUTPUT);
  pinMode(kRst.gpio, OUTPUT);
  setRst(false);  // inactive (not in reset) until we deliberately pulse it

  assertCs(true);  // asserted for the whole session

  printStatus();
}

static void runFillLoop() {
  static const uint16_t kColors[] = {0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F};
  g_running = true;
  Serial.println("FILLING... (send 'x' to stop)");
  size_t colorIdx = 0;
  while (g_running) {
    uint16_t color = kColors[colorIdx];
    for (uint32_t i = 0; i < 40000 && g_running; i++) {
      writeByte((i & 1) ? (color & 0xFF) : (color >> 8), false);
      if ((i & 0xFFF) == 0 && Serial.available()) {
        if (Serial.peek() == 'x') {
          Serial.read();
          g_running = false;
        }
      }
    }
    colorIdx = (colorIdx + 1) % (sizeof(kColors) / sizeof(kColors[0]));
    delay(800);
  }
  Serial.println("FILL_STOPPED");
}

static void commonInitAndFill() {
  doResetPulse();
  writeByte(0x01, true);  // software reset (common opcode)
  delay(150);
  writeByte(0x11, true);  // sleep out (common opcode)
  delay(150);
  writeByte(0x29, true);  // display on (common opcode)
  delay(50);
  writeByte(0x2C, true);  // memory write (common opcode)
  runFillLoop();
}

static void rawMemoryWriteAndFill() {
  doResetPulse();
  writeByte(0x2C, true);  // memory write, no other init
  runFillLoop();
}

void loop() {
  if (!Serial.available()) return;
  int c = Serial.read();
  if (c == 's') {
    doResetPulse();
    Serial.println("Reset pulse done.");
  } else if (c == '1') {
    commonInitAndFill();
  } else if (c == '2') {
    rawMemoryWriteAndFill();
  } else if (c == 'w') {
    g_cleHighIsCommand = !g_cleHighIsCommand;
    printStatus();
  } else if (c == 'q') {
    g_csActiveLow = !g_csActiveLow;
    assertCs(true);
    printStatus();
  } else if (c == 'e') {
    g_rstActiveLow = !g_rstActiveLow;
    setRst(false);
    printStatus();
  } else if (c == '\r' || c == '\n') {
    // ignore
  } else {
    printStatus();
  }
}
