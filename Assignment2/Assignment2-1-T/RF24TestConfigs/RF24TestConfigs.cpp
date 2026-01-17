
#include "RF24TestConfigs.h"

// Keep channel fixed as requested.
// (In reality channel can matter due to Wi-Fi/interference, but we keep it
// constant here.)
static constexpr uint8_t FIXED_CH = 40;

static const RadioConfig PRESETS[] = {
    // --- Low ARD/ARC (fast, less robust) ---
    {"1M  PA_LOW  ARD0 ARC1", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 0, 1},
    {"1M  PA_LOW  ARD0 ARC3", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 0, 3},
    {"1M  PA_LOW  ARD1 ARC3", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 1, 3},

    // --- Mixed ARD/ARC (typical tradeoffs) ---
    {"1M  PA_LOW  ARD2 ARC7", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 2, 7},
    {"1M  PA_LOW  ARD5 ARC15", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 5, 15},

    // --- Different data rates (keep other params same-ish) ---
    {"250K PA_LOW  ARD5 ARC15", FIXED_CH, RF24_250KBPS, RF24_PA_LOW, 5, 15},
    {"2M   PA_LOW  ARD5 ARC15", FIXED_CH, RF24_2MBPS, RF24_PA_LOW, 5, 15},

    // --- Different PA levels (keep rate + retries same) ---
    {"1M  PA_MIN  ARD5 ARC15", FIXED_CH, RF24_1MBPS, RF24_PA_MIN, 5, 15},
    {"1M  PA_LOW  ARD5 ARC15", FIXED_CH, RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_HIGH ARD5 ARC15", FIXED_CH, RF24_1MBPS, RF24_PA_HIGH, 5, 15},
    {"1M  PA_MAX  ARD5 ARC15", FIXED_CH, RF24_1MBPS, RF24_PA_MAX, 5, 15},
};

size_t rf24cfg_count() { return sizeof(PRESETS) / sizeof(PRESETS[0]); }

const RadioConfig &rf24cfg_get(size_t idx) {
  return PRESETS[idx];
}

void rf24cfg_apply(RF24 &radio, const RadioConfig &cfg) {
  radio.stopListening();
  radio.flush_rx();
  radio.flush_tx();
  radio.clearStatusFlags();

  radio.setChannel(cfg.channel);
  radio.setDataRate(cfg.rate);
  radio.setPALevel(cfg.pa);
  radio.setRetries(cfg.ard, cfg.arc);

  delay(3);
}
