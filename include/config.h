#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <driver/gpio.h>

// =============================================================================
// Phase Configuration
// =============================================================================
// Phase 1: Travel sensors on A0/A1, pressure transducers NOT connected (A2/A3)
// Phase 2: All 4 ADC channels connected
// Controls serial output only — all 4 channels are ALWAYS logged to SD.
#define DAQ_PHASE 1

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

// ADS1115 I2C ADC — travel sensors + pressure transducers
//   A0 = Travel sensor signal 1 (pushrod position — tapped from ECU pin 22)
//   A1 = Travel sensor signal 2 (pushrod position — tapped from ECU pin 2)
//   A2 = Front pressure transducer (Phase 2)
//   A3 = Rear pressure transducer (Phase 2)
// SDA=21, SCL=22 (default I2C)
static const int ADS_ADDR = 0x48;  // ADDR pin to GND

// BOOT button for hardware run trigger
static const int BOOT_BTN_PIN = 0;

// =============================================================================
// Configuration Constants
// =============================================================================

static const uint32_t SERIAL_BAUD    = 115200;
static const uint32_t CAN_SPEED_KBPS = 500;       // Both CAN buses
static const uint32_t ADC_RATE_HZ    = 100;        // ADC sample rate
static const uint32_t ADC_INTERVAL   = 1000 / ADC_RATE_HZ;  // ms

#endif // CONFIG_H
