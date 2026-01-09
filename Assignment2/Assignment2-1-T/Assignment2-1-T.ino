#include "RF24.h"
#include <SPI.h>

// ---------- PINS ----------
#define CE_PIN 9
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// ---------- ADDRESSES (must match RX) ----------
const uint8_t ADDR_RX[6] = "rx001";
const uint8_t ADDR_TX[6] = "tx001";

// ---------- CONTROL MODE (fixed, must match RX) ----------
static const uint8_t CONTROL_CHANNEL = 2;
static const rf24_datarate_e CONTROL_RATE = RF24_1MBPS;
static const rf24_pa_dbm_e CONTROL_PA = RF24_PA_LOW;
static const uint8_t CONTROL_ARD = 5;
static const uint8_t CONTROL_ARC = 15;

// ========== USER SETTINGS ==========
#define RUN_SWEEP 1 // 1 = sweep many settings, 0 = run one setting
static const uint16_t PACKETS_PER_TEST = 500;

// Sweep sets (edit freely)
static const rf24_pa_dbm_e PA_LEVELS[] = {RF24_PA_MIN, RF24_PA_LOW,
                                          RF24_PA_HIGH, RF24_PA_MAX};
static const uint8_t CHANNELS[] = {40, 76, 100}; // avoid CONTROL_CHANNEL
static const rf24_datarate_e RATES[] = {RF24_250KBPS, RF24_1MBPS, RF24_2MBPS};

struct RetryCfg {
  uint8_t ard;
  uint8_t arc;
};
static const RetryCfg RETRIES[] = {{0, 0}, // ART off
                                   {5, 3},
                                   {5, 15},
                                   {15, 15}};

// Single test (used if RUN_SWEEP=0)
static const rf24_pa_dbm_e SINGLE_PA = RF24_PA_LOW;
static const uint8_t SINGLE_CH = 76;
static const rf24_datarate_e SINGLE_RATE = RF24_1MBPS;
static const uint8_t SINGLE_ARD = 5;
static const uint8_t SINGLE_ARC = 15;
// ================================

enum PktType : uint8_t { PKT_CONFIG = 1, PKT_DATA = 2, PKT_END = 3 };

struct __attribute__((packed)) ConfigPkt {
  uint8_t type; // PKT_CONFIG
  uint16_t testId;
  uint16_t packets; // how many data packets TX will send
  uint8_t pa;       // rf24_pa_dbm_e
  uint8_t rate;     // rf24_datarate_e
  uint8_t channel;  // 0..125
  uint8_t ard;      // 0..15
  uint8_t arc;      // 0..15
};

struct __attribute__((packed)) DataPkt {
  uint8_t type; // PKT_DATA
  uint16_t testId;
  uint16_t seq;
  uint32_t magic;
};

struct __attribute__((packed)) EndPkt {
  uint8_t type; // PKT_END
  uint16_t testId;
};

static bool write32(const void *p, uint8_t len) {
  uint8_t buf[32] = {0};
  if (len > 32)
    len = 32;
  memcpy(buf, p, len);
  return radio.write(buf, 32);
}

static const char *paStr(uint8_t pa) {
  switch (pa) {
  case RF24_PA_MIN:
    return "MIN";
  case RF24_PA_LOW:
    return "LOW";
  case RF24_PA_HIGH:
    return "HIGH";
  case RF24_PA_MAX:
    return "MAX";
  default:
    return "?";
  }
}
static const char *rateStr(uint8_t r) {
  switch (r) {
  case RF24_250KBPS:
    return "250K";
  case RF24_1MBPS:
    return "1M";
  case RF24_2MBPS:
    return "2M";
  default:
    return "?";
  }
}

static void applyRadio(uint8_t channel, rf24_datarate_e rate, rf24_pa_dbm_e pa,
                       uint8_t ard, uint8_t arc) {
  radio.stopListening(); // TX doesn’t listen
  radio.setChannel(channel);
  radio.setDataRate(rate);
  radio.setPALevel(pa);
  radio.setRetries(ard, arc);
}

static void enterControlMode() {
  applyRadio(CONTROL_CHANNEL, CONTROL_RATE, CONTROL_PA, CONTROL_ARD,
             CONTROL_ARC);
}

