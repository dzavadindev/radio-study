#include <SPI.h>
#include "RF24.h"
#include "MCP23S08.h"

#define CE_PIN 9
#define CSN_PIN 8
#define MCP_nCS 10

RF24 radio(CE_PIN, CSN_PIN);
MCP23S08 MCP(MCP_nCS);

uint8_t address[][6] = { "thijs", "dan00" };

uint8_t lights_state = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin()) {
    Serial.println("Radio hardware is not responding!!");
    while (1) {}
  }

  Serial.println("RF24 Receiver: listening on \"dan00\"");

  // RADIO
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);

  radio.setPayloadSize(sizeof(lights_state));
  radio.setChannel(20);

  radio.openReadingPipe(1, address[1]);
  radio.openWritingPipe(address[0]);

  radio.startListening();

  radio.printDetails();

  // MCP
  SPI.begin();

  MCP.begin();

  MCP.pinMode8(0b11000000);
  MCP.setPullup8(0b11000000);

  for (int pin = 0; pin < 6; pin++) {
    MCP.write1(pin, 1);
  }
}

void loop() {
  static uint32_t last = 0;
  static uint32_t upperLastChange = 0;
  static uint32_t lowerLastChange = 0;
  static bool upperWasPressed = false;
  static bool lowerWasPressed = false;

  uint32_t now = millis();
  bool upperPressed = (MCP.read1(6) == 0);
  bool lowerPressed = (MCP.read1(7) == 0);

  if (radio.available()) {
    uint8_t bytes = radio.getPayloadSize();
    uint8_t payload = 0;

    radio.read(&payload, bytes);
    Serial.println("Receiving a reading. . .");

    if ((payload & 192) == 0) { return; }

    MCP.write8(payload);
  }

  handle_button(upperPressed, now, &upperLastChange, &upperWasPressed, true);
  handle_button(lowerPressed, now, &lowerLastChange, &lowerWasPressed, false);

  upperWasPressed = upperPressed;
  lowerWasPressed = lowerPressed;
}

static void handle_button(bool pressed, uint32_t now, uint32_t* lastChange, bool* wasPressed, bool action) {
  const uint32_t DEBOUNCE_MS = 50;
  if (pressed && !*wasPressed && (now - *lastChange) > DEBOUNCE_MS) {
    radio.stopListening();

    if (action) {
      lights_state >>= 1;
    } else {
      lights_state = (lights_state << 1) | 1;
    }

    // radio.write(&lights_state, sizeof(lights_state));
    MCP.write8(lights_state);

    radio.startListening();
    *lastChange = now;
  }
}
