
#include "RF24TestConfigs.h"

// Keep channel fixed as requested.
// (In reality channel can matter due to Wi-Fi/interference, but we keep it
// constant here.)
// constant here.)
static constexpr uint8_t CH_WIFI1 = 12;   // 2412 MHz (WiFi ch1 center)
static constexpr uint8_t CH_WIFI6 = 37;   // 2437 MHz (WiFi ch6 center)
static constexpr uint8_t CH_WIFI11 = 62;   // 2462 MHz (WiFi ch11 center)

static constexpr uint8_t CH_BLE_ADV1 = 2;    // 2402 MHz (BLE adv)
static constexpr uint8_t CH_BLE_ADV2 = 26;   // 2426 MHz (BLE adv)
static constexpr uint8_t CH_BLE_ADV3 = 80;   // 2480 MHz (BLE adv)

static constexpr uint8_t CH_BOTH = 37;   // overlap-ish (WiFi ch6 area + BLE traffic around)
static constexpr uint8_t CH_NEITHER = 100;  // 2500 MHz (above WiFi/BLE common band)
static constexpr uint8_t CH_RANDOM = 40;   // random mid band (your old fixed-ish)

// NOTE: target ~50 presets total
static const RadioConfig PRESETS[] = {
    // --- Rate sweep (5 each) @ PA_LOW, CH_NEITHER ---
    {"250K PA_LOW CH_NEITHER ARD0 ARC1",  CH_NEITHER, RF24_250KBPS, RF24_PA_LOW, 0, 1},
    {"250K PA_LOW CH_NEITHER ARD0 ARC3",  CH_NEITHER, RF24_250KBPS, RF24_PA_LOW, 0, 3},
    {"250K PA_LOW CH_NEITHER ARD1 ARC3",  CH_NEITHER, RF24_250KBPS, RF24_PA_LOW, 1, 3},
    {"250K PA_LOW CH_NEITHER ARD2 ARC7",  CH_NEITHER, RF24_250KBPS, RF24_PA_LOW, 2, 7},
    {"250K PA_LOW CH_NEITHER ARD5 ARC15", CH_NEITHER, RF24_250KBPS, RF24_PA_LOW, 5, 15},

    {"1M  PA_LOW CH_NEITHER ARD0 ARC1",   CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW, 0, 1},
    {"1M  PA_LOW CH_NEITHER ARD0 ARC3",   CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW, 0, 3},
    {"1M  PA_LOW CH_NEITHER ARD1 ARC3",   CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW, 1, 3},
    {"1M  PA_LOW CH_NEITHER ARD2 ARC7",   CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW, 2, 7},
    {"1M  PA_LOW CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW, 5, 15},

    {"2M  PA_LOW CH_NEITHER ARD0 ARC1",   CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW, 0, 1},
    {"2M  PA_LOW CH_NEITHER ARD0 ARC3",   CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW, 0, 3},
    {"2M  PA_LOW CH_NEITHER ARD1 ARC3",   CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW, 1, 3},
    {"2M  PA_LOW CH_NEITHER ARD2 ARC7",   CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW, 2, 7},
    {"2M  PA_LOW CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW, 5, 15},

    // --- PA sweep (all PA) across all rates @ ARD5 ARC15, CH_NEITHER ---
    {"250K PA_MIN  CH_NEITHER ARD5 ARC15", CH_NEITHER, RF24_250KBPS, RF24_PA_MIN,  5, 15},
    {"250K PA_LOW  CH_NEITHER ARD5 ARC15", CH_NEITHER, RF24_250KBPS, RF24_PA_LOW,  5, 15},
    {"250K PA_HIGH CH_NEITHER ARD5 ARC15", CH_NEITHER, RF24_250KBPS, RF24_PA_HIGH, 5, 15},
    {"250K PA_MAX  CH_NEITHER ARD5 ARC15", CH_NEITHER, RF24_250KBPS, RF24_PA_MAX,  5, 15},

    {"1M  PA_MIN  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_1MBPS,   RF24_PA_MIN,  5, 15},
    {"1M  PA_LOW  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_1MBPS,   RF24_PA_LOW,  5, 15},
    {"1M  PA_HIGH CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_1MBPS,   RF24_PA_HIGH, 5, 15},
    {"1M  PA_MAX  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_1MBPS,   RF24_PA_MAX,  5, 15},

    {"2M  PA_MIN  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_2MBPS,   RF24_PA_MIN,  5, 15},
    {"2M  PA_LOW  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_2MBPS,   RF24_PA_LOW,  5, 15},
    {"2M  PA_HIGH CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_2MBPS,   RF24_PA_HIGH, 5, 15},
    {"2M  PA_MAX  CH_NEITHER ARD5 ARC15",  CH_NEITHER, RF24_2MBPS,   RF24_PA_MAX,  5, 15},

    // --- Retry/Delay sweep (more ARD/ARC combos) @ 1M, PA_LOW, CH_NEITHER ---
    {"1M  PA_LOW CH_NEITHER ARD0 ARC5",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 0, 5},
    {"1M  PA_LOW CH_NEITHER ARD0 ARC10",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 0, 10},
    {"1M  PA_LOW CH_NEITHER ARD0 ARC15",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 0, 15},

    {"1M  PA_LOW CH_NEITHER ARD1 ARC1",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 1, 1},
    {"1M  PA_LOW CH_NEITHER ARD1 ARC7",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 1, 7},
    {"1M  PA_LOW CH_NEITHER ARD1 ARC15",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 1, 15},

    {"1M  PA_LOW CH_NEITHER ARD2 ARC1",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 2, 1},
    {"1M  PA_LOW CH_NEITHER ARD2 ARC3",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 2, 3},
    {"1M  PA_LOW CH_NEITHER ARD2 ARC15",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 2, 15},

    {"1M  PA_LOW CH_NEITHER ARD5 ARC1",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 5, 1},
    {"1M  PA_LOW CH_NEITHER ARD5 ARC3",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 5, 3},
    {"1M  PA_LOW CH_NEITHER ARD5 ARC7",    CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 5, 7},

    {"1M  PA_LOW CH_NEITHER ARD10 ARC3",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 10, 3},
    {"1M  PA_LOW CH_NEITHER ARD10 ARC7",   CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 10, 7},
    {"1M  PA_LOW CH_NEITHER ARD15 ARC15",  CH_NEITHER, RF24_1MBPS, RF24_PA_LOW, 15, 15},

    // --- Channel sweep (WiFi/BLE/both/neither/random) @ 1M, PA_LOW, ARD5 ARC15 ---
    {"1M  PA_LOW CH_WIFI1  ARD5 ARC15",    CH_WIFI1,   RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_WIFI6  ARD5 ARC15",    CH_WIFI6,   RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_WIFI11 ARD5 ARC15",    CH_WIFI11,  RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_BLE1   ARD5 ARC15",    CH_BLE_ADV1,RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_BLE2   ARD5 ARC15",    CH_BLE_ADV2,RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_BLE3   ARD5 ARC15",    CH_BLE_ADV3,RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_BOTH   ARD5 ARC15",    CH_BOTH,    RF24_1MBPS, RF24_PA_LOW, 5, 15},
    {"1M  PA_LOW CH_RANDOM ARD5 ARC15",    CH_RANDOM,  RF24_1MBPS, RF24_PA_LOW, 5, 15},
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
