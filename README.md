# Moon Patrol iBooster DAQ

[![CI](https://github.com/YeeP79/can-sniffer/actions/workflows/ci.yml/badge.svg)](https://github.com/YeeP79/can-sniffer/actions/workflows/ci.yml)

ESP32-based CAN bus sniffer and data acquisition system for reverse engineering the Bosch iBooster (Honda Accord) CAN protocol and characterizing brake system pressure response.

Sniffs two CAN buses simultaneously, reads the iBooster travel sensor and two analog pressure transducers via a 16-bit ADC, and logs everything to SD card CSV files for offline analysis.

## Hardware

| Component | Description |
|-----------|-------------|
| ESP32 Dev Board | HiLetgo 38-pin, USB-C, CP2102 |
| SN65HVD230 | 3.3V CAN transceiver for TWAI (CAN bus #1) |
| MCP2515 + TJA1050 | SPI CAN controller for CAN bus #2 (5V VCC, 3.3V SPI) |
| ADS1115 | 16-bit I2C ADC for travel sensors + pressure transducers |
| iBooster Travel Sensor | Dual hall effect, 0.5-4.5V output (tapped, not cut) |
| Pressure Transducers (x2) | 0-3000 PSI, 0.5-4.5V output (Phase 2) |
| SD Card Module | SPI, shared bus with MCP2515 |
| Buck Converter | 12V → 5V for MCP2515 module |

## Pin Assignments

```
ESP32 GPIO    Function
──────────    ────────────────────────
GPIO 4        TWAI CAN TX (→ SN65HVD230)
GPIO 5        TWAI CAN RX (← SN65HVD230)
GPIO 23       MCP2515 MOSI (VSPI)
GPIO 19       MCP2515 MISO (VSPI)
GPIO 18       MCP2515 SCK  (VSPI)
GPIO 15       MCP2515 CS
GPIO 2        MCP2515 INT
GPIO 16       SD Card CS   (shared VSPI bus)
GPIO 21       ADS1115 SDA  (I2C)
GPIO 22       ADS1115 SCL  (I2C)
GPIO 0        BOOT Button  (built-in, run trigger)

ADS1115 Ch    Function
──────────    ────────────────────────
A0            Travel sensor signal 1 (ECU pin 22) ← Phase 1
A1            Travel sensor signal 2 (ECU pin 2)  ← Phase 1
A2            Front pressure transducer            ← Phase 2
A3            Rear pressure transducer             ← Phase 2
```

## Wiring Diagram

```
                                 +12V from bench supply
                                      │
                                 ┌────┴────┐
                                 │  Buck    │
                                 │ 12V→5V  │
                                 └────┬────┘
                                      │ 5V
                    ┌─────────────────┤
                    │                 │
              ┌─────┴─────┐    ┌─────┴─────┐
              │  MCP2515  │    │  ESP32     │ USB
              │  TJA1050  │    │  Dev Board │──── MacBook
              │           │    │            │
              │  VCC=5V   │    │  3.3V/5V   │
              │  SPI@3.3V │◄──►│  VSPI      │
              │  CS=GPIO15│    │  GPIO 15   │
              │  INT=GPIO2│───►│  GPIO 2    │
              └─────┬─────┘    │            │
                CAN H/L        │  TWAI      │
                 to iBooster   │  GPIO 4 TX │    ┌───────────┐
                 CAN bus #2    │  GPIO 5 RX │◄──►│ SN65HVD230│
                               │            │    │  3.3V CAN │
                               │  I2C       │    └─────┬─────┘
                               │  GPIO 21   │◄──►┐   CAN H/L
                               │  GPIO 22   │    │   to iBooster
                               │            │  ┌─┴─────────┐ CAN bus #1
                               │  GPIO 16   │  │  ADS1115  │
                               │  (SD CS)   │  │  16-bit   │
                               └────┬───────┘  │   ADC     │
                                    │          │           │
                               ┌────┴────┐    │ A0 ← T1  │◄─┐ Travel sensor
                               │ SD Card │    │ A1 ← T2  │◄─┤ (tapped from
                               │ Module  │    │ A2 ← F   │  │ iBooster ECU
                               └─────────┘    │ A3 ← R   │  │ connector)
                                              └──────────┘  │
                                                            │
                             iBooster ECU ◄─────────────────┘
                             (signals pass through — tap only,
                              ADS1115 high-Z input)
```

**Note:** Travel sensor signals are *tapped* (spliced), not intercepted. The ADS1115 has high input impedance so the tap does not load the signal appreciably. Both signals still reach the iBooster ECU.

## Build and Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Flash to ESP32
pio run --target upload

# Serial monitor
pio device monitor
```

## Usage

### Serial Commands

Connect via serial monitor at 115200 baud:

| Command | Action |
|---------|--------|
| `NEW` | Start a new logging run |
| `RUN: description` | Start a run with a description header |
| `STOP` | Stop the current run or travel stream |
| `STATUS` | Print system status (sensors, run number, samples) |
| `TRAVEL` | Stream travel sensor voltages at 100Hz (for wiring validation) |

The **BOOT button** (GPIO 0) toggles logging on/off as a hardware trigger.

### TRAVEL Command

The `TRAVEL` command streams raw travel sensor voltages for quick validation that the sensors are connected and responding. Useful for checking wiring before a full CAN sniffing session:

```
> TRAVEL
T1: 2.341V  T2: 2.105V
T1: 2.342V  T2: 2.106V
T1: 2.580V  T2: 2.310V    ← pushrod moving
T1: 2.341V  T2: 2.105V    ← returned to rest
> STOP
```

### Log File Format

Files are saved to the SD card as `run_001.csv`, `run_002.csv`, etc.

```csv
# Optional description from RUN command
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1234,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1235,CAN2,0x292,A0,B1,C2,00,00,00,00,00,,,,
1240,ADC,,,,,,,,,2.341,2.105,0.0,0.0
```

### Data Analysis

```bash
# Activate the Python virtual environment
source venv/bin/activate

# Analyze a log file
python analysis/analyze.py run_001.csv

# Output: summary stats + plots saved to analysis/plots/
```

The analysis script will:
- Print unique CAN IDs and message frequency per ID
- Print travel sensor voltage min/max/avg for each channel
- Print pressure min/max/avg for each channel (Phase 2)
- Plot travel sensor voltage vs time
- Plot pressure vs time
- Plot each changing data byte vs time for every CAN ID
- **Plot CAN byte vs travel voltage correlation** — bytes that correlate linearly with travel sensor voltage are pushrod position encodings

### Phase Configuration

The firmware has a `DAQ_PHASE` define in `include/config.h`:

| Phase | Travel Sensors (A0/A1) | Pressure (A2/A3) | Serial Output |
|-------|----------------------|-------------------|---------------|
| **1** (default) | Connected | Not connected | Travel only |
| **2** | Connected | Connected | Travel + pressure |

**All 4 ADC channels are always logged to SD** regardless of phase. The phase setting only controls which readings appear on the serial monitor (to avoid showing noise from unconnected transducers).

## Development

### Testing

```bash
# Run all tests
make test

# C++ unit tests only (PlatformIO native)
make test-cpp

# Python tests only
make test-py
```

### Linting

```bash
# Run all linters
make lint

# C++ linting only (cppcheck via PlatformIO)
make lint-cpp

# Python linting only (ruff)
make lint-py
```

### Firmware Size Check

```bash
# Build and report flash/RAM usage vs ESP32 limits
make size

# Custom warning threshold (default 80%)
./scripts/check_size.sh --threshold 90
```

### Setup

```bash
# Install Python dev dependencies
source venv/bin/activate
pip install -r requirements-dev.txt
```

## Project Structure

```
can-sniffer/
├── src/
│   ├── main.cpp              # Composition root — wires modules via callbacks
│   ├── can_bus.cpp           # TWAI + MCP2515 CAN bus drivers
│   ├── adc_sensors.cpp       # ADS1115 ADC reading (travel + pressure)
│   ├── sd_logger.cpp         # SD card CSV logging
│   └── serial_cmd.cpp        # Serial commands, BOOT button, status
├── include/
│   ├── config.h              # Pin definitions, constants, DAQ_PHASE
│   ├── daq_state.h           # Shared state (extern globals + hardware objects)
│   ├── can_bus.h             # CAN bus module interface
│   ├── adc_sensors.h         # ADC sensors module interface
│   ├── sd_logger.h           # SD logger module interface
│   ├── serial_cmd.h          # Serial command module interface
│   └── utils.h               # Shared utility functions (testable on host)
├── platformio.ini            # PlatformIO build config (esp32dev + native test)
├── test/
│   └── test_utils/
│       └── test_utils.cpp    # C++ unit tests (Unity)
├── analysis/
│   ├── analyze.py            # Python log analysis script
│   ├── test_analyze.py       # Python tests (pytest)
│   └── plots/                # Generated plot images
├── requirements.txt          # Python runtime dependencies
├── requirements-dev.txt      # Python dev dependencies (pytest, ruff)
├── ruff.toml                 # Ruff linter configuration
├── scripts/
│   └── check_size.sh         # Firmware size checker (flash/RAM vs ESP32 limits)
├── Makefile                  # Convenience build/test/lint targets
├── .github/workflows/ci.yml  # GitHub Actions CI pipeline
├── lib/                      # Project-specific libraries
└── venv/                     # Python virtual environment
```

## Notes

- Both CAN channels operate in **listen-only mode** — no frames are transmitted
- A0/A1 are **Phase 1** (travel sensor taps), A2/A3 are **Phase 2** (pressure transducers)
- The ADS1115 runs at 860 SPS for fast 4-channel reads (~215 samples/channel/sec)
- CAN speed defaults to 500 kbps — change `CAN_SPEED_KBPS` in `include/config.h` if needed
- The MCP2515 module needs 5V on VCC (TJA1050 transceiver) but its SPI signals are 3.3V compatible
- The ADS1115 code handles gracefully when the sensor is not connected
- macOS may need the [Silicon Labs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) if the ESP32 board doesn't appear as a serial port
