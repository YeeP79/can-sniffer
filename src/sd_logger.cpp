#include "sd_logger.h"
#include "config.h"
#include "daq_state.h"
#include "utils.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

static File     log_file;
static uint16_t run_number   = 0;
static uint32_t sample_count = 0;

static uint16_t find_next_run_number() {
    uint16_t num = 1;
    char filename[20];
    while (num < 999) {
        snprintf(filename, sizeof(filename), "/run_%03d.csv", num);
        if (!SD.exists(filename)) break;
        num++;
    }
    return num;
}

void sd_init() {
    // Deselect MCP2515 before initializing SD
    pinMode(MCP_CS_PIN, OUTPUT);
    digitalWrite(MCP_CS_PIN, HIGH);

    if (SD.begin(SD_CS_PIN)) {
        Serial.println("[SD] Card initialized");
        sd_available = true;
        run_number = find_next_run_number();
        Serial.printf("[SD] Next run number: %03d\n", run_number);
    } else {
        Serial.println("[SD] ERROR: Card init failed — check wiring");
        sd_available = false;
    }
}

void sd_start_run(const char* description) {
    if (!sd_available) {
        Serial.println("[RUN] ERROR: SD card not available");
        return;
    }
    if (logging_active) {
        Serial.println("[RUN] Stopping current run first...");
        sd_stop_run();
    }

    char filename[20];
    snprintf(filename, sizeof(filename), "/run_%03d.csv", run_number);

    log_file = SD.open(filename, FILE_WRITE);
    if (!log_file) {
        Serial.printf("[RUN] ERROR: Could not create %s\n", filename);
        return;
    }

    // Write header
    if (description && strlen(description) > 0) {
        log_file.printf("# %s\n", description);
    }
    log_file.println("timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear");
    log_file.flush();

    logging_active = true;
    sample_count = 0;

    Serial.printf("[RUN] Started run %03d → %s\n", run_number, filename);
    if (description && strlen(description) > 0) {
        Serial.printf("[RUN] Description: %s\n", description);
    }
}

void sd_stop_run() {
    if (!logging_active) {
        Serial.println("[RUN] No active run");
        return;
    }

    log_file.flush();
    log_file.close();
    logging_active = false;

    Serial.printf("[RUN] Stopped run %03d — %lu samples logged\n",
                  run_number, sample_count);
    run_number++;
}

void sd_log_can(uint8_t bus, uint32_t id, uint8_t len, const uint8_t* data) {
    if (!log_file) return;

    char buf[128];
    format_can_csv_row(millis(), bus, id, data, len, buf, sizeof(buf));
    log_file.print(buf);
    sample_count++;

    // Flush periodically (every 100 samples)
    if (sample_count % 100 == 0) {
        log_file.flush();
    }
}

void sd_log_adc(float travel_v1, float travel_v2, float psi_front, float psi_rear) {
    if (!log_file) return;

    char buf[128];
    format_adc_csv_row(millis(), travel_v1, travel_v2, psi_front, psi_rear, buf, sizeof(buf));
    log_file.print(buf);
    sample_count++;

    if (sample_count % 100 == 0) {
        log_file.flush();
    }
}

uint16_t sd_get_run_number()   { return run_number; }
uint32_t sd_get_sample_count() { return sample_count; }
uint32_t sd_get_file_size()    { return log_file ? log_file.size() : 0; }
