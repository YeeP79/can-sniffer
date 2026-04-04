#ifndef DAQ_STATE_H
#define DAQ_STATE_H

#include <Arduino.h>
#include <mcp_can.h>
#include <Adafruit_ADS1X15.h>

// Shared state — defined in main.cpp
extern bool     ads_available;
extern bool     mcp_available;
extern bool     sd_available;
extern bool     logging_active;
extern bool     travel_streaming;
extern float    last_travel_v1;
extern float    last_travel_v2;
extern float    last_psi_front;
extern float    last_psi_rear;

// Hardware objects — defined in main.cpp
extern MCP_CAN          mcp;
extern Adafruit_ADS1115 ads;

#endif // DAQ_STATE_H
