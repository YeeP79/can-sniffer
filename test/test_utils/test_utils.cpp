#include <unity.h>
#include "utils.h"
#include <cmath>

// =========================================================================
// voltage_to_psi tests
// =========================================================================

void test_voltage_to_psi_at_min_boundary(void) {
    // 0.5V should map to 0 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, voltage_to_psi(0.5f));
}

void test_voltage_to_psi_at_max_boundary(void) {
    // 4.5V should map to 3000 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, voltage_to_psi(4.5f));
}

void test_voltage_to_psi_clamps_below_min(void) {
    // Below 0.5V should clamp to 0 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, voltage_to_psi(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, voltage_to_psi(0.3f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, voltage_to_psi(-1.0f));
}

void test_voltage_to_psi_clamps_above_max(void) {
    // Above 4.5V should clamp to 3000 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, voltage_to_psi(5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, voltage_to_psi(10.0f));
}

void test_voltage_to_psi_midpoint(void) {
    // 2.5V is midpoint of 0.5-4.5V range -> 1500 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1500.0f, voltage_to_psi(2.5f));
}

void test_voltage_to_psi_quarter_point(void) {
    // 1.5V is 25% of range -> 750 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 750.0f, voltage_to_psi(1.5f));
}

void test_voltage_to_psi_three_quarter_point(void) {
    // 3.5V is 75% of range -> 2250 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2250.0f, voltage_to_psi(3.5f));
}

void test_voltage_to_psi_typical_operating_point(void) {
    // 1.8V -> (1.8-0.5)/(4.5-0.5) * 3000 = 975 PSI
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 975.0f, voltage_to_psi(1.8f));
}

// =========================================================================
// format_can_data tests
// =========================================================================

void test_format_can_data_empty_frame(void) {
    char buf[64];
    uint8_t data[] = {};
    format_can_data(data, 0, buf);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_format_can_data_single_byte(void) {
    char buf[64];
    uint8_t data[] = {0xAB};
    format_can_data(data, 1, buf);
    TEST_ASSERT_EQUAL_STRING("AB ", buf);
}

void test_format_can_data_full_frame(void) {
    char buf[64];
    uint8_t data[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    format_can_data(data, 8, buf);
    TEST_ASSERT_EQUAL_STRING("00 11 22 33 44 55 66 77 ", buf);
}

void test_format_can_data_mixed_values(void) {
    char buf[64];
    uint8_t data[] = {0xFF, 0x00, 0xA0, 0x0B};
    format_can_data(data, 4, buf);
    TEST_ASSERT_EQUAL_STRING("FF 00 A0 0B ", buf);
}

void test_format_can_data_three_bytes(void) {
    char buf[64];
    uint8_t data[] = {0xDE, 0xAD, 0xBE};
    format_can_data(data, 3, buf);
    TEST_ASSERT_EQUAL_STRING("DE AD BE ", buf);
}

void test_format_can_data_len_capped_at_8(void) {
    // Even if len > 8 is passed, only 8 bytes should be formatted
    char buf[64];
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                      0x09, 0x0A, 0x0B, 0x0C};
    format_can_data(data, 12, buf);
    TEST_ASSERT_EQUAL_STRING("01 02 03 04 05 06 07 08 ", buf);
}

void test_format_can_data_max_len_255_still_safe(void) {
    // uint8_t max is 255; should still be capped at 8
    char buf[64];
    uint8_t data[16] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
                        0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0};
    format_can_data(data, 255, buf);
    // Should only format first 8 bytes
    TEST_ASSERT_EQUAL_STRING("FF FE FD FC FB FA F9 F8 ", buf);
}

void test_format_can_data_null_terminated(void) {
    // Verify buf is properly null-terminated after formatting
    char buf[64];
    memset(buf, 'X', sizeof(buf));  // Fill with junk
    uint8_t data[] = {0xAA};
    format_can_data(data, 1, buf);
    TEST_ASSERT_EQUAL_STRING("AA ", buf);
    // Verify no junk after the null terminator affects string length
    TEST_ASSERT_EQUAL(3, strlen(buf));
}

// =========================================================================
// format_can_csv_row tests
// =========================================================================

static int count_char(const char* s, char c) {
    int n = 0;
    while (*s) { if (*s == c) n++; s++; }
    return n;
}

static int count_fields(const char* s) {
    return count_char(s, ',') + 1;
}

void test_can_csv_full_frame_has_15_fields(void) {
    char buf[256];
    uint8_t data[] = {0xFF, 0x00, 0xA0, 0x0B, 0x12, 0x34, 0x56, 0x78};
    format_can_csv_row(1000, 1, 0x1AB, data, 8, buf, sizeof(buf));
    // Strip trailing newline for field count
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    TEST_ASSERT_EQUAL_INT(15, count_fields(buf));
}

