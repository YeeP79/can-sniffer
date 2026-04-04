// Moon Patrol iBooster DAQ — ESP32 CAN Sniffer + Travel/Pressure Logger
// Composition root: wires modules together via callbacks (DIP).

#include "config.h"
#include "daq_state.h"
#include "can_bus.h"
#include "adc_sensors.h"
#include "sd_logger.h"
#include "serial_cmd.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// =============================================================================
// Global State (declared extern in daq_state.h)
// =============================================================================

bool     ads_available    = false;
bool     mcp_available    = false;
bool     sd_available     = false;
bool     logging_active   = false;
bool     travel_streaming = false;
float    last_travel_v1   = 0.0f;
float    last_travel_v2   = 0.0f;
float    last_psi_front   = 0.0f;
float    last_psi_rear    = 0.0f;

// =============================================================================
// Hardware Objects (declared extern in daq_state.h)
// =============================================================================

MCP_CAN          mcp(MCP_CS_PIN);
Adafruit_ADS1115 ads;

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

    // Wire dependencies (DIP) — producers don't know about consumers
    can_bus_set_log_callback(sd_log_can);
    adc_set_log_callback(sd_log_adc);
    serial_cmd_set_run_callbacks(sd_start_run, sd_stop_run);

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
