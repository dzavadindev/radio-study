// ===============================
// TX sketch (Transmitter)
// - Loops presets in RF24TestConfigs.h
// - Sends CONFIG (control), then PING+DATA+END (test)
// - Waits for RX CONTROL using a probe packet (so scripts stay aligned)
// ===============================

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "RF24TestConfigs.h"

#define CE_PIN 7
#define CSN_PIN 8
#define MAGIC 0xC0FFEE42UL

RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t CTRL_ADDR[6] = "CTL01";
static const uint8_t DATA_ADDR[6] = "DAT01";

// Control mode fixed (MUST match RX control)
static const uint8_t CONTROL_CHANNEL = 2;
static const rf24_datarate_e CONTROL_RATE = RF24_1MBPS;
static const rf24_pa_dbm_e CONTROL_PA = RF24_PA_LOW;
static const uint8_t CONTROL_ARD = 5;
static const uint8_t CONTROL_ARC = 15;

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
  uint8_t paCode;   // 0=MIN,1=LOW,2=HIGH,3=MAX
  uint8_t rateCode; // 0=250K,1=1M,2=2M
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

static constexpr uint16_t BASE_TEST_ID = 1;
static constexpr uint16_t N_PACKETS = 500;
static constexpr uint16_t INTERVAL_MS = 5;

static void applyControlRadio() {
  radio.stopListening();
  radio.flush_rx();
  radio.flush_tx();
  radio.clearStatusFlags();

  radio.setChannel(CONTROL_CHANNEL);
  radio.setDataRate(CONTROL_RATE);
  radio.setPALevel(CONTROL_PA);
  radio.setRetries(CONTROL_ARD, CONTROL_ARC);
  delay(3);
}

static bool sendWithRetries(const void *p, uint8_t len, int tries,
                            int delayMs) {
  for (int i = 0; i < tries; i++) {
    if (radio.write(p, len))
      return true;
    delay(delayMs);
  }
  return false;
}

static uint8_t rateCodeFromEnum(rf24_datarate_e r) {
  if (r == RF24_250KBPS)
    return 0;
  if (r == RF24_1MBPS)
    return 1;
  return 2;
}
static uint8_t paCodeFromEnum(rf24_pa_dbm_e p) {
  if (p == RF24_PA_MIN)
    return 0;
  if (p == RF24_PA_LOW)
    return 1;
  if (p == RF24_PA_HIGH)
    return 2;
  return 3;
}

static bool waitForRxControlReady(uint32_t timeoutMs) {
  applyControlRadio();
  radio.openWritingPipe(CTRL_ADDR);

  uint8_t probe[32] = {0};
  probe[0] = 0x55;

  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (radio.write(probe, sizeof(probe))) {
      Serial.println("TX: RX back in CONTROL");
      return true;
    }
    delay(10);
  }
  Serial.println("TX: timeout waiting RX CONTROL");
  return false;
}

static void runOneTest(size_t idx) {
  const RadioConfig &cfg = rf24cfg_get(idx);
  const uint16_t testId = BASE_TEST_ID + (uint16_t)idx;

  Serial.println("\n==============================");
  Serial.print("Preset: ");
  Serial.println(cfg.name);

  // Make sure RX is in CONTROL before CONFIG
  waitForRxControlReady(4000);

  // 1) CONFIG on control link
  applyControlRadio();
  radio.openWritingPipe(CTRL_ADDR);

  ConfigPkt c{};
  c.type = PKT_CONFIG;
  c.testId = testId;
  c.packets = N_PACKETS;
  c.paCode = paCodeFromEnum(cfg.pa);
  c.rateCode = rateCodeFromEnum(cfg.rate);
  c.channel = cfg.channel;
  c.ard = cfg.ard;
  c.arc = cfg.arc;
  c.magic = MAGIC;

  Serial.println("TX: sending CONFIG...");
  if (!sendWithRetries(&c, sizeof(c), 60, 20)) {
    Serial.println("TX: CONFIG FAILED -> skip preset");
    return;
  }
  Serial.println("TX: CONFIG ACKed");

  // 2) switch to test + PING
  rf24cfg_apply(radio, cfg);
  radio.openWritingPipe(DATA_ADDR);

  PingPkt p{};
  p.type = PKT_PING;
  p.testId = testId;
  p.magic = MAGIC;

  Serial.println("TX: pinging...");
  if (!sendWithRetries(&p, sizeof(p), 200, 10)) {
    Serial.println("TX: PING FAILED -> skip preset");
    return;
  }
  Serial.println("TX: PING ACKed");

  // 3) DATA
  uint32_t sent = 0, acked = 0, failed = 0, retriesSum = 0;

  DataPkt d{};
  d.type = PKT_DATA;
  d.testId = testId;
  d.magic = MAGIC;

  for (uint16_t i = 0; i < N_PACKETS; i++) {
    d.seq = i;
    sent++;
    bool ok = radio.write(&d, sizeof(d));
    retriesSum += radio.getARC();
    if (ok)
      acked++;
    else
      failed++;
    delay(INTERVAL_MS);
  }

  // 4) END
  EndPkt e{};
  e.type = PKT_END;
  e.testId = testId;
  e.magic = MAGIC;
  (void)sendWithRetries(&e, sizeof(e), 40, 10);

  float per = (sent > 0) ? (float)failed / (float)sent : 0.0f;
  float avgRetries = (sent > 0) ? (float)retriesSum / (float)sent : 0.0f;

  Serial.println("--- RESULT (TX) ---");
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

  // Let RX timeout back to CONTROL even if END got lost
  waitForRxControlReady(4000);
}

void setup() {
  Serial.begin(115200);
  delay(800);

  if (!radio.begin()) {
    Serial.println("TX: begin FAILED");
    while (1) {
    }
  }
  if (!radio.isChipConnected()) {
    Serial.println("TX: not connected");
    while (1) {
    }
  }

  radio.setAddressWidth(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.stopListening();

  Serial.println("TX: started");
  radio.printDetails();

  for (size_t i = 0; i < rf24cfg_count(); i++) {
    runOneTest(i);
    delay(500);
  }

  Serial.println("\nTX: all presets done");
}

void loop() {}