void test_can_csv_short_frame_has_15_fields(void) {
    char buf[256];
    uint8_t data[] = {0xDE, 0xAD};
    format_can_csv_row(500, 2, 0x100, data, 2, buf, sizeof(buf));
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    TEST_ASSERT_EQUAL_INT(15, count_fields(buf));
}

void test_can_csv_empty_frame_has_15_fields(void) {
    char buf[256];
    uint8_t data[] = {};
    format_can_csv_row(0, 1, 0x000, data, 0, buf, sizeof(buf));
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    TEST_ASSERT_EQUAL_INT(15, count_fields(buf));
}

void test_can_csv_14_commas(void) {
    char buf[256];
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    format_can_csv_row(100, 1, 0x7FF, data, 8, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(14, count_char(buf, ','));
}

void test_can_csv_trailing_4_fields_empty(void) {
    char buf[256];
    uint8_t data[] = {0xAA};
    format_can_csv_row(42, 1, 0x010, data, 1, buf, sizeof(buf));
    // Row should end with ",,,,\n" — 4 empty trailing fields
    const char* end = buf + strlen(buf);
    TEST_ASSERT_EQUAL_STRING(",,,,\n", end - 5);
}

void test_can_csv_bus_number_format(void) {
    char buf[256];
    uint8_t data[] = {0x00};
    format_can_csv_row(0, 1, 0x000, data, 1, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",CAN1,"));

    format_can_csv_row(0, 2, 0x000, data, 1, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",CAN2,"));
}

void test_can_csv_can_id_hex_format(void) {
    char buf[256];
    uint8_t data[] = {0x00};
    format_can_csv_row(0, 1, 0x0AB, data, 1, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "0x0AB"));

    format_can_csv_row(0, 1, 0x7FF, data, 1, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "0x7FF"));
}

void test_can_csv_data_bytes_uppercase_hex(void) {
    char buf[256];
    uint8_t data[] = {0x0A, 0xBF, 0xFF};
    format_can_csv_row(0, 1, 0x100, data, 3, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",0A,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",BF,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",FF,"));
}

void test_can_csv_ends_with_newline(void) {
    char buf[256];
    uint8_t data[] = {0x01};
    format_can_csv_row(0, 1, 0x100, data, 1, buf, sizeof(buf));
    size_t len = strlen(buf);
    TEST_ASSERT_TRUE(len > 0);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[len - 1]);
}

void test_can_csv_short_frame_pads_empty(void) {
    // len=3: d0-d2 populated, d3-d7 empty
    char buf[256];
    uint8_t data[] = {0x11, 0x22, 0x33};
    format_can_csv_row(0, 1, 0x100, data, 3, buf, sizeof(buf));
    // After "0x100,11,22,33" there should be 5 empty data fields ",,,,,", then ",,,,\n"
    // The pattern ",11,22,33,,,,," should appear
    TEST_ASSERT_NOT_NULL(strstr(buf, ",11,22,33,,,,,"));
}

// =========================================================================
// format_adc_csv_row tests
// =========================================================================

void test_adc_csv_has_15_fields(void) {
    char buf[256];
    format_adc_csv_row(1000, 2.341f, 1.876f, 150.5f, 200.3f, buf, sizeof(buf));
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    TEST_ASSERT_EQUAL_INT(15, count_fields(buf));
}

