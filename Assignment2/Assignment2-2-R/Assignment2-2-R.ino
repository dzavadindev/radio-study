#include <RF24.h>
#include <SPI.h>

// ===== Pins =====
#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t RF_CHANNEL = 76;
static const uint8_t RETRY_DELAY = 5;
static const uint8_t RETRY_COUNT = 15;
static const rf24_pa_dbm_e RF_PA = RF24_PA_LOW;
static const rf24_datarate_e RF_RATE = RF24_1MBPS; // RF24_250KBPS / RF24_1MBPS / RF24_2MBPS


const uint8_t ADDR[6] = "THR01";

static const uint32_t N = 10000;
uint32_t expected = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!radio.begin()) {
    Serial.println("RX: radio.begin() FAILED");
    while (1) {
    }
  }
  if (!radio.isChipConnected()) {
    Serial.println("RX: nRF24 NOT connected (check wiring/power)");
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
  radio.setPayloadSize(sizeof(uint32_t));

  radio.openReadingPipe(1, ADDR);
  radio.startListening();

  Serial.println("RX: ready");
}

void loop() {
  if (!radio.available())
    return;

  uint32_t v;
  radio.read(&v, sizeof(v));

  // Accept only the next expected value.
  // Duplicates happen when TX retries; we ignore those.
  if (v == expected) {
    expected++;

    Serial.println(v);
    if (expected == N) {
      Serial.println("RX: received all 10,000 numbers.");
      while (1) {
      }
    }
  }
}
