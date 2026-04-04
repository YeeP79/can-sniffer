#include "adc_sensors.h"
#include "config.h"
#include "daq_state.h"
#include "utils.h"

#include <Arduino.h>
#include <Wire.h>

static AdcCallback log_cb     = nullptr;
static uint32_t    last_adc_ms = 0;

void adc_set_log_callback(AdcCallback cb) {
    log_cb = cb;
}

void ads_init() {
    Wire.begin(21, 22);

    if (ads.begin(ADS_ADDR, &Wire)) {
        Serial.println("[ADS1115] Initialized at 0x48");
        ads.setGain(GAIN_ONE);              // ±4.096V range
        ads.setDataRate(RATE_ADS1115_860SPS); // 860 SPS for faster 4-channel reads
        ads_available = true;
        Serial.println("[ADS1115] A0/A1=travel sensors, A2/A3=pressure transducers");
    } else {
        Serial.println("[ADS1115] Not found — ADC readings disabled");
        ads_available = false;
    }
}

void ads_loop() {
    if (!ads_available) return;

    uint32_t now = millis();
    if (now - last_adc_ms < ADC_INTERVAL) return;
    last_adc_ms = now;

    // Read all 4 channels
    last_travel_v1 = ads.computeVolts(ads.readADC_SingleEnded(0));
    last_travel_v2 = ads.computeVolts(ads.readADC_SingleEnded(1));
    float v2       = ads.computeVolts(ads.readADC_SingleEnded(2));
    float v3       = ads.computeVolts(ads.readADC_SingleEnded(3));
    last_psi_front = voltage_to_psi(v2);
    last_psi_rear  = voltage_to_psi(v3);

    // Serial output
    if (travel_streaming) {
        char buf[40];
        snprintf(buf, sizeof(buf), "T1: %.3fV  T2: %.3fV", last_travel_v1, last_travel_v2);
        Serial.println(buf);
    } else {
        char buf[80];
#if DAQ_PHASE >= 2
        snprintf(buf, sizeof(buf), "[ADC] T1=%.3fV T2=%.3fV | F=%.1f R=%.1f PSI",
                 last_travel_v1, last_travel_v2, last_psi_front, last_psi_rear);
#else
        snprintf(buf, sizeof(buf), "[ADC] T1=%.3fV T2=%.3fV",
                 last_travel_v1, last_travel_v2);
#endif
        Serial.println(buf);
    }

    // Log to SD (always all 4 channels regardless of phase)
    if (logging_active && log_cb) {
        log_cb(last_travel_v1, last_travel_v2, last_psi_front, last_psi_rear);
    }
}