void test_adc_csv_14_commas(void) {
    char buf[256];
    format_adc_csv_row(1000, 2.341f, 1.876f, 150.5f, 200.3f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(14, count_char(buf, ','));
}

void test_adc_csv_10_commas_between_adc_and_first_value(void) {
    char buf[256];
    format_adc_csv_row(500, 2.341f, 1.876f, 150.5f, 200.3f, buf, sizeof(buf));
    // Find "ADC" in the string, then count commas until first digit of travel_v1
    const char* adc = strstr(buf, "ADC");
    TEST_ASSERT_NOT_NULL(adc);
    const char* p = adc + 3; // skip "ADC"
    int commas = 0;
    while (*p == ',') { commas++; p++; }
    TEST_ASSERT_EQUAL_INT(10, commas);
}

void test_adc_csv_first_9_fields_after_source_empty(void) {
    char buf[256];
    format_adc_csv_row(100, 1.0f, 2.0f, 3.0f, 4.0f, buf, sizeof(buf));
    // After "100,ADC," the next 9 commas should have nothing between them
    // That means ",ADC,,,,,,,,,," with 10 commas after ADC
    TEST_ASSERT_NOT_NULL(strstr(buf, ",ADC,,,,,,,,,,"));
}

void test_adc_csv_travel_3_decimal_places(void) {
    char buf[256];
    format_adc_csv_row(0, 2.341f, 1.876f, 0.0f, 0.0f, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "2.341"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1.876"));
}

void test_adc_csv_psi_1_decimal_place(void) {
    char buf[256];
    format_adc_csv_row(0, 0.0f, 0.0f, 150.5f, 200.3f, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "150.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "200.3"));
}

void test_adc_csv_ends_with_newline(void) {
    char buf[256];
    format_adc_csv_row(0, 0.0f, 0.0f, 0.0f, 0.0f, buf, sizeof(buf));
    size_t len = strlen(buf);
    TEST_ASSERT_TRUE(len > 0);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[len - 1]);
}

void test_adc_csv_zero_values(void) {
    char buf[256];
    format_adc_csv_row(0, 0.0f, 0.0f, 0.0f, 0.0f, buf, sizeof(buf));
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    TEST_ASSERT_EQUAL_INT(15, count_fields(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "0.000,0.000,0.0,0.0"));
}

// =========================================================================
// parse_serial_command tests
// =========================================================================

void test_parse_cmd_new_uppercase(void) {
    ParsedCommand cmd = parse_serial_command("NEW");
    TEST_ASSERT_EQUAL(CMD_NEW, cmd.type);
}

void test_parse_cmd_new_lowercase(void) {
    ParsedCommand cmd = parse_serial_command("new");
    TEST_ASSERT_EQUAL(CMD_NEW, cmd.type);
}

void test_parse_cmd_new_mixed_case(void) {
    ParsedCommand cmd = parse_serial_command("nEw");
    TEST_ASSERT_EQUAL(CMD_NEW, cmd.type);
}

void test_parse_cmd_run_with_description(void) {
    ParsedCommand cmd = parse_serial_command("RUN: some desc");
    TEST_ASSERT_EQUAL(CMD_RUN, cmd.type);
    TEST_ASSERT_EQUAL_STRING("some desc", cmd.description);
}

void test_parse_cmd_run_no_space_after_colon(void) {
    ParsedCommand cmd = parse_serial_command("run:nodesc");
    TEST_ASSERT_EQUAL(CMD_RUN, cmd.type);
    TEST_ASSERT_EQUAL_STRING("nodesc", cmd.description);
}

void test_parse_cmd_run_empty_description(void) {
    ParsedCommand cmd = parse_serial_command("RUN:");
    TEST_ASSERT_EQUAL(CMD_RUN, cmd.type);
    TEST_ASSERT_EQUAL_STRING("", cmd.description);
}

void test_parse_cmd_stop(void) {
    ParsedCommand cmd = parse_serial_command("STOP");
    TEST_ASSERT_EQUAL(CMD_STOP, cmd.type);
}

void test_parse_cmd_travel(void) {
    ParsedCommand cmd = parse_serial_command("TRAVEL");
    TEST_ASSERT_EQUAL(CMD_TRAVEL, cmd.type);
}

void test_parse_cmd_status(void) {
    ParsedCommand cmd = parse_serial_command("STATUS");
    TEST_ASSERT_EQUAL(CMD_STATUS, cmd.type);
}

void test_parse_cmd_empty_string(void) {
    ParsedCommand cmd = parse_serial_command("");
    TEST_ASSERT_EQUAL(CMD_UNKNOWN, cmd.type);
}

void test_parse_cmd_garbage(void) {
    ParsedCommand cmd = parse_serial_command("GARBAGE");
    TEST_ASSERT_EQUAL(CMD_UNKNOWN, cmd.type);
}

void test_parse_cmd_null_input(void) {
    ParsedCommand cmd = parse_serial_command(NULL);
    TEST_ASSERT_EQUAL(CMD_UNKNOWN, cmd.type);
}

void test_parse_cmd_leading_trailing_whitespace(void) {
    ParsedCommand cmd = parse_serial_command("  STOP  ");
    TEST_ASSERT_EQUAL(CMD_STOP, cmd.type);
}

void test_parse_cmd_run_description_truncation(void) {
    // Build a string with a very long description (200+ chars)
    char input[300] = "RUN: ";
    memset(input + 5, 'A', 250);
    input[255] = '\0';
    ParsedCommand cmd = parse_serial_command(input);
    TEST_ASSERT_EQUAL(CMD_RUN, cmd.type);
    // Description should be truncated to 127 chars
    TEST_ASSERT_EQUAL(127, strlen(cmd.description));
}

// =========================================================================
// voltage_to_psi edge case tests
// =========================================================================

void test_voltage_to_psi_just_above_min(void) {
    // 0.501V is just inside the linear range
    float psi = voltage_to_psi(0.501f);
    TEST_ASSERT_TRUE(psi > 0.0f);
    TEST_ASSERT_TRUE(psi < 5.0f);  // Very small PSI value
}

void test_voltage_to_psi_just_below_max(void) {
    // 4.499V is just inside the linear range
    float psi = voltage_to_psi(4.499f);
    TEST_ASSERT_TRUE(psi < 3000.0f);
    TEST_ASSERT_TRUE(psi > 2995.0f);  // Very close to max
}

void test_voltage_to_psi_linearity(void) {
    // Check multiple points for linear relationship: PSI = (V-0.5)/4.0 * 3000
    float test_voltages[] = {0.9f, 1.3f, 2.1f, 3.0f, 3.7f, 4.2f};
    for (int i = 0; i < 6; i++) {
        float v = test_voltages[i];
        float expected = (v - 0.5f) / 4.0f * 3000.0f;
        TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, voltage_to_psi(v));
    }
}

int main(void) {
    UNITY_BEGIN();

    // voltage_to_psi tests
    RUN_TEST(test_voltage_to_psi_at_min_boundary);
    RUN_TEST(test_voltage_to_psi_at_max_boundary);
    RUN_TEST(test_voltage_to_psi_clamps_below_min);
    RUN_TEST(test_voltage_to_psi_clamps_above_max);
    RUN_TEST(test_voltage_to_psi_midpoint);
    RUN_TEST(test_voltage_to_psi_quarter_point);
    RUN_TEST(test_voltage_to_psi_three_quarter_point);
    RUN_TEST(test_voltage_to_psi_typical_operating_point);

    // format_can_data tests
    RUN_TEST(test_format_can_data_empty_frame);
    RUN_TEST(test_format_can_data_single_byte);
    RUN_TEST(test_format_can_data_full_frame);
    RUN_TEST(test_format_can_data_mixed_values);
    RUN_TEST(test_format_can_data_three_bytes);
    RUN_TEST(test_format_can_data_len_capped_at_8);
    RUN_TEST(test_format_can_data_max_len_255_still_safe);
    RUN_TEST(test_format_can_data_null_terminated);

    // format_can_csv_row tests
    RUN_TEST(test_can_csv_full_frame_has_15_fields);
    RUN_TEST(test_can_csv_short_frame_has_15_fields);
    RUN_TEST(test_can_csv_empty_frame_has_15_fields);
    RUN_TEST(test_can_csv_14_commas);
    RUN_TEST(test_can_csv_trailing_4_fields_empty);
    RUN_TEST(test_can_csv_bus_number_format);
    RUN_TEST(test_can_csv_can_id_hex_format);
    RUN_TEST(test_can_csv_data_bytes_uppercase_hex);
    RUN_TEST(test_can_csv_ends_with_newline);
    RUN_TEST(test_can_csv_short_frame_pads_empty);

    // format_adc_csv_row tests
    RUN_TEST(test_adc_csv_has_15_fields);
    RUN_TEST(test_adc_csv_14_commas);
    RUN_TEST(test_adc_csv_10_commas_between_adc_and_first_value);
    RUN_TEST(test_adc_csv_first_9_fields_after_source_empty);
    RUN_TEST(test_adc_csv_travel_3_decimal_places);
    RUN_TEST(test_adc_csv_psi_1_decimal_place);
    RUN_TEST(test_adc_csv_ends_with_newline);
    RUN_TEST(test_adc_csv_zero_values);

    // parse_serial_command tests
    RUN_TEST(test_parse_cmd_new_uppercase);
    RUN_TEST(test_parse_cmd_new_lowercase);
    RUN_TEST(test_parse_cmd_new_mixed_case);
    RUN_TEST(test_parse_cmd_run_with_description);
    RUN_TEST(test_parse_cmd_run_no_space_after_colon);
    RUN_TEST(test_parse_cmd_run_empty_description);
    RUN_TEST(test_parse_cmd_stop);
    RUN_TEST(test_parse_cmd_travel);
    RUN_TEST(test_parse_cmd_status);
    RUN_TEST(test_parse_cmd_empty_string);
    RUN_TEST(test_parse_cmd_garbage);
    RUN_TEST(test_parse_cmd_null_input);
    RUN_TEST(test_parse_cmd_leading_trailing_whitespace);
    RUN_TEST(test_parse_cmd_run_description_truncation);

    // voltage_to_psi edge cases
    RUN_TEST(test_voltage_to_psi_just_above_min);
    RUN_TEST(test_voltage_to_psi_just_below_max);
    RUN_TEST(test_voltage_to_psi_linearity);

    return UNITY_END();
}
