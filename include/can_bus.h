#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <cstdint>

using CanFrameCallback = void(*)(uint8_t bus, uint32_t id, uint8_t len, const uint8_t* data);

void can_bus_set_log_callback(CanFrameCallback cb);
void twai_init();
void twai_loop();
void mcp_init();
void mcp_loop();

#endif // CAN_BUS_H
