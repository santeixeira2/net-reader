// ==========================================================================
//  Speed-Net — STM32WB5MM-DK OLED Dashboard Firmware
//  Receives JSON via USART1 and renders stats on the built-in SSD1315 OLED.
// ==========================================================================

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <ArduinoJson.h>

// --------------------------------------------------------------------------
//  OLED Wiring on STM32WB5MM-DK (built-in SSD1315 128×64 via SPI1)
//    SCK  = PA5   (SPI1_SCK)
//    MOSI = PA7   (SPI1_MOSI)
//    CS   = PH0   (CS_DISP)
//    DC   = PC6
//    RST  = PC12
// --------------------------------------------------------------------------
//  U8g2 constructor for SSD1315, 128x64, hardware SPI, full framebuffer
//  SSD1315 is register-compatible with SSD1306 — U8g2 supports it natively.
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(
    U8G2_R0,       // rotation: landscape
    PH0,           // CS
    PC6,           // DC
    PC12           // RST
);

// --------------------------------------------------------------------------
//  Data from the FastAPI backend
// --------------------------------------------------------------------------
struct NetData {
    float dl     = 0.0f;   // download Mbps
    float ul     = 0.0f;   // upload Mbps
    float ping   = 0.0f;   // latency ms
    float rx     = 0.0f;   // real-time RX Mbps
    float tx     = 0.0f;   // real-time TX Mbps
    char  status[4] = "in"; // 2-char status code
};

static NetData net;
static bool    dataReceived = false;
static uint32_t lastDataMs  = 0;

// Spinner animation frames
static const char* spinnerFrames[] = { "|", "/", "-", "\\" };
static uint8_t spinnerIdx = 0;

// --------------------------------------------------------------------------
//  Parse one JSON line from Serial
// --------------------------------------------------------------------------
void parseSerial() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        // Silently ignore malformed lines
        return;
    }

    net.dl   = doc["dl"]   | 0.0f;
    net.ul   = doc["ul"]   | 0.0f;
    net.ping = doc["ping"] | 0.0f;
    net.rx   = doc["rx"]   | 0.0f;
    net.tx   = doc["tx"]   | 0.0f;

    const char* st = doc["status"] | "??";
    strncpy(net.status, st, sizeof(net.status) - 1);
    net.status[sizeof(net.status) - 1] = '\0';

    dataReceived = true;
    lastDataMs   = millis();
}

// --------------------------------------------------------------------------
//  Render the dashboard on the OLED
// --------------------------------------------------------------------------
void drawDashboard() {
    u8g2.clearBuffer();

    // ── Title bar ──────────────────────────────────────────────────
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(2, 12, "SPEED-NET");

    // Status badge (right-aligned)
    char badge[8];
    if (strcmp(net.status, "ok") == 0) {
        snprintf(badge, sizeof(badge), "[OK]");
    } else if (strcmp(net.status, "te") == 0) {
        snprintf(badge, sizeof(badge), "[..]");
    } else if (strcmp(net.status, "er") == 0) {
        snprintf(badge, sizeof(badge), "[!!]");
    } else {
        snprintf(badge, sizeof(badge), "[%s]", net.status);
    }
    uint8_t badgeW = u8g2.getStrWidth(badge);
    u8g2.drawStr(126 - badgeW, 12, badge);

    // Separator
    u8g2.drawHLine(0, 15, 128);

    // ── Speed test results ─────────────────────────────────────────
    u8g2.setFont(u8g2_font_6x13_tr);

    char buf[32];

    // Download
    u8g2.drawStr(2, 28, "\x19");  // ↓ arrow down
    snprintf(buf, sizeof(buf), "%.1f Mbps", net.dl);
    u8g2.drawStr(14, 28, buf);

    // Upload
    u8g2.drawStr(2, 40, "\x18");  // ↑ arrow up
    snprintf(buf, sizeof(buf), "%.1f Mbps", net.ul);
    u8g2.drawStr(14, 40, buf);

    // Ping
    u8g2.drawStr(2, 52, "~");
    snprintf(buf, sizeof(buf), "%.0f ms", net.ping);
    u8g2.drawStr(14, 52, buf);

    // Separator
    u8g2.drawHLine(0, 54, 128);

    // ── Real-time traffic ──────────────────────────────────────────
    u8g2.setFont(u8g2_font_5x8_tr);

    snprintf(buf, sizeof(buf), "RX %.2f", net.rx);
    u8g2.drawStr(2, 63, buf);

    snprintf(buf, sizeof(buf), "TX %.2f", net.tx);
    u8g2.drawStr(70, 63, buf);

    u8g2.sendBuffer();
}

// --------------------------------------------------------------------------
//  Render "waiting" screen when no data has arrived yet
// --------------------------------------------------------------------------
void drawWaiting() {
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(18, 20, "SPEED-NET");

    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(14, 38, "Waiting for");
    u8g2.drawStr(32, 50, "data...");

    // Spinner
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(60, 63, spinnerFrames[spinnerIdx]);
    spinnerIdx = (spinnerIdx + 1) % 4;

    u8g2.sendBuffer();
}

// --------------------------------------------------------------------------
//  Setup
// --------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);  // USART1: PB6 (TX) / PB7 (RX)

    // Initialise the OLED display
    u8g2.begin();
    u8g2.setContrast(200);

    // Splash screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(18, 30, "SPEED-NET");
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 45, "Initializing...");
    u8g2.sendBuffer();

    delay(1500);
}

// --------------------------------------------------------------------------
//  Main loop
// --------------------------------------------------------------------------
void loop() {
    parseSerial();

    if (dataReceived) {
        drawDashboard();
    } else {
        drawWaiting();
    }

    // If no data for 30 s, show waiting again
    if (dataReceived && (millis() - lastDataMs > 30000)) {
        dataReceived = false;
    }

    delay(200);  // ~5 Hz refresh
}
