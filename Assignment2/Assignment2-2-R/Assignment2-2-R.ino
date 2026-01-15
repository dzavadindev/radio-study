#include <RF24.h>
#include <SPI.h>

// ===== Pins (UNO/Nano): CE=9, CSN=10 =====
#define CE_PIN 7
#define CSN_PIN 8
RF24 radio(CE_PIN, CSN_PIN);

// ===== Must match TX settings =====
static const uint8_t RF_CHANNEL = 76;
static const rf24_datarate_e RF_RATE = RF24_1MBPS;
static const rf24_pa_dbm_e RF_PA = RF24_PA_LOW;
static const uint8_t RETRY_DELAY = 5;
static const uint8_t RETRY_COUNT = 15;

// ===== Address (must match TX) =====
const uint8_t ADDR[6] = "THR01";

// ===== Test parameters =====
static const uint32_t WORDS_PER_PACKET = 8;
static const uint32_t MAGIC = 0xA5A5A5A5;

struct Packet {
  uint32_t w[WORDS_PER_PACKET];
};

volatile uint32_t expected = 0;  // next 32-bit number index expected (0..10000)

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!radio.begin()) {
    Serial.println("RF24 begin() failed");
    while (1) {
    }
  }

  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF_RATE);
  radio.setPALevel(RF_PA);
  radio.setRetries(RETRY_DELAY, RETRY_COUNT);
  radio.setAutoAck(true);
  radio.setCRCLength(RF24_CRC_16);

  radio.disableDynamicPayloads();
  radio.setPayloadSize(sizeof(Packet));

  radio.openReadingPipe(1, ADDR);

  radio.enableAckPayload();

  radio.startListening();

  Serial.println("RX ready.");
}

void loop() {
  if (!radio.available())
    return;

  Packet p;
  radio.read(&p, sizeof(p));

  // If this is a reset/sync packet:
  if (p.w[0] == MAGIC) {
    expected = 0;
    uint32_t ack = expected;  // send "next expected"
    radio.writeAckPayload(1, &ack, sizeof(ack));
    Serial.println("Synced/reset expected=0");
    return;
  }

  // Validate packet content matches expected sequence
  bool ok = true;
  for (uint32_t i = 0; i < WORDS_PER_PACKET; i++) {
    if (p.w[i] != (expected + i)) {
      ok = false;
      break;
    }
  }

  if (ok) {
    expected += WORDS_PER_PACKET;
  } else {
    // Do not advance; TX will resend based on ack below
  }

  uint32_t ack = expected;  // tell TX what we want next
  radio.writeAckPayload(1, &ack, sizeof(ack));
}
