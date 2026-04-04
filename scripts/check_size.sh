#!/usr/bin/env bash
# check_size.sh — Build firmware and verify it fits on the ESP32.
#
# Parses PlatformIO's "RAM: ... Flash: ..." output and exits non-zero
# if either exceeds the warning threshold (default 80%).
#
# Usage:
#   ./scripts/check_size.sh              # default 80% warning threshold
#   ./scripts/check_size.sh --threshold 90   # custom threshold

set -euo pipefail

THRESHOLD=80

while [[ $# -gt 0 ]]; do
    case "$1" in
        --threshold)
            THRESHOLD="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [--threshold PERCENT]"
            echo "  Build firmware and check that flash/RAM usage stays below PERCENT%."
            echo "  Default threshold: 80%"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

echo "Building firmware..."
BUILD_OUTPUT=$(pio run -e esp32dev 2>&1)

# Extract RAM line: "RAM:   [=         ]   6.9% (used 22552 bytes from 327680 bytes)"
RAM_LINE=$(echo "$BUILD_OUTPUT" | grep -E '^RAM:')
FLASH_LINE=$(echo "$BUILD_OUTPUT" | grep -E '^Flash:')

if [[ -z "$RAM_LINE" || -z "$FLASH_LINE" ]]; then
    echo "ERROR: Could not parse size info from build output." >&2
    echo "$BUILD_OUTPUT"
    exit 1
fi

# Parse: "used XXXXX bytes from YYYYY bytes"
RAM_USED=$(echo "$RAM_LINE" | grep -oE 'used [0-9]+' | grep -oE '[0-9]+')
RAM_TOTAL=$(echo "$RAM_LINE" | grep -oE 'from [0-9]+' | grep -oE '[0-9]+')
FLASH_USED=$(echo "$FLASH_LINE" | grep -oE 'used [0-9]+' | grep -oE '[0-9]+')
FLASH_TOTAL=$(echo "$FLASH_LINE" | grep -oE 'from [0-9]+' | grep -oE '[0-9]+')

# Calculate percentages (using awk for float math)
RAM_PCT=$(awk "BEGIN {printf \"%.1f\", ($RAM_USED / $RAM_TOTAL) * 100}")
FLASH_PCT=$(awk "BEGIN {printf \"%.1f\", ($FLASH_USED / $FLASH_TOTAL) * 100}")
RAM_FREE=$((RAM_TOTAL - RAM_USED))
FLASH_FREE=$((FLASH_TOTAL - FLASH_USED))

# Format sizes in KB
RAM_USED_KB=$(awk "BEGIN {printf \"%.1f\", $RAM_USED / 1024}")
RAM_TOTAL_KB=$(awk "BEGIN {printf \"%.1f\", $RAM_TOTAL / 1024}")
RAM_FREE_KB=$(awk "BEGIN {printf \"%.1f\", $RAM_FREE / 1024}")
FLASH_USED_KB=$(awk "BEGIN {printf \"%.1f\", $FLASH_USED / 1024}")
FLASH_TOTAL_KB=$(awk "BEGIN {printf \"%.1f\", $FLASH_TOTAL / 1024}")
FLASH_FREE_KB=$(awk "BEGIN {printf \"%.1f\", $FLASH_FREE / 1024}")

echo ""
echo "========================================"
echo "  ESP32 Firmware Size Report"
echo "========================================"
echo ""
echo "  RAM:    ${RAM_USED_KB} KB / ${RAM_TOTAL_KB} KB  (${RAM_PCT}%)  — ${RAM_FREE_KB} KB free"
echo "  Flash:  ${FLASH_USED_KB} KB / ${FLASH_TOTAL_KB} KB  (${FLASH_PCT}%)  — ${FLASH_FREE_KB} KB free"
echo ""
echo "  Threshold: ${THRESHOLD}%"
echo ""

FAIL=0

RAM_OVER=$(awk "BEGIN {print ($RAM_PCT > $THRESHOLD) ? 1 : 0}")
FLASH_OVER=$(awk "BEGIN {print ($FLASH_PCT > $THRESHOLD) ? 1 : 0}")

if [[ "$RAM_OVER" == "1" ]]; then
    echo "  WARN: RAM usage ${RAM_PCT}% exceeds ${THRESHOLD}% threshold!"
    FAIL=1
else
    echo "  RAM:   OK"
fi

if [[ "$FLASH_OVER" == "1" ]]; then
    echo "  WARN: Flash usage ${FLASH_PCT}% exceeds ${THRESHOLD}% threshold!"
    FAIL=1
else
    echo "  Flash: OK"
fi

echo ""

if [[ "$FAIL" -eq 1 ]]; then
    echo "FAIL — firmware size exceeds threshold"
    exit 1
else
    echo "PASS — firmware fits comfortably"
    exit 0
fi
