import asyncio
import json
import logging
import os
import platform
import threading
import time
from contextlib import asynccontextmanager

import psutil
import serial
import speedtest
import uvicorn
from fastapi import FastAPI

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SERIAL_PORT = os.getenv("SERIAL_PORT", "")        # Override via env var
BAUD_RATE   = int(os.getenv("BAUD_RATE", "115200"))
HOST        = os.getenv("HOST", "0.0.0.0")
PORT        = int(os.getenv("PORT", "8000"))
SPEEDTEST_INTERVAL = int(os.getenv("SPEEDTEST_INTERVAL", "300"))  # seconds

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("speed-net")

# ---------------------------------------------------------------------------
# Shared state
# ---------------------------------------------------------------------------
state = {
    "download": 0.0,
    "upload":   0.0,
    "ping":     0.0,
    "rx_mbps":  0.0,
    "tx_mbps":  0.0,
    "status":   "initializing",
}

ser = None

# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------
SERIAL_CANDIDATES = [
    "/dev/ttyACM0",
    "/dev/ttyACM1",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/ttyS3",    # COM3 mapped in WSL
    "/dev/ttyS4",    # COM4 mapped in WSL
    "COM3",          # Native Windows fallback
]


def _pick_serial_port() -> str:
    """Return the first serial port that exists, or the configured one."""
    if SERIAL_PORT:
        return SERIAL_PORT

    for candidate in SERIAL_CANDIDATES:
        try:
            if os.path.exists(candidate):
                return candidate
        except Exception:
            continue
    return SERIAL_CANDIDATES[0]  # fallback


def serial_connect():
    """Open the serial port; log but don't crash on failure."""
    global ser
    port = _pick_serial_port()
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        log.info(f"Serial connected: {port}")
    except Exception as e:
        log.warning(f"Serial unavailable ({port}): {e}")


def send_data():
    """Continuously push compact JSON to the serial port (1 Hz)."""
    while True:
        if ser and ser.is_open:
            payload = json.dumps({
                "dl":     round(state["download"], 1),
                "ul":     round(state["upload"], 1),
                "ping":   round(state["ping"], 0),
                "rx":     round(state["rx_mbps"], 2),
                "tx":     round(state["tx_mbps"], 2),
                "status": state["status"][:2],
            }) + "\n"
            try:
                ser.write(payload.encode())
            except Exception as e:
                log.error(f"Serial write error: {e}")
        time.sleep(1)


# ---------------------------------------------------------------------------
# Background workers
# ---------------------------------------------------------------------------
def loop_speedtest():
    """Run a speed test every SPEEDTEST_INTERVAL seconds."""
    backoff = 10
    while True:
        try:
            state["status"] = "testing"
            log.info("Starting speed test ...")
            st = speedtest.Speedtest()
            st.get_best_server()
            state["ping"]     = st.results.ping
            state["download"] = st.download() / 1_000_000
            state["upload"]   = st.upload()   / 1_000_000
            state["status"]   = "ok"
            backoff = 10  # reset backoff on success
            log.info(
                f"Speed test done — ↓ {state['download']:.1f} Mbps  "
                f"↑ {state['upload']:.1f} Mbps  ◷ {state['ping']:.0f} ms"
            )
        except Exception as e:
            state["status"] = "error"
            log.error(f"Speedtest error: {e} — retrying in {backoff}s")
            time.sleep(backoff)
            backoff = min(backoff * 2, SPEEDTEST_INTERVAL)
            continue
        time.sleep(SPEEDTEST_INTERVAL)


def traffic_loop():
    """Measure real-time network throughput via psutil (≈1 Hz)."""
    while True:
        try:
            before = psutil.net_io_counters()
            time.sleep(1)
            after  = psutil.net_io_counters()
            state["rx_mbps"] = (after.bytes_recv - before.bytes_recv) * 8 / 1_000_000
            state["tx_mbps"] = (after.bytes_sent - before.bytes_sent) * 8 / 1_000_000
        except Exception as e:
            log.error(f"Traffic error: {e}")
            time.sleep(1)


# ---------------------------------------------------------------------------
# FastAPI app with modern lifespan
# ---------------------------------------------------------------------------
@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup / shutdown lifecycle."""
    serial_connect()
    threads = [
        threading.Thread(target=loop_speedtest, daemon=True),
        threading.Thread(target=traffic_loop,   daemon=True),
        threading.Thread(target=send_data,      daemon=True),
    ]
    for t in threads:
        t.start()
    log.info("All background workers started.")

    yield  # ← app is running

    # Shutdown
    if ser and ser.is_open:
        ser.close()
        log.info("Serial port closed.")


app = FastAPI(title="Speed-Net", lifespan=lifespan)


@app.get("/status")
def get_status():
    """Return current network stats."""
    return state


@app.post("/speedtest")
def trigger_speedtest():
    """Trigger an on-demand speed test."""
    threading.Thread(target=loop_speedtest, daemon=True).start()
    return {"msg": "Speedtest started"}


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    uvicorn.run("main:app", host=HOST, port=PORT, reload=False)