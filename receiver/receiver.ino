#include <SPI.h>
#include "printf.h"
#include "RF24.h"
#include "MCP23S08.h"

#define CE_PIN 9
#define CSN_PIN 8
#define MCP_nCS 10

RF24 radio(CE_PIN, CSN_PIN);
MCP23S08 MCP(MCP_nCS);

uint8_t address[][6] = { "thijs", "dan00" };

uint8_t payload = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));
    while (1) {}
  }

  Serial.println(F("RF24 Receiver: listening on \"dan00\""));

  // RADIO
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(sizeof(payload));

  radio.openReadingPipe(1, address[1]);
  radio.startListening();

  radio.printDetails();

  // MCP
  SPI.begin();

  MCP.begin();
  delay(100);

  MCP.pinMode8(0b11000000);
  MCP.setPullup8(0b11000000);

  for (int pin = 0; pin < 8; pin++) {
    MCP.write1(pin, 0);
    delay(100);
  }
}

void loop() {
  static uint32_t last = 0;
  uint8_t pipe;

  if (radio.available(&pipe)) {
    uint8_t bytes = radio.getPayloadSize();
    radio.read(&payload, bytes);
    MCP.write8(payload);
  }

  if (MCP.read1(6) == 0) {  // UPPER BUTTON
    return;
  }


  if (MCP.read1(7) == 0) {  // LOWER BUTTON
    return;
  }
}
