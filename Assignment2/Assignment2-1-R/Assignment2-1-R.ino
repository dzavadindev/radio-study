#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#define CE_PIN 9
#define CSN_PIN 8
#define MAGIC  0xC0FFEE42UL

RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t CTRL_ADDR[6] = "CTL01";
static const uint8_t DATA_ADDR[6] = "DAT01";

// Control mode fixed 
static const uint8_t         CONTROL_CHANNEL = 2;
static const rf24_datarate_e CONTROL_RATE    = RF24_1MBPS;
static const rf24_pa_dbm_e   CONTROL_PA      = RF24_PA_LOW;
static const uint8_t         CONTROL_ARD     = 5;
static const uint8_t         CONTROL_ARC     = 15;

// If RX is in TEST mode and sees no valid TEST packets for this long -> back to CONTROL
static const uint32_t TEST_TIMEOUT_MS = 1500;

enum PktType : uint8_t { PKT_CONFIG=1, PKT_PING=2, PKT_DATA=3, PKT_END=4 };

#pragma pack(push, 1)
struct ConfigPkt {
  uint8_t  type;
  uint16_t testId;
  uint16_t packets;
  uint8_t  paCode;    // 0=MIN,1=LOW,2=HIGH,3=MAX
  uint8_t  rateCode;  // 0=250K,1=1M,2=2M
  uint8_t  channel;
  uint8_t  ard;
  uint8_t  arc;
  uint32_t magic;
};
struct PingPkt {
  uint8_t  type;
  uint16_t testId;
  uint32_t magic;
};
struct DataPkt {
  uint8_t  type;
  uint16_t testId;
  uint16_t seq;
  uint32_t magic;
};
struct EndPkt {
  uint8_t  type;
  uint16_t testId;
  uint32_t magic;
};
#pragma pack(pop)

// -------- state --------
static bool     inTest = false;
static uint16_t curTestId = 0;
static uint16_t expectedPackets = 0;
static uint16_t gotPackets = 0;
static uint32_t lastTestSeenMs = 0;

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
  radio.startListening();
}

static void enterControl() {
  inTest = false;
  applyRadio(CONTROL_CHANNEL, CONTROL_RATE, CONTROL_PA, CONTROL_ARD, CONTROL_ARC);
  radio.openReadingPipe(1, CTRL_ADDR);
  Serial.println("RX: CONTROL (waiting CONFIG)");
}

static void enterTestFromCfg(const ConfigPkt& c) {
  inTest = true;
  curTestId = c.testId;
  expectedPackets = c.packets;
  gotPackets = 0;
  lastTestSeenMs = millis();

  applyRadio(c.channel, c.rateCode, c.paCode, c.ard, c.arc);
  radio.openReadingPipe(1, DATA_ADDR);

  Serial.print("RX: TEST mode. testId=");
  Serial.print(curTestId);
  Serial.print(" expectedPackets=");
  Serial.print(expectedPackets);
  Serial.print(" ch=");
  Serial.print(c.channel);
  Serial.print(" rateCode=");
  Serial.print(c.rateCode);
  Serial.print(" paCode=");
  Serial.println(c.paCode);
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  if (!radio.begin()) { Serial.println("RX: begin FAILED"); while (1) {} }

  radio.setAddressWidth(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.startListening();

  Serial.println("RX: started");
  radio.printDetails();

  enterControl();
}

void loop() {
  // Timeout safety: if we're in TEST and haven't seen valid TEST packets, go back to CONTROL
  if (inTest && (millis() - lastTestSeenMs > TEST_TIMEOUT_MS)) {
    Serial.println("RX: TEST timeout -> back to CONTROL");
    Serial.println("------------------");
    enterControl();
  }

  while (radio.available()) {
    uint8_t buf[32];
    radio.read(buf, sizeof(buf));
    const uint8_t type = buf[0];

    if (!inTest) {
      if (type != PKT_CONFIG) continue;

      ConfigPkt c{};
      memcpy(&c, buf, sizeof(c));
      if (c.magic != MAGIC) { Serial.println("RX: bad CONFIG magic"); continue; }

      Serial.println("------------------");
      Serial.print("RX: got CONFIG testId=");
      Serial.print(c.testId);
      Serial.println(" -> switching to TEST");
      enterTestFromCfg(c);
      continue;
    }

    // in TEST mode:
    switch (type) {
      case PKT_PING: {
        PingPkt p{};
        memcpy(&p, buf, sizeof(p));
        if (p.magic == MAGIC && p.testId == curTestId) {
          lastTestSeenMs = millis();
        }
        break;
      }

      case PKT_DATA: {
        DataPkt d{};
        memcpy(&d, buf, sizeof(d));
        if (d.magic == MAGIC && d.testId == curTestId) {
          gotPackets++;
          lastTestSeenMs = millis();
        }
        break;
      }

      case PKT_END: {
        EndPkt e{};
        memcpy(&e, buf, sizeof(e));
        if (e.magic == MAGIC && e.testId == curTestId) {
          Serial.print("RX: got END. gotPackets=");
          Serial.print(gotPackets);
          Serial.print(" expectedPackets=");
          Serial.println(expectedPackets);
          Serial.println("------------------");
          enterControl();
        }
        break;
      }

      default:
        break;
    }
  }
}
