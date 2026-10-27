#pragma once
#include <Arduino.h>
#include <RF24.h>

// A single radio configuration preset
struct RadioConfig {
  const char *name;     // label for logging
  uint8_t channel;      // 0..125
  rf24_datarate_e rate; // RF24_250KBPS / RF24_1MBPS / RF24_2MBPS
  rf24_pa_dbm_e pa;     // RF24_PA_MIN/LOW/HIGH/MAX
  uint8_t ard;          // 0..15 (retransmit delay steps of 250us)
  uint8_t arc;          // 0..15 (retransmit count)
};

// Access to presets
size_t rf24cfg_count();
const RadioConfig &rf24cfg_get(size_t idx);

// Apply helper
void rf24cfg_apply(RF24 &radio, const RadioConfig &cfg);
