/*
  nRF24 Throughput Test (TX)

  Sends 10,000 32-bit numbers (320,000 payload bits) to a matching RX and
  measures total transfer time to compute achieved throughput. Data is sent in
  32-byte packets (8 numbers). Reliability: RX returns an ACK payload with the
  “next expected index”; TX resends a packet until it receives the expected ACK,
  ensuring all numbers arrive in order.

  Change per experiment: RF_RATE (250k/1M/2M), RF_CHANNEL, RF_PA, RETRY
  settings. Requires RX sketch that supports ACK payloads and returns the next
  expected index.
*/

#include <RF24.h>
#include <SPI.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t RF_CHANNEL = 76;               // 0..125
static const rf24_datarate_e RF_RATE = RF24_1MBPS;  // RF24_250KBPS / RF24_1MBPS / RF24_2MBPS
static const rf24_pa_dbm_e RF_PA = RF24_PA_LOW;     // MIN/LOW/HIGH/MAX
static const uint8_t RETRY_DELAY = 5;               // 0..15 (steps of 250us)
static const uint8_t RETRY_COUNT = 15;              // 0..15

// ===== Address (must match RX) =====
const uint8_t ADDR[6] = "THR01";

// ===== Test parameters =====
static const uint32_t TOTAL_NUMBERS = 10000;
static const uint32_t TOTAL_BITS = TOTAL_NUMBERS * 32UL;

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!radio.begin()) {
    Serial.println("RF24 begin() failed");
    while (1) {}
  }

  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF_RATE);
  radio.setPALevel(RF_PA);
  radio.setRetries(RETRY_DELAY, RETRY_COUNT);

  radio.setAutoAck(true);
  radio.enableAckPayload();

  radio.setCRCLength(RF24_CRC_16);

  radio.disableDynamicPayloads();
  radio.setPayloadSize(sizeof(uint32_t)); 

  radio.openWritingPipe(ADDR);
  radio.stopListening();

  radio.flush_rx();
  radio.flush_tx();
  radio.clearStatusFlags();

  Serial.println("TX ready. Starting throughput test...");
}

void loop() {
  uint32_t startMs = millis();

  uint32_t thing = 12345;
  bool a = radio.write(&thing, sizeof(thing));

  for (uint32_t i = 0; i < TOTAL_NUMBERS; i++) {
    // Keep sending i until it is ACKed
    while (!radio.write(&i, sizeof(i))) {
      // auto-retry inside radio also happens; this loop retries again if it still failed
    Serial.println("Loading . . .");
    }
  }

  uint32_t endMs = millis();
  float seconds = (endMs - startMs) / 1000.0f;
  float achieved_bps = (seconds > 0) ? (TOTAL_BITS / seconds) : 0.0f;

  Serial.println("----- RESULT -----");
  Serial.print("Configured RF rate: ");
  if (RF_RATE == RF24_250KBPS) Serial.println("250 kbps");
  else if (RF_RATE == RF24_1MBPS) Serial.println("1 Mbps");
  else if (RF_RATE == RF24_2MBPS) Serial.println("2 Mbps");
  else Serial.println("unknown");

  Serial.print("Time (s): ");
  Serial.println(seconds, 6);

  Serial.print("Achieved throughput (bps): ");
  Serial.println(achieved_bps, 2);

  Serial.print("Achieved throughput (kbps): ");
  Serial.println(achieved_bps / 1000.0f, 2);

  Serial.println("------------------");
  Serial.println("Change RF_RATE (both TX+RX), upload, and reset to repeat.");

  while (1) {}
}
