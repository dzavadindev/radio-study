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

// pragma pack tells the compiler align fields on 1-byte boundaries
// this will prevent the radios sending garbled packets,
// because the structs structure is stricter and will not be optimized away
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

static bool inTest = false;  // Currently running the test for the config
static uint16_t curTestId = 0;
static uint16_t expected = 0;
static uint16_t got = 0;

static void applyRadio(uint8_t ch, rf24_datarate_e rate, rf24_pa_dbm_e pa, uint8_t ard, uint8_t arc) {
  radio.stopListening();
  radio.flush_rx();
  radio.flush_tx();
  radio.clearStatusFlags();

  radio.setChannel(ch);
  radio.setDataRate(rate);
  radio.setPALevel(pa);
  radio.setRetries(ard, arc);

  radio.startListening();
}

// Sets the radio into Control Mode
static void enterControl() {
  inTest = false;
  applyRadio(CONTROL_CHANNEL, CONTROL_RATE, CONTROL_PA, CONTROL_ARD, CONTROL_ARC);
  radio.openReadingPipe(1, CTRL_ADDR);
  Serial.println("RX: CONTROL (waiting CONFIG)");
}

// Sets the radio params to the received params
static void enterTestFromCfg(const ConfigPkt& c) {
  inTest = true;
  curTestId = c.testId;
  expected = c.packets;
  got = 0;

  applyRadio(c.channel, (rf24_datarate_e)c.rate, (rf24_pa_dbm_e)c.pa, c.ard, c.arc);
  radio.openReadingPipe(1, DATA_ADDR);

  Serial.print("RX: TEST mode armed. testId=");
  Serial.print(curTestId);
  Serial.print(" expected=");
  Serial.println(expected);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!radio.begin()) {
    Serial.println("RX: radio.begin() FAILED");
    while (1) {}
  }
  if (!radio.isChipConnected()) {
    Serial.println("RX: nRF24 not connected (check wiring/power)");
    while (1) {}
  }

  radio.setAddressWidth(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.setPayloadSize(32);
  radio.startListening();

  Serial.println("RX: started");
  radio.printDetails();

  enterControl();
}

void loop() {
  while (radio.available()) {
    uint8_t buf[32];
    radio.read(buf, sizeof(buf));

    const uint8_t type = buf[0];

    // ---------- RECEIVING A CONFIGURATION
    if (!inTest) {
      if (type != PKT_CONFIG) continue;

      ConfigPkt c{};
      memcpy(&c, buf, sizeof(c));
      if (c.magic != MAGIC) {
        Serial.println("RX: bad CONFIG magic");
        continue;
      }

      Serial.println("RX: got CONFIG (ACKed by hardware). Switching to TEST...");
      enterTestFromCfg(c);
      continue;
    }

    // --------- TESTING A CONFIGURATION
    if (type == PKT_PING) {
      PingPkt p{};
      memcpy(&p, buf, sizeof(p));
      if (p.magic == MAGIC && p.testId == curTestId) {
        Serial.println("RX: got PING (test config confirmed)");
      }
    } else if (type == PKT_DATA) {
      DataPkt d{};
      memcpy(&d, buf, sizeof(d));
      if (d.magic == MAGIC && d.testId == curTestId) {
        got++;
      }
    } else if (type == PKT_END) {
      EndPkt e{};
      memcpy(&e, buf, sizeof(e));
      if (e.magic == MAGIC && e.testId == curTestId) {
        Serial.print("RX: got END. got=");
        Serial.print(got);
        Serial.print(" expected=");
        Serial.println(expected);
        enterControl();
      }
    }
  }
}
