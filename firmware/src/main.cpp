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
//    DC   = PC9   (D/C_DISP)
//    RST  = PC8   (RST_DISP)
// --------------------------------------------------------------------------
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(
    U8G2_R0,
    PH0,   // CS  — CS_DISP
    PC9,   // DC  — D/C_DISP
    PC8    // RST — RST_DISP
);

//  Data from the Python API
struct NetData {
    float dl     = 0.0f;
    float ul     = 0.0f;
    float ping   = 0.0f;
    float rx     = 0.0f;
    float tx     = 0.0f;
    char  status[4] = "in";
};

static NetData  net;
static bool     dataReceived = false;
static uint32_t lastDataMs   = 0;

// Spinner animation frames
static const char* spinnerFrames[] = { "|", "/", "-", "\\" };
static uint8_t spinnerIdx = 0;


//  Non-blocking serial reader — accumulates chars until '\n'
static String serialBuf;

void parseSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            serialBuf.trim();
            if (serialBuf.length() == 0) continue;

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, serialBuf);
            serialBuf = "";

            if (err) continue;

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
        } else {
            serialBuf += c;
        }
    }
}

// --------------------------------------------------------------------------
//  Dashboard layout  (128 × 64)
// --------------------------------------------------------------------------
void drawDashboard() {
    u8g2.clearBuffer();

    // ── Title bar ──────────────────────────────────────────────────
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(2, 12, "SPEED-NET");

    char badge[8];
    if      (strcmp(net.status, "ok") == 0) snprintf(badge, sizeof(badge), "[OK]");
    else if (strcmp(net.status, "te") == 0) snprintf(badge, sizeof(badge), "[..]");
    else if (strcmp(net.status, "er") == 0) snprintf(badge, sizeof(badge), "[!!]");
    else                                    snprintf(badge, sizeof(badge), "[%s]", net.status);

    uint8_t badgeW = u8g2.getStrWidth(badge);
    u8g2.drawStr(126 - badgeW, 12, badge);

    u8g2.drawHLine(0, 14, 128);

    // Speed test results
    u8g2.setFont(u8g2_font_6x13_tr);
    char buf[32];

    u8g2.drawStr(2, 27, "\x19");
    snprintf(buf, sizeof(buf), "%.1f Mbps", net.dl);
    u8g2.drawStr(12, 27, buf);

    u8g2.drawStr(2, 39, "\x18");
    snprintf(buf, sizeof(buf), "%.1f Mbps", net.ul);
    u8g2.drawStr(12, 39, buf);

    u8g2.drawStr(2, 51, "~");
    snprintf(buf, sizeof(buf), "%.0f ms", net.ping);
    u8g2.drawStr(12, 51, buf);

    u8g2.drawHLine(0, 53, 128);

    // Real-time traffic
    u8g2.setFont(u8g2_font_5x8_tr);

    snprintf(buf, sizeof(buf), "RX:%.2f", net.rx);
    u8g2.drawStr(2, 62, buf);

    snprintf(buf, sizeof(buf), "TX:%.2f", net.tx);
    u8g2.drawStr(68, 62, buf);

    u8g2.sendBuffer();
}

//  Waiting screen

void drawWaiting() {
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(18, 20, "SPEED-NET");

    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(14, 38, "Waiting for");
    u8g2.drawStr(32, 50, "data...");

    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(60, 62, spinnerFrames[spinnerIdx]);
    spinnerIdx = (spinnerIdx + 1) % 4;

    u8g2.sendBuffer();
}

//  Setup

void setup() {
    Serial.begin(115200);

    u8g2.begin();
    u8g2.setContrast(200);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(18, 30, "SPEED-NET");
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 45, "Initializing...");
    u8g2.sendBuffer();

    delay(1000);
}

//  Main loop

void loop() {
    parseSerial();

    if (dataReceived) {
        drawDashboard();
    } else {
        drawWaiting();
    }

    if (dataReceived && (millis() - lastDataMs > 30000)) {
        dataReceived = false;
    }

    delay(200);
}
