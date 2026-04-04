#include "serial_cmd.h"
#include "config.h"
#include "daq_state.h"
#include "sd_logger.h"
#include "utils.h"

#include <Arduino.h>

static RunStartCallback start_cb = nullptr;
static RunStopCallback  stop_cb  = nullptr;
static bool boot_btn_last        = HIGH;

void serial_cmd_set_run_callbacks(RunStartCallback start, RunStopCallback stop) {
    start_cb = start;
    stop_cb  = stop;
}

void serial_init() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) { delay(10); }

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Moon Patrol iBooster DAQ v2.0");
    Serial.println("  CAN Sniffer + Travel/Pressure Logger");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  NEW            — Start new log run");
    Serial.println("  RUN: <desc>    — Start run with description");
    Serial.println("  STOP           — Stop current run / travel stream");
    Serial.println("  STATUS         — Print system status");
    Serial.println("  TRAVEL         — Stream travel sensor voltages");
    Serial.println("  (BOOT button)  — Toggle run on/off");
    Serial.println();
}

void serial_loop() {
    if (!Serial.available()) return;

    String raw = Serial.readStringUntil('\n');
    ParsedCommand cmd = parse_serial_command(raw.c_str());

    switch (cmd.type) {
    case CMD_NEW:
        if (start_cb) start_cb(NULL);
        break;
    case CMD_RUN:
        if (start_cb) start_cb(cmd.description);
        break;
    case CMD_STOP:
        if (travel_streaming) {
            travel_streaming = false;
            Serial.println("[TRAVEL] Stopped");
        }
        if (logging_active) {
            if (stop_cb) stop_cb();
        }
        break;
    case CMD_TRAVEL:
        if (!ads_available) {
            Serial.println("[TRAVEL] ERROR: ADS1115 not available");
        } else {
            travel_streaming = true;
            Serial.println("[TRAVEL] Streaming travel sensor data (STOP to end)");
        }
        break;
    case CMD_STATUS:
        print_status();
        break;
    case CMD_UNKNOWN:
        raw.trim();
        if (raw.length() > 0) {
            Serial.print("Unknown command: ");
            Serial.println(raw);
        }
        break;
    }
}

void run_mgmt_init() {
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    boot_btn_last = digitalRead(BOOT_BTN_PIN);
}

void run_mgmt_loop() {
    bool btn = digitalRead(BOOT_BTN_PIN);
    if (boot_btn_last == HIGH && btn == LOW) {
        if (logging_active) {
            if (stop_cb) stop_cb();
        } else {
            if (start_cb) start_cb("button trigger");
        }
        delay(50);  // debounce
    }
    boot_btn_last = btn;
}

void print_status() {
    Serial.println();
    Serial.println("=== STATUS ===");
    Serial.printf("  CAN1 bus:     active (%lu kbps)\n", CAN_SPEED_KBPS);
    Serial.printf("  CAN2 bus:     %s\n", mcp_available ? "active" : "NOT FOUND");
    Serial.printf("  ADS1115:      %s\n", ads_available ? "active" : "NOT FOUND");
    Serial.printf("  SD Card:      %s\n", sd_available ? "active" : "NOT FOUND");
    Serial.printf("  Logging:      %s\n", logging_active ? "ACTIVE" : "stopped");
    Serial.printf("  Run number:   %03d\n", sd_get_run_number());
    if (logging_active) {
        Serial.printf("  Samples:      %lu\n", sd_get_sample_count());
        Serial.printf("  File size:    %lu bytes\n", sd_get_file_size());
    }
    if (ads_available) {
        Serial.printf("  Travel 1:     %.3fV\n", last_travel_v1);
        Serial.printf("  Travel 2:     %.3fV\n", last_travel_v2);
#if DAQ_PHASE >= 2
        Serial.printf("  Front PSI:    %.1f\n", last_psi_front);
        Serial.printf("  Rear PSI:     %.1f\n", last_psi_rear);
#else
        Serial.println("  Front PSI:    N/C (Phase 2)");
        Serial.println("  Rear PSI:     N/C (Phase 2)");
#endif
    }
    Serial.printf("  Uptime:       %lus\n", millis() / 1000);
    Serial.println("===============");
    Serial.println();
}
