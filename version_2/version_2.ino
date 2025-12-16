#include <SPI.h>
#include <MCP23S08.h>
#include "RF24.h"

#define CE_PIN 9
#define CSN_PIN 8
#define MCP_CS_PIN 10

RF24 radio(CE_PIN, CSN_PIN);
MCP23S08 MCP(MCP_CS_PIN);

// Pick ONE per board:
#define I_AM_THIJS 0  // Thijs board: 1, Dan board: 0

const uint8_t ADDR_THIJS[6] = "thijs";
const uint8_t ADDR_DAN[6]   = "dan00";

const uint8_t* MY_ADDR    = I_AM_THIJS ? ADDR_THIJS : ADDR_DAN;
const uint8_t* OTHER_ADDR = I_AM_THIJS ? ADDR_DAN   : ADDR_THIJS;

// MCP mapping
static const uint8_t NUM_LEDS = 6;  // MCP pins 0..5
static const uint8_t BTN_INC  = 6;  // MCP pin 6
static const uint8_t BTN_DEC  = 7;  // MCP pin 7

// If LEDs are active-low, swap these:
static const uint8_t LED_ON_LEVEL  = 1;
static const uint8_t LED_OFF_LEVEL = 0;

static const unsigned long DEBOUNCE_MS = 30;

// We will broadcast FULL STATE each time (ledCount)
enum : uint8_t { CMD_SET = 0 };

struct Msg {
  uint8_t sender;    // 0=Thijs, 1=Dan
  uint8_t seq;       // increments per send
  uint8_t cmd;       // CMD_SET
  uint8_t ledCount;  // 0..NUM_LEDS (the full state)
};

uint8_t mySenderId = I_AM_THIJS ? 0 : 1;
uint8_t mySeq = 0;
uint8_t lastSeqFromOther = 255;

uint8_t ledCount = 0;

// Debounce state (buttons are pulled up; pressed = LOW)
bool lastRawInc = true, stableInc = true;
bool lastRawDec = true, stableDec = true;
unsigned long lastChangeInc = 0;
unsigned long lastChangeDec = 0;

// Wrap-safe "newer seq" check (prevents accepting older out-of-order packets)
static bool isNewerSeq(uint8_t s, uint8_t last) {
  if (s == last) return false;
  return (uint8_t)(s - last) < 128;
}

void applyLedCount() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    MCP.write1(i, (i < ledCount) ? LED_ON_LEVEL : LED_OFF_LEVEL);
  }
}

bool sendState(uint8_t newCount) {
  Msg m;
  m.sender   = mySenderId;
  m.seq      = ++mySeq;
  m.cmd      = CMD_SET;
  m.ledCount = newCount;

  radio.stopListening();
  bool ok = radio.write(&m, sizeof(m));
  radio.startListening();

  Serial.print(F("TX SET ledCount="));
  Serial.print(m.ledCount);
  Serial.print(F(" seq="));
  Serial.print(m.seq);
  Serial.print(F(" -> "));
  Serial.println(ok ? F("OK") : F("FAIL"));

  return ok;
}

// Update local state FIRST, then broadcast full state
void updateLocalAndBroadcast(int8_t delta) {
  int16_t next = (int16_t)ledCount + (int16_t)delta;
  if (next < 0) next = 0;
  if (next > NUM_LEDS) next = NUM_LEDS;

  if ((uint8_t)next == ledCount) return; // no change
  ledCount = (uint8_t)next;

  applyLedCount();

  // Send the full state. Optionally try a couple times if it fails.
  if (!sendState(ledCount)) {
    // quick retry (RF24 already does auto-retry, this is extra safety)
    delay(5);
    sendState(ledCount);
  }
}

void handleButtonsAndSend() {
  unsigned long now = millis();

  bool rawInc = (MCP.read1(BTN_INC) != 0);  // HIGH=not pressed, LOW=pressed
  bool rawDec = (MCP.read1(BTN_DEC) != 0);

  // INC debounce
  if (rawInc != lastRawInc) {
    lastRawInc = rawInc;
    lastChangeInc = now;
  }
  if ((now - lastChangeInc) >= DEBOUNCE_MS && rawInc != stableInc) {
    bool prev = stableInc;
    stableInc = rawInc;
    if (prev == true && stableInc == false) {
      // pressed
      updateLocalAndBroadcast(+1);
    }
  }

  // DEC debounce
  if (rawDec != lastRawDec) {
    lastRawDec = rawDec;
    lastChangeDec = now;
  }
  if ((now - lastChangeDec) >= DEBOUNCE_MS && rawDec != stableDec) {
    bool prev = stableDec;
    stableDec = rawDec;
    if (prev == true && stableDec == false) {
      // pressed
      updateLocalAndBroadcast(-1);
    }
  }
}

void handleReceive() {
  while (radio.available()) {
    Msg m;
    radio.read(&m, sizeof(m));

    if (m.sender == mySenderId) continue; // ignore own packets

    // ignore duplicates / out-of-order older messages
    if (!isNewerSeq(m.seq, lastSeqFromOther)) continue;
    lastSeqFromOther = m.seq;

    if (m.cmd == CMD_SET) {
      if (m.ledCount > NUM_LEDS) m.ledCount = NUM_LEDS;
      ledCount = m.ledCount;
      applyLedCount();

      Serial.print(F("RX SET ledCount="));
      Serial.print(ledCount);
      Serial.print(F(" seq="));
      Serial.println(m.seq);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // De-select chip selects before SPI
  pinMode(CSN_PIN, OUTPUT);
  digitalWrite(CSN_PIN, HIGH);
  pinMode(MCP_CS_PIN, OUTPUT);
  digitalWrite(MCP_CS_PIN, HIGH);

  SPI.begin();

  // Radio init FIRST
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(76);
  radio.setRetries(5, 15);
  radio.setPayloadSize(sizeof(Msg));

  radio.openWritingPipe(OTHER_ADDR);
  radio.openReadingPipe(1, MY_ADDR);
  radio.startListening();

  // MCP init
  if (!MCP.begin()) {
    Serial.println(F("MCP23S08 not responding!!"));
    while (1) {}
  }

  MCP.pinMode8(0b11000000);    // 6,7 inputs; 0..5 outputs
  MCP.setPullup8(0b11000000);  // pullups on 6,7

  ledCount = 0;
  applyLedCount();

  stableInc = lastRawInc = (MCP.read1(BTN_INC) != 0);
  stableDec = lastRawDec = (MCP.read1(BTN_DEC) != 0);
  lastChangeInc = lastChangeDec = millis();

  Serial.print(F("I am "));
  Serial.println(I_AM_THIJS ? F("thijs") : F("dan00"));

  // Optional: broadcast initial state once so the other side snaps to it
  sendState(ledCount);
}

void loop() {
  handleButtonsAndSend();
  handleReceive();
  delay(5);
}
