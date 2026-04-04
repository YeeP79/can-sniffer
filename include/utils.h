#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <cstdio>
#include <cstring>

// Pressure transducer calibration: 0.5-4.5V -> 0-3000 PSI
// ADS1115 at gain 1 (+/-4.096V): 1 bit = 0.125 mV
static const float PRESSURE_V_MIN   = 0.5f;
static const float PRESSURE_V_MAX   = 4.5f;
static const float PRESSURE_PSI_MIN = 0.0f;
static const float PRESSURE_PSI_MAX = 3000.0f;

inline float voltage_to_psi(float voltage) {
    if (voltage <= PRESSURE_V_MIN) return PRESSURE_PSI_MIN;
    if (voltage >= PRESSURE_V_MAX) return PRESSURE_PSI_MAX;
    return (voltage - PRESSURE_V_MIN) / (PRESSURE_V_MAX - PRESSURE_V_MIN)
           * (PRESSURE_PSI_MAX - PRESSURE_PSI_MIN) + PRESSURE_PSI_MIN;
}

inline void format_can_data(const uint8_t* data, uint8_t len, char* buf) {
    buf[0] = '\0';
    // CAN frames are at most 8 bytes; cap to prevent buffer overflow
    if (len > 8) len = 8;
    // Max output: 8 bytes * 3 chars ("XX ") + null = 25 chars
    int pos = 0;
    for (int i = 0; i < len; i++) {
        pos += snprintf(buf + pos, 4, "%02X ", data[i]);
    }
}

// =========================================================================
// CSV row formatters (pure, Arduino-free — testable on native platform)
// =========================================================================

// Format a CAN bus data row as CSV.
// Produces: timestamp_ms,CAN<bus>,0x<id>,d0,d1,...,d7,,,, \n
// Returns number of chars written (excluding null terminator).
inline int format_can_csv_row(uint32_t timestamp_ms, uint8_t bus, uint32_t can_id,
                              const uint8_t* data, uint8_t len,
                              char* buf, size_t buf_size) {
    int pos = snprintf(buf, buf_size, "%lu,CAN%d,0x%03X", (unsigned long)timestamp_ms, bus, can_id);
    for (int i = 0; i < 8; i++) {
        if (i < len) {
            pos += snprintf(buf + pos, buf_size - pos, ",%02X", data[i]);
        } else {
            pos += snprintf(buf + pos, buf_size - pos, ",");
        }
    }
    pos += snprintf(buf + pos, buf_size - pos, ",,,,\n");
    return pos;
}

// Format an ADC sensor row as CSV.
// Produces: timestamp_ms,ADC,,,,,,,,,,travel_v1,travel_v2,psi_front,psi_rear\n
// The 10 commas between ADC and travel_v1 span the 9 empty fields (can_id + d0-d7).
// Returns number of chars written (excluding null terminator).
inline int format_adc_csv_row(uint32_t timestamp_ms, float travel_v1, float travel_v2,
                              float psi_front, float psi_rear,
                              char* buf, size_t buf_size) {
    return snprintf(buf, buf_size, "%lu,ADC,,,,,,,,,,%.3f,%.3f,%.1f,%.1f\n",
                    (unsigned long)timestamp_ms, travel_v1, travel_v2, psi_front, psi_rear);
}

// =========================================================================
// Serial command parser (pure, Arduino-free)
// =========================================================================

enum CommandType { CMD_UNKNOWN, CMD_NEW, CMD_RUN, CMD_STOP, CMD_TRAVEL, CMD_STATUS };

struct ParsedCommand {
    CommandType type;
    char description[128];
};

inline ParsedCommand parse_serial_command(const char* input) {
    ParsedCommand cmd;
    cmd.type = CMD_UNKNOWN;
    cmd.description[0] = '\0';

    if (!input) return cmd;

    // Skip leading whitespace
    while (*input == ' ' || *input == '\t' || *input == '\r' || *input == '\n') input++;

    // Find the end (trim trailing whitespace)
    size_t slen = strlen(input);
    while (slen > 0 && (input[slen - 1] == ' ' || input[slen - 1] == '\t'
                        || input[slen - 1] == '\r' || input[slen - 1] == '\n')) {
        slen--;
    }

    if (slen == 0) return cmd;

    // Check for RUN: prefix (case insensitive) — must check before simple keyword match
    if (slen >= 4 && (input[0] == 'R' || input[0] == 'r')
                   && (input[1] == 'U' || input[1] == 'u')
                   && (input[2] == 'N' || input[2] == 'n')
                   && input[3] == ':') {
        cmd.type = CMD_RUN;
        const char* desc = input + 4;
        // Skip whitespace after colon
        while (*desc == ' ' || *desc == '\t') desc++;
        // Calculate remaining length within trimmed bounds
        size_t desc_start = desc - input;
        if (desc_start < slen) {
            size_t desc_len = slen - desc_start;
            if (desc_len >= sizeof(cmd.description)) desc_len = sizeof(cmd.description) - 1;
            memcpy(cmd.description, desc, desc_len);
            cmd.description[desc_len] = '\0';
        }
        return cmd;
    }

    // Simple keyword commands (case insensitive)
    if (slen == 3 && (input[0] == 'N' || input[0] == 'n')
                  && (input[1] == 'E' || input[1] == 'e')
                  && (input[2] == 'W' || input[2] == 'w')) {
        cmd.type = CMD_NEW;
    } else if (slen == 4 && (input[0] == 'S' || input[0] == 's')
                          && (input[1] == 'T' || input[1] == 't')
                          && (input[2] == 'O' || input[2] == 'o')
                          && (input[3] == 'P' || input[3] == 'p')) {
        cmd.type = CMD_STOP;
    } else if (slen == 6 && (input[0] == 'T' || input[0] == 't')
                          && (input[1] == 'R' || input[1] == 'r')
                          && (input[2] == 'A' || input[2] == 'a')
                          && (input[3] == 'V' || input[3] == 'v')
                          && (input[4] == 'E' || input[4] == 'e')
                          && (input[5] == 'L' || input[5] == 'l')) {
        cmd.type = CMD_TRAVEL;
    } else if (slen == 6 && (input[0] == 'S' || input[0] == 's')
                          && (input[1] == 'T' || input[1] == 't')
                          && (input[2] == 'A' || input[2] == 'a')
                          && (input[3] == 'T' || input[3] == 't')
                          && (input[4] == 'U' || input[4] == 'u')
                          && (input[5] == 'S' || input[5] == 's')) {
        cmd.type = CMD_STATUS;
    }

    return cmd;
}

#endif // UTILS_H
