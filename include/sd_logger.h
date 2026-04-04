#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <cstdint>

void sd_init();
void sd_start_run(const char* description);
void sd_stop_run();
void sd_log_can(uint8_t bus, uint32_t id, uint8_t len, const uint8_t* data);
void sd_log_adc(float travel_v1, float travel_v2, float psi_front, float psi_rear);

uint16_t sd_get_run_number();
uint32_t sd_get_sample_count();
uint32_t sd_get_file_size();

#endif // SD_LOGGER_H
