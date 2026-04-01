// Moon Patrol iBooster DAQ — ESP32 CAN Sniffer + Pressure Logger
// Sniffs two CAN buses (TWAI + MCP2515), reads two pressure transducers
// (ADS1115), logs everything to SD card CSV files.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <driver/twai.h>
#include <mcp_can.h>
#include <Adafruit_ADS1X15.h>

// =============================================================================
// Pin Definitions
// =============================================================================

// TWAI (ESP32 built-in CAN) — iBooster CAN bus #1
static const gpio_num_t TWAI_TX_PIN = GPIO_NUM_4;
static const gpio_num_t TWAI_RX_PIN = GPIO_NUM_5;

// MCP2515 SPI CAN controller — iBooster CAN bus #2
static const int MCP_CS_PIN  = 15;
static const int MCP_INT_PIN = 2;
// SPI: MOSI=23, MISO=19, SCK=18 (default VSPI)

// SD Card (shared SPI bus, different CS)
static const int SD_CS_PIN = 16;

// ADS1115 I2C ADC — pressure transducers
// SDA=21, SCL=22 (default I2C)
static const int ADS_ADDR = 0x48;  // ADDR pin to GND

// BOOT button for hardware run trigger
static const int BOOT_BTN_PIN = 0;

// =============================================================================
// Configuration Constants
// =============================================================================

static const uint32_t SERIAL_BAUD       = 115200;
static const uint32_t CAN_SPEED_KBPS    = 500;       // Both CAN buses
static const uint32_t PRESSURE_RATE_HZ  = 100;       // ADC sample rate
static const uint32_t PRESSURE_INTERVAL = 1000 / PRESSURE_RATE_HZ;  // ms

// Pressure transducer calibration: 0.5-4.5V → 0-3000 PSI
// ADS1115 at gain 1 (±4.096V): 1 bit = 0.125 mV
static const float PRESSURE_V_MIN   = 0.5f;
static const float PRESSURE_V_MAX   = 4.5f;
static const float PRESSURE_PSI_MIN = 0.0f;
static const float PRESSURE_PSI_MAX = 3000.0f;

// =============================================================================
// Global Objects
// =============================================================================

MCP_CAN mcp(MCP_CS_PIN);
Adafruit_ADS1115 ads;

// =============================================================================
// Run Management State
// =============================================================================

static bool     logging_active  = false;
static File     log_file;
static uint16_t run_number      = 0;
static uint32_t sample_count    = 0;
static bool     ads_available   = false;
static bool     mcp_available   = false;
static bool     sd_available    = false;
static bool     boot_btn_last   = HIGH;
static uint32_t last_pressure_ms = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

void serial_init();
void serial_loop();
void twai_init();
void twai_loop();
void mcp_init();
void mcp_loop();
void ads_init();
void ads_loop();
void sd_init();
void sd_start_run(const char* description);
void sd_stop_run();
void sd_log_can(uint8_t bus, uint32_t id, uint8_t len, const uint8_t* data);
void sd_log_pressure(float psi_front, float psi_rear);
void run_mgmt_init();
void run_mgmt_loop();
void print_status();
uint16_t find_next_run_number();
float voltage_to_psi(float voltage);
void format_can_data(const uint8_t* data, uint8_t len, char* buf);

// =============================================================================
// 1. Serial — USB serial for monitoring and run management
// =============================================================================

void serial_init() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) { delay(10); }

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Moon Patrol iBooster DAQ v1.0");
    Serial.println("  CAN Sniffer + Pressure Logger");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  NEW            — Start new log run");
    Serial.println("  RUN: <desc>    — Start run with description");
    Serial.println("  STOP           — Stop current run");
    Serial.println("  STATUS         — Print system status");
    Serial.println("  (BOOT button)  — Toggle run on/off");
    Serial.println();
}

void serial_loop() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("NEW")) {
        sd_start_run(NULL);
    } else if (cmd.startsWith("RUN:") || cmd.startsWith("run:")) {
        String desc = cmd.substring(4);
        desc.trim();
        sd_start_run(desc.c_str());
    } else if (cmd.equalsIgnoreCase("STOP")) {
        sd_stop_run();
    } else if (cmd.equalsIgnoreCase("STATUS")) {
        print_status();
    } else {
        Serial.print("Unknown command: ");
        Serial.println(cmd);
    }
}