static bool sendConfig(uint16_t testId, rf24_pa_dbm_e pa, rf24_datarate_e rate,
                       uint8_t channel, uint8_t ard, uint8_t arc) {
  ConfigPkt c;
  c.type = PKT_CONFIG;
  c.testId = testId;
  c.packets = PACKETS_PER_TEST;
  c.pa = (uint8_t)pa;
  c.rate = (uint8_t)rate;
  c.channel = channel;
  c.ard = ard;
  c.arc = arc;

  enterControlMode();
  delay(5);
  return write32(&c, sizeof(c));
}

static void runOneTest(uint16_t testId, rf24_pa_dbm_e pa, rf24_datarate_e rate,
                       uint8_t channel, uint8_t ard, uint8_t arc) {
  // 1) Tell RX what to switch to (control channel)
  bool cfgOk = sendConfig(testId, pa, rate, channel, ard, arc);
  if (!cfgOk) {
    Serial.println(F("CONFIG failed (no ACK). Is RX running on control mode?"));
    return;
  }

  // 2) Switch TX to test settings and send packets
  applyRadio(channel, rate, pa, ard, arc);
  delay(10);

  uint16_t ok = 0, fail = 0;

  for (uint16_t i = 0; i < PACKETS_PER_TEST; i++) {
    DataPkt d;
    d.type = PKT_DATA;
    d.testId = testId;
    d.seq = i;
    d.magic = 0xC0FFEE00u ^ ((uint32_t)testId << 8) ^ i;

    bool w = write32(&d, sizeof(d));
    if (w)
      ok++;
    else
      fail++;

    // pacing helps stability on some cheap modules
    delayMicroseconds(500);
  }

  // 3) END marker (software retry so RX prints summary reliably)
  EndPkt e;
  e.type = PKT_END;
  e.testId = testId;

  bool endOk = false;
  for (uint8_t t = 0; t < 50; t++) {
    if (write32(&e, sizeof(e))) {
      endOk = true;
      break;
    }
    delay(10);
  }

  // TX-side PER (failures = no ACK even after HW retries)
  uint16_t sent = ok + fail;
  float perTx = (sent > 0) ? ((float)fail / (float)sent) : 1.0f;

  Serial.print(F("TEST "));
  Serial.print(testId);
  Serial.print(F(" PA="));
  Serial.print(paStr(pa));
  Serial.print(F(" rate="));
  Serial.print(rateStr(rate));
  Serial.print(F(" ch="));
  Serial.print(channel);
  Serial.print(F(" ARD="));
  Serial.print(ard);
  Serial.print(F(" ARC="));
  Serial.print(arc);
  Serial.print(F(" sent="));
  Serial.print(sent);
  Serial.print(F(" ok="));
  Serial.print(ok);
  Serial.print(F(" fail="));
  Serial.print(fail);
  Serial.print(F(" PER_TX="));
  Serial.print(perTx, 6);
  Serial.print(F(" END="));
  Serial.println(endOk ? F("OK") : F("FAIL"));

  // Back to control for next test
  enterControlMode();
  delay(50);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  if (!radio.begin()) {
    Serial.println(F("RF24 not responding"));
    while (1) {
    }
  }

  radio.setAutoAck(true);
  radio.setPayloadSize(32);

  // TX writes to RX
  radio.openWritingPipe(ADDR_RX);

  enterControlMode();
  Serial.println(F("PER_TX ready"));
}

void loop() {
  static uint16_t testId = 1;

#if RUN_SWEEP
  Serial.println(F("Starting PER sweep..."));

  for (uint8_t ip = 0; ip < (sizeof(PA_LEVELS) / sizeof(PA_LEVELS[0])); ip++) {
    for (uint8_t ic = 0; ic < (sizeof(CHANNELS) / sizeof(CHANNELS[0])); ic++) {
      for (uint8_t ir = 0; ir < (sizeof(RATES) / sizeof(RATES[0])); ir++) {
        for (uint8_t ia = 0; ia < (sizeof(RETRIES) / sizeof(RETRIES[0]));
             ia++) {
          runOneTest(testId++, PA_LEVELS[ip], RATES[ir], CHANNELS[ic],
                     RETRIES[ia].ard, RETRIES[ia].arc);
        }
      }
    }
  }

  Serial.println(F("\nSweep complete. Reset TX to run again."));
  while (1) {
  }

#else
  Serial.println(F("Running SINGLE PER test..."));
  runOneTest(testId++, SINGLE_PA, SINGLE_RATE, SINGLE_CH, SINGLE_ARD,
             SINGLE_ARC);
  Serial.println(F("Single test complete. Reset TX to run again."));
  while (1) {
  }
#endif
}
