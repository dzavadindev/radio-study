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

#define CE_PIN 9
#define CSN_PIN 10
RF24 radio(CE_PIN, CSN_PIN);

static const uint8_t RF_CHANNEL = 76; // 0..125
static const rf24_datarate_e RF_RATE =
    RF24_1MBPS; // RF24_250KBPS / RF24_1MBPS / RF24_2MBPS
static const rf24_pa_dbm_e RF_PA = RF24_PA_LOW; // MIN/LOW/HIGH/MAX
static const uint8_t RETRY_DELAY = 5;           // 0..15 (steps of 250us)
static const uint8_t RETRY_COUNT = 15;          // 0..15
// ===== Address (must match RX) =====
const uint8_t ADDR[6] = "THR01";
// ===== Test parameters =====
static const uint32_t TOTAL_NUMBERS = 10000;
static const uint32_t WORDS_PER_PACKET =
    8; // 8*32-bit = 32 bytes payload is max for the RF24
static const uint32_t TOTAL_BITS = TOTAL_NUMBERS * 32UL;

static const uint32_t MAGIC = 0xA5A5A5A5;

struct Packet {
  uint32_t w[WORDS_PER_PACKET];
};

uint32_t ackExpected = 0;

bool sendPacketAndGetAck(const Packet &p, uint32_t &ackOut) {
  bool ok = radio.write(&p, sizeof(p)); // auto-ack enabled
  if (!ok)
    return false;

  if (radio.isAckPayloadAvailable()) {
    radio.read(&ackOut,
               sizeof(ackOut)); // ackOut = "next expected number index" at RX
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

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
  radio.openWritingPipe(ADDR);
  radio.stopListening();
  radio.enableAckPayload();
  Serial.println("TX ready. Starting throughput test...");
}

void loop() {
  // ----- Step 1: sync/reset RX expected counter to 0 -----
  Packet resetPkt = {};
  resetPkt.w[0] = MAGIC;
  resetPkt.w[1] = TOTAL_NUMBERS;
  resetPkt.w[2] = WORDS_PER_PACKET;

  while (true) {
    uint32_t ack = 0xFFFFFFFF;
    bool gotAck = sendPacketAndGetAck(resetPkt, ack);
    if (gotAck && ack == 0) {
      Serial.println("RX synced (ack=0).");
      break;
    }
    Serial.println("Sync retry...");
    delay(10);
  }

  // ----- Step 2: send data -----
  uint32_t seq = 0;
  uint32_t startMs = millis();

  while (seq < TOTAL_NUMBERS) {
    Packet p;
    for (uint32_t i = 0; i < WORDS_PER_PACKET; i++) {
      p.w[i] = seq + i; // numbers are 0..9999
    }

    uint32_t ack = 0xFFFFFFFF;
    bool gotAck = sendPacketAndGetAck(p, ack);

    // RX returns next expected index (0..10000)
    if (gotAck && ack == (seq + WORDS_PER_PACKET)) {
      seq += WORDS_PER_PACKET;
    } else {
      // resend same packet
    }
  }

  uint32_t endMs = millis();
  float seconds = (endMs - startMs) / 1000.0f;

  float achieved_bps = (seconds > 0) ? (TOTAL_BITS / seconds) : 0.0f;

  Serial.println("----- RESULT -----");
  Serial.print("Configured RF rate: ");
  if (RF_RATE == RF24_250KBPS)
    Serial.println("250 kbps");
  else if (RF_RATE == RF24_1MBPS)
    Serial.println("1 Mbps");
  else if (RF_RATE == RF24_2MBPS)
    Serial.println("2 Mbps");
  else
    Serial.println("unknown");

  Serial.print("Total bits sent: ");
  Serial.println(TOTAL_BITS);
  Serial.print("Time (s): ");
  Serial.println(seconds, 6);
  Serial.print("Achieved throughput (bps): ");
  Serial.println(achieved_bps, 2);
  Serial.print("Achieved throughput (kbps): ");
  Serial.println(achieved_bps / 1000.0f, 2);

  Serial.println("------------------");
  Serial.println("Done. Reset board to repeat with a different RF_RATE.");

  while (1) {
  }
}
