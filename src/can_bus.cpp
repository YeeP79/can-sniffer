#include "can_bus.h"
#include "config.h"
#include "daq_state.h"
#include "utils.h"

#include <Arduino.h>
#include <driver/twai.h>

static CanFrameCallback log_cb = nullptr;

void can_bus_set_log_callback(CanFrameCallback cb) {
    log_cb = cb;
}

// =============================================================================
// TWAI — ESP32 built-in CAN controller (iBooster CAN bus #1)
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
        char buf[128];
        char data_str[32];
        format_can_data(msg.data, msg.data_length_code, data_str);

        if (ads_available) {
#if DAQ_PHASE >= 2
            snprintf(buf, sizeof(buf), "[CAN1] %03X [%d] %s| T1:%.3f T2:%.3f | F:%.1f R:%.1f",
                     msg.identifier, msg.data_length_code, data_str,
                     last_travel_v1, last_travel_v2, last_psi_front, last_psi_rear);
#else
            snprintf(buf, sizeof(buf), "[CAN1] %03X [%d] %s| T1:%.3f T2:%.3f",
                     msg.identifier, msg.data_length_code, data_str,
                     last_travel_v1, last_travel_v2);
#endif
        } else {
            snprintf(buf, sizeof(buf), "[CAN1] %03X [%d] %s",
                     msg.identifier, msg.data_length_code, data_str);
        }
        Serial.println(buf);

        if (logging_active && log_cb) {
            log_cb(1, msg.identifier, msg.data_length_code, msg.data);
        }
    }
}

// =============================================================================
// MCP2515 — SPI CAN controller (iBooster CAN bus #2)
// =============================================================================

void mcp_init() {
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

    while (!digitalRead(MCP_INT_PIN)) {
        long unsigned int id = 0;
        uint8_t len = 0;
        uint8_t data[8];

        if (mcp.readMsgBuf(&id, &len, data) == CAN_OK) {
            uint32_t can_id = id & 0x1FFFFFFF;

            char buf[128];
            char data_str[32];
            format_can_data(data, len, data_str);

            if (ads_available) {
#if DAQ_PHASE >= 2
                snprintf(buf, sizeof(buf), "[CAN2] %03X [%d] %s| T1:%.3f T2:%.3f | F:%.1f R:%.1f",
                         can_id, len, data_str,
                         last_travel_v1, last_travel_v2, last_psi_front, last_psi_rear);
#else
                snprintf(buf, sizeof(buf), "[CAN2] %03X [%d] %s| T1:%.3f T2:%.3f",
                         can_id, len, data_str,
                         last_travel_v1, last_travel_v2);
#endif
            } else {
                snprintf(buf, sizeof(buf), "[CAN2] %03X [%d] %s",
                         can_id, len, data_str);
            }
            Serial.println(buf);

            if (logging_active && log_cb) {
                log_cb(2, can_id, len, data);
            }
        } else {
            break;
        }
    }
}
