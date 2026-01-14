#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

#define CE_PIN D7
#define CSN_PIN D8
#define MAGIC 0xC0FFEE42;

RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t CTRL_ADDR[6] = "CTL01";
static const uint8_t DATA_ADDR[6] = "DAT01";

// Control mode fixed
#define CONTROL_CHANNEL 2
#define CONTROL_RATE RF24_1MBPS
#define CONTROL_PA RF24_PA_LOW
#define CONTROL_ARD 5
#define CONTROL_ARC 15

enum PktType : uint8_t {
  PKT_CONFIG = 1,
  PKT_PING = 2,
  PKT_DATA = 3,
  PKT_END = 4
};

#pragma pack(push, 1)
struct ConfigPkt {
  uint8_t type;
  uint16_t testId;
  uint16_t packets;
  uint8_t pa;
  uint8_t rate;
  uint8_t channel;
  uint8_t ard;
  uint8_t arc;
  uint32_t magic;
};
struct PingPkt {
  uint8_t type;
  uint16_t testId;
  uint32_t magic;
};
struct DataPkt {
  uint8_t type;
  uint16_t testId;
  uint16_t seq;
  uint32_t magic;
};
struct EndPkt {
  uint8_t type;
  uint16_t testId;
  uint32_t magic;
};
#pragma pack(pop)

static constexpr uint16_t TEST_ID = 1;
static constexpr uint16_t N_PACKETS = 200;

static constexpr uint8_t TEST_CH = 40;
static constexpr rf24_datarate_e TEST_RATE = RF24_1MBPS;
static constexpr rf24_pa_dbm_e TEST_PA = RF24_PA_LOW;
static constexpr uint8_t TEST_ARD = 2;
static constexpr uint8_t TEST_ARC = 7;

static constexpr uint16_t INTERVAL_MS = 5;

static void applyRadio(uint8_t ch, rf24_datarate_e rate, rf24_pa_dbm_e pa, uint8_t ard, uint8_t arc) {
  radio.stopListening();
  radio.flush_rx();
  radio.flush_tx();
  radio.clearStatusFlags();

  radio.setChannel(ch);
  radio.setDataRate(rate);
  radio.setPALevel(pa);
  radio.setRetries(ard, arc);

  delay(3);
}

static bool sendWithRetries(const void* p, uint8_t len, int tries, int delayMs) {
  for (int i = 0; i < tries; i++) {
    if (radio.write(p, len)) return true;
    delay(delayMs);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(800);

  if (!radio.begin()) {
    Serial.println("TX: radio.begin() FAILED");
    while (1) {}
  }
  if (!radio.isChipConnected()) {
    Serial.println("TX: nRF24 not connected (check wiring/power)");
    while (1) {}
  }

  radio.setAddressWidth(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.setPayloadSize(32);

  Serial.println("TX: started");
  radio.printDetails();

  // ---- send CONFIG ----
  applyRadio(CONTROL_CHANNEL, CONTROL_RATE, CONTROL_PA, CONTROL_ARD, CONTROL_ARC);
  radio.openWritingPipe(CTRL_ADDR);

  ConfigPkt c{};
  c.type = PKT_CONFIG;
  c.testId = TEST_ID;
  c.packets = N_PACKETS;
  c.pa = (uint8_t)TEST_PA;
  c.rate = (uint8_t)TEST_RATE;
  c.channel = TEST_CH;
  c.ard = TEST_ARD;
  c.arc = TEST_ARC;
  c.magic = MAGIC;

  Serial.println("TX: sending CONFIG...");
  if (!sendWithRetries(&c, sizeof(c), 20, 20)) {
    Serial.println("TX: CONFIG FAILED (no ACK on control) -> stop");
    while (1) {}
  }
  Serial.println("TX: CONFIG ACKed");

  // ---- switch + PING until ACK ----
  applyRadio(TEST_CH, TEST_RATE, TEST_PA, TEST_ARD, TEST_ARC);
  radio.openWritingPipe(DATA_ADDR);

  PingPkt p{};
  p.type = PKT_PING;
  p.testId = TEST_ID;
  p.magic = MAGIC;

  Serial.println("TX: pinging on TEST config...");
  if (!sendWithRetries(&p, sizeof(p), 100, 10)) {
    Serial.println("TX: PING FAILED (RX likely did not switch) -> stop");
    while (1) {}
  }
  Serial.println("TX: PING ACKed, starting DATA...");

  // ---- DATA trial ----
  uint32_t sent = 0, acked = 0, failed = 0, retriesSum = 0;

  DataPkt d{};
  d.type = PKT_DATA;
  d.testId = TEST_ID;
  d.magic = MAGIC;

  for (uint16_t i = 0; i < N_PACKETS; i++) {
    d.seq = i;
    sent++;
    bool ok = radio.write(&d, sizeof(d));
    retriesSum += radio.getARC();
    if (ok) acked++;
    else failed++;
    delay(INTERVAL_MS);
  }

  // ---- END ----
  EndPkt e{};
  e.type = PKT_END;
  e.testId = TEST_ID;
  e.magic = MAGIC;

  (void)sendWithRetries(&e, sizeof(e), 20, 10);

  float per = (sent > 0) ? (float)failed / (float)sent : 0.0f;
  float avgRetries = (sent > 0) ? (float)retriesSum / (float)sent : 0.0f;

  Serial.println("\n--- RESULT (TX) ---");
  Serial.print("sent=");
  Serial.print(sent);
  Serial.print(" acked=");
  Serial.print(acked);
  Serial.print(" failed=");
  Serial.print(failed);
  Serial.print(" PER=");
  Serial.print(per, 6);
  Serial.print(" avgRetries=");
  Serial.println(avgRetries, 3);

  Serial.println("TX: done");
}

void loop() {}
