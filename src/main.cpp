#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "secrets.h"

// ── Display pins (ESP32-C3 SuperMin → ST7735S 80×160 TFT) ────────────────────
// Pins map straight onto one column of the board — plug in order, no jumpers:
//   Display:  GND  VCC  SCL  SDA  RES  DC   CS   BLK
//   Board:    GND  3V3   4    3    2    1    0   3V3
#define TFT_SCK   4   // SCL
#define TFT_MOSI  3   // SDA
#define TFT_RST   2   // RES
#define TFT_DC    1
#define TFT_CS    0
// BLK → 3V3 (last pin on the column)

#define SCREEN_W  160
#define SCREEN_H   80

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ── WiFi ──────────────────────────────────────────────────────────────────────
// Credentials live in include/secrets.h (gitignored)

#define WIFI_TIMEOUT_MS    10000
#define FETCH_INTERVAL_MS  60000   // fetch every 60 seconds

// ── State ─────────────────────────────────────────────────────────────────────
static double audPrice  = 0;
static double prevAud   = 0;
static double usdPrice  = 0;
static double prevUsd   = 0;
static bool   hasPrice  = false;

// ── Helpers ───────────────────────────────────────────────────────────────────

String formatPrice(double p, const char* symbol) {
    long whole = (long)p;
    String digits = String(whole);
    String out;
    int len = digits.length();
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return String(symbol) + out;
}

// Centre a size-2 string horizontally and print it at the given y
void printCentred2(const String& s, int y, uint16_t color) {
    tft.setTextSize(2);
    tft.setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    int xPos = ((int)SCREEN_W - (int)w) / 2;
    if (xPos < 0) xPos = 0;
    tft.setCursor(xPos, y);
    tft.print(s);
}

void showMessage(const char* line1, const char* line2 = nullptr) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(4, 24);
    tft.println(line1);
    if (line2) {
        tft.setCursor(4, 40);
        tft.println(line2);
    }
}

void updateDisplay(bool fetchOk) {
    tft.fillScreen(ST77XX_BLACK);

    // ── Header ────────────────────────────────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(4, 5);
    tft.print("BITCOIN");

    // Trend indicator or error flag (top-right corner)
    if (hasPrice && prevAud > 0) {
        uint16_t col;
        const char* sym;
        if      (audPrice > prevAud) { sym = "+"; col = ST77XX_GREEN; }
        else if (audPrice < prevAud) { sym = "-"; col = ST77XX_RED;   }
        else                         { sym = "="; col = ST77XX_WHITE; }
        tft.setTextColor(col);
        tft.setCursor(SCREEN_W - 7, 5);
        tft.print(sym);
    }
    if (!fetchOk) {
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(SCREEN_W - 7, 5);
        tft.print("!");
    }

    // Divider line
    tft.drawFastHLine(0, 16, SCREEN_W, ST77XX_YELLOW);

    // ── Prices ────────────────────────────────────────────────────────────────
    if (!hasPrice) {
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(20, 36);
        tft.print("Fetching...");
        return;
    }

    uint16_t priceColor;
    if      (prevAud <= 0)       priceColor = ST77XX_WHITE;
    else if (audPrice > prevAud) priceColor = ST77XX_GREEN;
    else if (audPrice < prevAud) priceColor = ST77XX_RED;
    else                         priceColor = ST77XX_WHITE;

    // AUD row
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(4, 20);
    tft.print("AUD");
    printCentred2(formatPrice(audPrice, "A$"), 29, priceColor);

    // USD row
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(4, 50);
    tft.print("USD");
    printCentred2(formatPrice(usdPrice, "$"), 59, priceColor);
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

bool connectWiFi() {
    showMessage("Connecting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) return false;
        delay(100);
    }
    Serial.printf("WiFi connected in %lums\n", millis() - start);
    return true;
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

bool fetchPrice() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=aud,usd");
    http.setTimeout(10000);
    http.addHeader("User-Agent", "ESP32-C3-BTC/1.0");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    Serial.println(payload);

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("JSON parse error");
        return false;
    }

    double aud = doc["bitcoin"]["aud"].as<double>();
    double usd = doc["bitcoin"]["usd"].as<double>();
    if (aud <= 0 || usd <= 0) return false;

    prevAud  = audPrice;
    prevUsd  = usdPrice;
    audPrice = aud;
    usdPrice = usd;
    hasPrice = true;

    Serial.printf("BTC/AUD: A$%.0f  BTC/USD: $%.0f\n", aud, usd);
    return true;
}

// ── Entry point ───────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    SPI.begin(TFT_SCK, -1, TFT_MOSI);
    tft.initR(INITR_MINI160x80);  // ST7735S 80×160
    tft.setRotation(3);           // landscape, cable exits right
    tft.fillScreen(ST77XX_BLACK);

    if (!connectWiFi()) {
        showMessage("WiFi failed!", "Check SSID/pass");
        return;
    }

    bool ok = fetchPrice();
    updateDisplay(ok);
}

void loop() {
    delay(FETCH_INTERVAL_MS);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        WiFi.disconnect();
        connectWiFi();
    }

    bool ok = fetchPrice();
    updateDisplay(ok);
}
