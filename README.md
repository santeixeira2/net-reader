# Speed-Net

Real-time internet speed and network traffic monitor displayed on the built-in OLED of the STM32WB5MM-DK.

```
[Python FastAPI] ──serial──► [STM32WB5MM-DK] ──SPI──► [SSD1315 OLED 128×64]
```

The Python backend runs a speed test, measures live RX/TX throughput, and pushes compact JSON to the STM32 over UART every second. The firmware parses the JSON and renders a live dashboard on the display.

---

## Display

```
┌─────────────────────────┐
│ SPEED-NET          [OK] │
│─────────────────────────│
│ ↓ 416.4 Mbps           │
│ ↑ 322.9 Mbps           │
│ ~ 6 ms                 │
│─────────────────────────│
│ RX:1.23      TX:0.45   │
└─────────────────────────┘
```

| Symbol | Meaning |
|--------|---------|
| `↓` | Download (speed test result) |
| `↑` | Upload (speed test result) |
| `~` | Ping to best server |
| `RX / TX` | Live throughput, updated every second |
| `[OK]` | Idle — last test succeeded |
| `[..]` | Speed test running (~30–60 s) |
| `[!!]` | Speed test error |

---

## Requirements

- Python 3.10+
- PlatformIO CLI (`pip install platformio`)
- STM32WB5MM-DK connected via USB (ST-Link)

---

## Setup

### 1. Clone and create virtual environment

```bash
git clone <repo-url>
cd speed-net
python3 -m venv .venv
source .venv/bin/activate      # macOS / Linux
# .\.venv\Scripts\activate     # Windows
pip install -r requirements.txt
```

### 2. Flash firmware

```bash
cd firmware
platformio run --target upload
cd ..
```

> **Windows:** upload requires STM32CubeProgrammer. Use `flash.bat` instead.

### 3. Find the serial port

**macOS:**
```bash
ls /dev/cu.*
# Look for something like /dev/cu.usbmodem1103
```

**Linux:**
```bash
ls /dev/ttyACM*
# Usually /dev/ttyACM0
```

**Windows:** Device Manager → Ports (COM & LPT) → usually `COM3`

### 4. Run

```bash
source .venv/bin/activate
python3 main.py                          # auto-detects serial port
SERIAL_PORT=/dev/cu.usbmodem1103 python3 main.py  # or specify explicitly
```

The display shows `[..]` while the first speed test runs (~30–60 s), then switches to `[OK]` with results.

---

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SERIAL_PORT` | auto-detect | STM32 serial port |
| `BAUD_RATE` | `115200` | UART baud rate |
| `PORT` | `8000` | HTTP port for the API |
| `SPEEDTEST_INTERVAL` | `300` | Seconds between automatic speed tests |

---

## API

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/status` | Current network stats |
| `POST` | `/speedtest` | Trigger an immediate speed test |

```bash
curl http://localhost:8000/status
curl -X POST http://localhost:8000/speedtest
```

---

## Architecture

### Python backend (`main.py`)

Three background threads run concurrently:

- **`loop_speedtest`** — runs `speedtest-cli` every `SPEEDTEST_INTERVAL` seconds via `get_best_server()`, which selects the lowest-latency server and measures download, upload, and ping.
- **`traffic_loop`** — samples `psutil.net_io_counters()` every second to compute live RX/TX in Mbps.
- **`send_data`** — serializes state to compact JSON and writes it to the serial port at 1 Hz.

### STM32 firmware (`firmware/src/main.cpp`)

- UART RX buffer set to 256 bytes (`SERIAL_RX_BUFFER_SIZE=256`) to fit full JSON packets.
- `parseSerial()` accumulates bytes until `\n`, then deserializes with ArduinoJson.
- `drawDashboard()` redraws the full framebuffer via U8g2 every 200 ms.

---

## Platform notes

### macOS / Linux
- ST-Link works natively, no extra drivers needed.
- Serial port is usually auto-detected.

### Windows
- Requires the official ST-Link driver: [STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)
- Flash with `flash.bat` (uses STM32CubeProgrammer).
- Serial communication over ST-Link VCP can be unstable — prefer macOS/Linux.
