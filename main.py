import json
import logging
import os
import platform
import threading
import time

import psutil
import serial
import speedtest

SERIAL_PORT        = os.getenv("SERIAL_PORT", "")
BAUD_RATE          = int(os.getenv("BAUD_RATE", "115200"))
SPEEDTEST_INTERVAL = int(os.getenv("SPEEDTEST_INTERVAL", "300"))

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("speed-net")

state = {
    "download": 0.0,
    "upload":   0.0,
    "ping":     0.0,
    "rx_mbps":  0.0,
    "tx_mbps":  0.0,
    "status":   "initializing",
}

ser = None

def _pick_serial_port() -> str:
    if SERIAL_PORT:
        return SERIAL_PORT

    import serial.tools.list_ports

    ports = list(serial.tools.list_ports.comports())
    log.info(f"Available serial ports: {[p.device for p in ports]}")

    stm_keywords = ["stm32", "st-link", "stlink", "usb serial", "usb com"]
    for p in ports:
        desc = (p.description or "").lower()
        if any(kw in desc for kw in stm_keywords):
            log.info(f"Found STM32-like port: {p.device} ({p.description})")
            return p.device

    if ports:
        log.info(f"Using first available port: {ports[0].device}")
        return ports[0].device

    default = "COM3" if platform.system() == "Windows" else "/dev/ttyACM0"
    log.warning(f"No serial ports found, falling back to {default}")
    return default

def serial_connect():
    global ser
    port = _pick_serial_port()
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        log.info(f"Serial connected: {port}")
    except Exception as e:
        log.warning(f"Serial unavailable ({port}): {e}")

def send_data():
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
                ser.flush()
                log.info(f"Serial sent: {payload.strip()}")
            except Exception as e:
                log.error(f"Serial write error: {e}")
        time.sleep(1)

def loop_speedtest():
    backoff = 10
    while True:
        try:
            state["status"] = "testing"
            log.info("Starting speed test ...")
            st = speedtest.Speedtest(secure=True)
            st.get_best_server()
            state["ping"]     = st.results.ping
            state["download"] = st.download() / 1_000_000
            state["upload"]   = st.upload()   / 1_000_000
            state["status"]   = "ok"
            backoff = 10
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

if __name__ == "__main__":
    serial_connect()
    threads = [
        threading.Thread(target=loop_speedtest, daemon=True),
        threading.Thread(target=traffic_loop,   daemon=True),
        threading.Thread(target=send_data,      daemon=True),
    ]
    for t in threads:
        t.start()
    log.info("All background workers started.")
    threading.Event().wait()