// =============================================================================
// 2. TWAI — ESP32 built-in CAN controller (iBooster CAN bus #1)
// =============================================================================

void twai_init() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_PIN, TWAI_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    g_config.rx_queue_len = 32;

    twai_timing_config_t t_config;
    if (CAN_SPEED_KBPS == 500) {
        t_config = TWAI_TIMING_CONFIG_500KBITS();
    } else if (CAN_SPEED_KBPS == 250) {
        t_config = TWAI_TIMING_CONFIG_250KBITS();
    } else if (CAN_SPEED_KBPS == 1000) {
        t_config = TWAI_TIMING_CONFIG_1MBITS();
    } else {
        t_config = TWAI_TIMING_CONFIG_500KBITS();
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("[TWAI] Driver installed");
    } else {
        Serial.println("[TWAI] ERROR: Driver install failed");
        return;
    }

    if (twai_start() == ESP_OK) {
        Serial.println("[TWAI] Started — listen-only mode");
    } else {
        Serial.println("[TWAI] ERROR: Start failed");
    }
}

void twai_loop() {
    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
        // Echo to serial
        char buf[64];
        char data_str[32];
        format_can_data(msg.data, msg.data_length_code, data_str);
        snprintf(buf, sizeof(buf), "[CAN1] %03X [%d] %s",
                 msg.identifier, msg.data_length_code, data_str);
        Serial.println(buf);

        // Log to SD
        if (logging_active) {
            sd_log_can(1, msg.identifier, msg.data_length_code, msg.data);
        }
    }
}

// =============================================================================
// 3. MCP2515 — SPI CAN controller (iBooster CAN bus #2)
// =============================================================================

void mcp_init() {
    // MCP2515 uses the default VSPI bus (shared with SD card)
    // CS pin differentiates the two devices
    if (mcp.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        Serial.println("[MCP2515] Initialized");
        mcp.setMode(MCP_LISTENONLY);
        Serial.println("[MCP2515] Listen-only mode");
        mcp_available = true;
    } else {
        Serial.println("[MCP2515] ERROR: Init failed — check wiring");
        mcp_available = false;
    }

    pinMode(MCP_INT_PIN, INPUT);
}

void mcp_loop() {
    if (!mcp_available) return;

    // Check for received frames (poll INT pin or check directly)
    while (!digitalRead(MCP_INT_PIN)) {
        long unsigned int id = 0;
        uint8_t len = 0;
        uint8_t data[8];

        if (mcp.readMsgBuf(&id, &len, data) == CAN_OK) {
            // Mask off extended frame flag if present
            uint32_t can_id = id & 0x1FFFFFFF;

            char buf[64];
            char data_str[32];
            format_can_data(data, len, data_str);
            snprintf(buf, sizeof(buf), "[CAN2] %03X [%d] %s",
                     can_id, len, data_str);
            Serial.println(buf);

            if (logging_active) {
                sd_log_can(2, can_id, len, data);
            }
        } else {
            break;
        }
    }
}

// =============================================================================
// 4. ADS1115 — I2C 16-bit ADC for pressure transducers
// =============================================================================

void ads_init() {
    Wire.begin(21, 22);

    if (ads.begin(ADS_ADDR, &Wire)) {
        Serial.println("[ADS1115] Initialized at 0x48");
        ads.setGain(GAIN_ONE);  // ±4.096V range
        ads.setDataRate(RATE_ADS1115_128SPS);
        ads_available = true;
    } else {
        Serial.println("[ADS1115] Not found — pressure readings disabled");
        ads_available = false;
    }
}

void ads_loop() {
    if (!ads_available) return;

    uint32_t now = millis();
    if (now - last_pressure_ms < PRESSURE_INTERVAL) return;
    last_pressure_ms = now;

    float v0 = ads.computeVolts(ads.readADC_SingleEnded(0));
    float v1 = ads.computeVolts(ads.readADC_SingleEnded(1));

    float psi_front = voltage_to_psi(v0);
    float psi_rear  = voltage_to_psi(v1);

    // Echo to serial
    char buf[80];
    snprintf(buf, sizeof(buf), "[PSI] front=%.1f rear=%.1f (%.3fV, %.3fV)",
             psi_front, psi_rear, v0, v1);
    Serial.println(buf);

    // Log to SD
    if (logging_active) {
        sd_log_pressure(psi_front, psi_rear);
    }
}

