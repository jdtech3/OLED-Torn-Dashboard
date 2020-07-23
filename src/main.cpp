#include <string>

#include <Arduino.h>

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "settings.h"

ADC_MODE(ADC_VCC);      // for boot-up Vcc display
Adafruit_SSD1306 display(128, 64, &Wire, -1);  // height, width, lib, reset pin

uint64_t timeNow = 0;       // for (more) accurate loop delay
uint8_t arrowAnimPos = 0;   // offset position of arrow animation
uint16_t checkTimer = 60;   // timer for next API check
uint16_t timer;             // travel timer

// Small info data type
struct TravelInfo {
    String state;
    String destination;
    uint16_t timeLeft;
};

struct TravelInfo tInfo;    // "cached" travel info

const unsigned char jLogoBitmap [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
	0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x40, 0xf0, 0x00, 0x00, 0xc0, 0xf2, 0x00, 0x01, 0xc0, 0xf3, 0x00, 0x03, 0xc0, 0xf3, 0x80,
	0x07, 0xc0, 0xf3, 0xc0, 0x07, 0xc0, 0xf3, 0xc0, 0x07, 0x80, 0xf3, 0xc0, 0x07, 0x00, 0xf1, 0xc0,
	0x07, 0x80, 0xf3, 0xc0, 0x07, 0xc0, 0xf3, 0xc0, 0x07, 0xc0, 0xf3, 0xc0, 0x03, 0xd0, 0xf3, 0xc0,
	0x01, 0xd8, 0xf3, 0x80, 0x00, 0xdc, 0xf3, 0x00, 0x00, 0x5e, 0xf2, 0x00, 0x00, 0x1f, 0xf0, 0x00,
	0x00, 0x1f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x03, 0xf0, 0x00,
	0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Function skeletons
struct TravelInfo getTravelInfo();


void setup() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);     // I2C address
    display.display(); display.clearDisplay();     // clear splash screen

    display.setTextSize(1);               // set initial display settings
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 0);

    char vccBuf[24];
    sprintf(vccBuf, "Vcc: %.2fV", ESP.getVcc() / 1000.0);
    display.println(vccBuf);

    // Connect to WiFi (continously displaying Vcc and connection status)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        display.setCursor(0, 48);
        display.println(String(F("Connecting to: ")) + WIFI_SSID);
        display.display();

        delay(500);
    }

    // Display Logo, IP and Vcc
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(vccBuf);
    display.setCursor(0, 48);
    display.println(F("Connected."));
    display.println(String("IP: ") + WiFi.localIP().toString());
    display.drawBitmap(48, 16, jLogoBitmap, 32, 32, WHITE);  // use own splash screen

    display.display();  // display all of above

    delay(1000);

    display.clearDisplay(); display.display();  // clear splash stuff
}

void loop() {
    timeNow = millis();   // for timing

    // Update travel info "cache" every 60 secs and overwrite/recal timer
    if (checkTimer == 60) {
        struct TravelInfo tInfoTemp = getTravelInfo();

        timer = tInfoTemp.timeLeft;
        checkTimer = 0;

        // If display state/config will possibly be different, clear display and store new
        if (tInfoTemp.destination != tInfo.destination || tInfoTemp.state != tInfo.state) {
            tInfo = tInfoTemp;
            display.clearDisplay();
        }
    }
    else {
        checkTimer++;
    }

    // Travelling...
    if (timer > 0) {
        // - Destination display - //

        char buf[24];
        sprintf(buf, "> %s <", tInfo.destination.c_str());

        // Center "> destination <" within screen (64px max width)
        int16_t _s; uint16_t _u;
        uint16_t textWidth; display.getTextBounds(buf, 0, 0, &_s, &_s, &textWidth, &_u);
        display.setCursor((128 - textWidth) / 2, 0);

        display.setTextSize(1);
        display.println(buf);

        // - Travel timer - //

        // Calc hours, minutes, seconds
        uint16_t s = timer;
        uint8_t hours = s / 3600; s = s % 3600;
        uint8_t minutes = s / 60; s = s % 60;
        uint8_t seconds = s;

        // Format timer output string (0-pad minutes and seconds)
        sprintf(buf, "T: %d:%02d:%02d", hours, minutes, seconds);

        display.setCursor(0, 24);
        display.setTextSize(2);
        display.println(buf);

        timer--;  // decrement timer by a second

        // If timer hits 0 after above decrement, update info and clear display to make room to new info
        if (timer == 0) {
            tInfo = getTravelInfo(); checkTimer = 0;
            display.clearDisplay();
        }
    }
    else {
        // Not travelling, but arrived at overseas destination!
        if (tInfo.destination != "Torn") {
            display.setCursor(0, 0);
            display.setTextSize(1);
            display.println(String("Arrived: ") + tInfo.destination);
        }
        // Not travelling... in Torn
        else {
            display.setCursor(0, 0);
            display.setTextSize(1);
            display.println(F("At: Torn City"));
            display.println(F("No travel detected :("));
        }

        // Display status anyway
        display.setCursor(0, 24);
        display.println(F("Status:"));
        display.setTextSize(2);
        display.println(tInfo.state);
    }

    display.display();  // update display

    // More complicated but more accurate (?) version of delay()
    while (millis() < timeNow + 1000) {

        // Play arrow animation if travelling
        if (timer > 0) {
            if (millis() % 500 == 0) {
                display.setTextSize(1);
                display.setCursor(51, 56); display.print(F("     "));   // clear existing

                // Make arrow point left when returning, right when going and make animation go the corresponding way
                char arrow;
                if (tInfo.destination == F("Torn")) {
                    arrow = '<';
                    display.setCursor(71 - arrowAnimPos, 56);
                }
                else {
                    arrow = '>';
                    display.setCursor(51 + arrowAnimPos, 56);
                }
                display.print(arrow);

                display.display();

                arrowAnimPos == 20 ? arrowAnimPos = 0 : arrowAnimPos += 5;  // check number = (# of arrows - 1) * 5
            }
        }
    }
}

struct TravelInfo getTravelInfo() {
    HTTPClient httpClient;

    // Query API
    httpClient.begin(String(F("https://api.torn.com/user/?selections=travel,basic&key=")) + API_KEY, SSL_FINGERPRINT);
    int httpCode = httpClient.GET();

    if (httpCode > 0) {   // http err if negative code
        // Get response and close conn
        String payload = httpClient.getString();
        httpClient.end();

        // Parse
        const size_t capacity = JSON_OBJECT_SIZE(4) +
                                JSON_OBJECT_SIZE(5) +
                                JSON_OBJECT_SIZE(6) + 220;   // generated by ArduinoJson "assistant"
        StaticJsonDocument<capacity> doc;
        DeserializationError err = deserializeJson(doc, payload);

        // Return JSON error to be printed
        if (err) {
            TravelInfo ret = {"", String("JSON err: ") + err.c_str(), 0};
            return ret;
        }

        // Get info and return
        TravelInfo ret = {
            doc["status"]["state"].as<String>(),
            doc["travel"]["destination"].as<String>(),
            doc["travel"]["time_left"].as<uint16_t>()
        };
        return ret;
    }

    // Return HTTP error to be printed
    else {
        httpClient.end(); // close conn!

        TravelInfo ret = {"", String("HTTP error: ") + httpClient.errorToString(httpCode).c_str(), 0};
        return ret;
    }
}
