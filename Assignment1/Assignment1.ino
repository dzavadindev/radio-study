#include "RF24.h"
#include <MCP23S08.h>
#include <SPI.h>

#define CE_PIN 9
#define CSN_PIN 8
#define MCP_CS_PIN 10

RF24 radio(CE_PIN, CSN_PIN);
MCP23S08 MCP(MCP_CS_PIN);

// Pick one for board:
#define DEVICE_ADDR 1  // dev01 board: 0, dev02 board: 1

const uint8_t ADDR_FIRST[6] = "dev01";
const uint8_t ADDR_SECOND[6] = "dev02";

const uint8_t *MY_ADDR = DEVICE_ADDR ? ADDR_FIRST : ADDR_SECOND;
const uint8_t *OTHER_ADDR = DEVICE_ADDR ? ADDR_SECOND : ADDR_FIRST;

static const uint8_t NUM_LEDS = 6;  // MCP pins 0..5
static const uint8_t BTN_INC = 6;   // MCP pin 6
static const uint8_t BTN_DEC = 7;   // MCP pin 7

static const unsigned long DEBOUNCE_MS = 30;

struct Msg {
  uint8_t sender;    // 0=dev01, 1=dev02
  uint8_t seq;       // increments per send
  uint8_t ledCount;  // 0..NUM_LEDS (the full state)
};

uint8_t mySeq = 0;
uint8_t lastSeqFromOther = 255;

uint8_t ledCount = 0;

// debounce state (buttons are pulled up; pressed = LOW)
bool lastRawInc = true, stableInc = true;
bool lastRawDec = true, stableDec = true;
unsigned long lastChangeInc = 0;
unsigned long lastChangeDec = 0;

// "newer seq" check (prevents accepting older out-of-order packets)
static bool isNewerSeq(uint8_t s, uint8_t last) {
  if (s == last)
    return false;
  return (uint8_t)(s - last) < 128;
}

void applyLedCount() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    MCP.write1(i, (i < ledCount) ? 1 : 0);
  }
}

bool sendState(uint8_t newState) {
  Msg m;
  m.sender = DEVICE_ADDR;
  m.seq = ++mySeq;
  m.ledCount = newState;

  radio.stopListening();
  bool ok = radio.write(&m, sizeof(m));
  radio.startListening();

  Serial.print("TX SET ledCount=");
  Serial.print(m.ledCount);
  Serial.print(" seq=");
  Serial.print(m.seq);
  Serial.print(" -> ");
  Serial.println(ok ? "OK" : "FAIL");

  return ok;
}

// update local state FIRST, then broadcast full state
void updateLocalAndBroadcast(int8_t delta) {
  int16_t next = (int16_t)ledCount + (int16_t)delta;
  if (next < 0)
    next = 0;
  if (next > NUM_LEDS)
    next = NUM_LEDS;

  if ((uint8_t)next == ledCount)
    return;

  uint8_t target = (uint8_t)next;

  bool ok = sendState(target);
  if (!ok) {
    delay(5);
    ok = sendState(target);
  }

  if (ok) {
    ledCount = target;
    applyLedCount();
  }
}


void handleButtonsAndSend() {
  unsigned long now = millis();

  // HIGH=not pressed, LOW=pressed
  bool rawInc = (MCP.read1(BTN_INC) != 0);
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
      updateLocalAndBroadcast(1);
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
      updateLocalAndBroadcast(-1);
    }
  }
}

void handleReceive() {
  while (radio.available()) {
    Msg m;
    radio.read(&m, sizeof(m));

    // probably a useless guard, but better be safe
    if (m.sender == DEVICE_ADDR)
      continue;

    // ignore duplicates / out-of-order older messages
    if (!isNewerSeq(m.seq, lastSeqFromOther))
      continue;
    lastSeqFromOther = m.seq;

    if (m.ledCount > NUM_LEDS)
      m.ledCount = NUM_LEDS;
    ledCount = m.ledCount;
    applyLedCount();

    Serial.print(F("RX SET ledCount="));
    Serial.print(ledCount);
    Serial.print(F(" seq="));
    Serial.println(m.seq);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // deactivate CS before SPI
  pinMode(CSN_PIN, OUTPUT);
  digitalWrite(CSN_PIN, HIGH);
  pinMode(MCP_CS_PIN, OUTPUT);
  digitalWrite(MCP_CS_PIN, HIGH);

  SPI.begin();

  // radio init
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(76);
  radio.setRetries(5, 15);
  radio.setPayloadSize(sizeof(Msg));
  radio.setAutoAck(true);
  radio.openWritingPipe(OTHER_ADDR);
  radio.openReadingPipe(1, MY_ADDR);
  radio.startListening();

  // MCP init
  if (!MCP.begin()) {
    Serial.println(F("MCP23S08 not responding!!"));
    while (1) {
    }
  }

  MCP.pinMode8(0b11000000);    // 6,7 inputs; 0..5 outputs
  MCP.setPullup8(0b11000000);  // pullups on 6,7

  ledCount = 0;
  applyLedCount();

  stableInc = lastRawInc = (MCP.read1(BTN_INC) != 0);
  stableDec = lastRawDec = (MCP.read1(BTN_DEC) != 0);
  lastChangeInc = lastChangeDec = millis();

  Serial.print("I am ");
  Serial.println(DEVICE_ADDR ? "dev01" : "dev02");

  // broadcast initial state once so the other side snaps to it, if it's already running
  sendState(ledCount);
}

void loop() {
  handleButtonsAndSend();
  handleReceive();
  delay(5);
}