float voltage_to_psi(float voltage) {
    if (voltage <= PRESSURE_V_MIN) return PRESSURE_PSI_MIN;
    if (voltage >= PRESSURE_V_MAX) return PRESSURE_PSI_MAX;
    return (voltage - PRESSURE_V_MIN) / (PRESSURE_V_MAX - PRESSURE_V_MIN)
           * (PRESSURE_PSI_MAX - PRESSURE_PSI_MIN) + PRESSURE_PSI_MIN;
}

// =============================================================================
// 5. SD Card — SPI logging to CSV
// =============================================================================

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

uint16_t find_next_run_number() {
    uint16_t num = 1;
    char filename[20];
    while (num < 999) {
        snprintf(filename, sizeof(filename), "/run_%03d.csv", num);
        if (!SD.exists(filename)) break;
        num++;
    }
    return num;
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
    log_file.println("timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,psi_front,psi_rear");
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

    log_file.printf("%lu,CAN%d,0x%03X", millis(), bus, id);
    for (int i = 0; i < 8; i++) {
        if (i < len) {
            log_file.printf(",%02X", data[i]);
        } else {
            log_file.print(",");
        }
    }
    log_file.println(",,");  // empty pressure fields
    sample_count++;

    // Flush periodically (every 100 samples)
    if (sample_count % 100 == 0) {
        log_file.flush();
    }
}

void sd_log_pressure(float psi_front, float psi_rear) {
    if (!log_file) return;

    log_file.printf("%lu,PSI,,,,,,,,,%.1f,%.1f\n",
                    millis(), psi_front, psi_rear);
    sample_count++;

    if (sample_count % 100 == 0) {
        log_file.flush();
    }
}

// =============================================================================
// 6. Run Management — serial commands + BOOT button
// =============================================================================

void run_mgmt_init() {
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    boot_btn_last = digitalRead(BOOT_BTN_PIN);
}

void run_mgmt_loop() {
    // BOOT button: toggle run on falling edge
    bool btn = digitalRead(BOOT_BTN_PIN);
    if (boot_btn_last == HIGH && btn == LOW) {
        if (logging_active) {
            sd_stop_run();
        } else {
            sd_start_run("button trigger");
        }
        delay(50);  // debounce
    }
    boot_btn_last = btn;
}

void print_status() {
    Serial.println();
    Serial.println("--- System Status ---");
    Serial.printf("  TWAI (CAN1):  active\n");
    Serial.printf("  MCP2515 (CAN2): %s\n", mcp_available ? "active" : "NOT FOUND");
    Serial.printf("  ADS1115:      %s\n", ads_available ? "active" : "NOT FOUND");
    Serial.printf("  SD Card:      %s\n", sd_available ? "active" : "NOT FOUND");
    Serial.printf("  Logging:      %s\n", logging_active ? "ACTIVE" : "stopped");
    Serial.printf("  Run number:   %03d\n", run_number);
    if (logging_active) {
        Serial.printf("  Samples:      %lu\n", sample_count);
        Serial.printf("  File size:    %lu bytes\n", log_file.size());
    }
    Serial.println("---------------------");
    Serial.println();
}

// =============================================================================
// Utilities
// =============================================================================

void format_can_data(const uint8_t* data, uint8_t len, char* buf) {
    buf[0] = '\0';
    for (int i = 0; i < len; i++) {
        sprintf(buf + strlen(buf), "%02X ", data[i]);
    }
}

// =============================================================================
// Arduino Entry Points
// =============================================================================

void setup() {
    serial_init();
    twai_init();
    mcp_init();
    ads_init();
    sd_init();
    run_mgmt_init();

    Serial.println("[SETUP] All subsystems initialized");
    print_status();
}

void loop() {
    twai_loop();
    mcp_loop();
    ads_loop();
    serial_loop();
    run_mgmt_loop();
}
