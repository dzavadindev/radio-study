#include "RF24.h"
#include <SPI.h>

// ---------- PINS ----------
#define CE_PIN 9
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// ---------- ADDRESSES (must match TX) ----------
const uint8_t ADDR_RX[6] = "rx001";
const uint8_t ADDR_TX[6] = "tx001";

// ---------- CONTROL MODE (fixed) ----------
// TX uses this to tell RX what test settings to switch to
static const uint8_t CONTROL_CHANNEL = 2;
static const rf24_datarate_e CONTROL_RATE = RF24_1MBPS;
static const rf24_pa_dbm_e CONTROL_PA = RF24_PA_LOW;
static const uint8_t CONTROL_ARD = 5;
static const uint8_t CONTROL_ARC = 15;

// ---------- RX tracking ----------
static const uint16_t MAX_TRACK = 1000; // sequences we can de-dup perfectly
static bool seen[MAX_TRACK];

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
  uint16_t seq; // 0..packets-1
  uint32_t magic;
};

struct __attribute__((packed)) EndPkt {
  uint8_t type; // PKT_END
  uint16_t testId;
};

static uint16_t expectedPackets = 0;
static uint16_t uniqueReceived = 0;
static uint16_t currentTestId = 0;
static bool inTestMode = false;

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

// Apply radio settings (and restart listening)
static void applyRadio(uint8_t channel, rf24_datarate_e rate, rf24_pa_dbm_e pa,
                       uint8_t ard, uint8_t arc) {
  radio.stopListening();
  radio.setChannel(channel);
  radio.setDataRate(rate);
  radio.setPALevel(pa);
  radio.setRetries(ard, arc);
  radio.startListening();
}

static void enterControlMode() {
  applyRadio(CONTROL_CHANNEL, CONTROL_RATE, CONTROL_PA, CONTROL_ARD,
             CONTROL_ARC);
  inTestMode = false;
}

static void resetTracking(uint16_t testId, uint16_t packets) {
  currentTestId = testId;
  expectedPackets = packets;
  uniqueReceived = 0;

  uint16_t n = (packets > MAX_TRACK) ? MAX_TRACK : packets;
  for (uint16_t i = 0; i < n; i++)
    seen[i] = false;
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

  // RX listens on ADDR_RX
  radio.openReadingPipe(1, ADDR_RX);
  // Writing pipe not required on RX for this test
  radio.startListening();

  enterControlMode();
  Serial.println(F("PER_RX ready (waiting CONFIG on control channel)"));
}

void loop() {
  while (radio.available()) {
    uint8_t buf[32];
    radio.read(buf, sizeof(buf));
    uint8_t type = buf[0];

    if (type == PKT_CONFIG) {
      ConfigPkt c;
      memcpy(&c, buf, sizeof(c));

      Serial.print(F("\n[CONFIG] testId="));
      Serial.print(c.testId);
      Serial.print(F(" packets="));
      Serial.print(c.packets);
      Serial.print(F(" PA="));
      Serial.print(paStr(c.pa));
      Serial.print(F(" rate="));
      Serial.print(rateStr(c.rate));
      Serial.print(F(" ch="));
      Serial.print(c.channel);
      Serial.print(F(" ARD="));
      Serial.print(c.ard);
      Serial.print(F(" ARC="));
      Serial.println(c.arc);

      resetTracking(c.testId, c.packets);

      // Switch to test settings to receive DATA stream
      applyRadio(c.channel, (rf24_datarate_e)c.rate, (rf24_pa_dbm_e)c.pa, c.ard,
                 c.arc);
      inTestMode = true;
    } else if (type == PKT_DATA) {
      if (!inTestMode)
        continue;

      DataPkt d;
      memcpy(&d, buf, sizeof(d));
      if (d.testId != currentTestId)
        continue;

      if (d.seq < MAX_TRACK) {
        if (!seen[d.seq]) {
          seen[d.seq] = true;
          uniqueReceived++;
        }
      } else {
        // beyond tracking range: still count, but can't perfectly de-dup
        uniqueReceived++;
      }
    } else if (type == PKT_END) {
      if (!inTestMode)
        continue;

      EndPkt e;
      memcpy(&e, buf, sizeof(e));
      if (e.testId != currentTestId)
        continue;

      float per = 1.0f;
      if (expectedPackets > 0)
        per = 1.0f - ((float)uniqueReceived / (float)expectedPackets);

      Serial.print(F("[END] testId="));
      Serial.print(currentTestId);
      Serial.print(F(" uniqueReceived="));
      Serial.print(uniqueReceived);
      Serial.print(F(" expected="));
      Serial.print(expectedPackets);
      Serial.print(F(" PER_RX="));
      Serial.println(per, 6);

      // back to control for next test
      enterControlMode();
      Serial.println(F("Back to control channel, waiting next CONFIG..."));
    }
  }
}
