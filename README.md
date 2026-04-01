# Moon Patrol iBooster DAQ

ESP32-based CAN bus sniffer and data acquisition system for reverse engineering the Bosch iBooster (Honda Accord) CAN protocol and characterizing brake system pressure response.

Sniffs two CAN buses simultaneously, reads two analog pressure transducers via a 16-bit ADC, and logs everything to SD card CSV files for offline analysis.

## Hardware

| Component | Description |
|-----------|-------------|
| ESP32 Dev Board | HiLetgo 38-pin, USB-C, CP2102 |
| SN65HVD230 | 3.3V CAN transceiver for TWAI (CAN bus #1) |
| MCP2515 + TJA1050 | SPI CAN controller for CAN bus #2 (5V VCC, 3.3V SPI) |
| ADS1115 | 16-bit I2C ADC for pressure transducers |
| Pressure Transducers (x2) | 0-3000 PSI, 0.5-4.5V output |
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
                               │            │  ┌─┴──────┐  CAN bus #1
                               │  GPIO 16   │  │ADS1115 │
                               │  (SD CS)   │  │ 16-bit │
                               └────┬───────┘  │  ADC   │
                                    │          │ A0←Front│◄── Pressure
                               ┌────┴────┐    │ A1←Rear │◄── Transducer
                               │ SD Card │    └────────┘
                               │ Module  │
                               └─────────┘
```

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
| `STOP` | Stop the current run and close the log file |
| `STATUS` | Print system status (active peripherals, run number, samples) |

The **BOOT button** (GPIO 0) toggles logging on/off as a hardware trigger.

### Log File Format

Files are saved to the SD card as `run_001.csv`, `run_002.csv`, etc.

```csv
# Optional description from RUN command
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,psi_front,psi_rear
1234,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,
1235,CAN2,0x292,A0,B1,C2,00,00,00,00,00,,
1240,PSI,,,,,,,,,1250.5,1180.3
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
- Print pressure min/max/avg for each channel
- Plot pressure vs time
- Plot each changing data byte vs time for every CAN ID (to identify which bytes encode useful data)

## Project Structure

```
can-sniffer/
├── src/main.cpp           # ESP32 firmware
├── platformio.ini         # PlatformIO build config
├── analysis/
│   ├── analyze.py         # Python log analysis script
│   └── plots/             # Generated plot images
├── include/               # Project headers (if needed)
├── lib/                   # Project-specific libraries
└── venv/                  # Python virtual environment
```

## Notes

- Both CAN channels operate in **listen-only mode** — no frames are transmitted
- The ADS1115 code handles gracefully when the sensor is not connected (Phase 1: CAN-only testing)
- CAN speed defaults to 500 kbps — change `CAN_SPEED_KBPS` in `main.cpp` if needed
- The MCP2515 module needs 5V on VCC (TJA1050 transceiver) but its SPI signals are 3.3V compatible
- macOS may need the [Silicon Labs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) if the ESP32 board doesn't appear as a serial port
