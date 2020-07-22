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

uint64_t timeNow = 0;   // for (more) accurate loop delay
uint16_t timer;         // travel timer
String state;           // player state (i.e. "Okay")

// Small info data type
struct TravelInfo {
    String state;
    String destination;
    uint16_t timeLeft;
};

// Functions
struct TravelInfo getTravelInfo();


void setup() {
    pinMode(0, INPUT_PULLUP);   // use FLASH button

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);     // I2C address
    display.display();
    display.clearDisplay();   // clear splash screen

    display.setTextSize(1);               // set initial display settings
    display.setTextColor(WHITE, BLACK);

    // Connect to WiFi (continously displaying Vcc and connection status)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        display.setCursor(0, 0);
        display.print(ESP.getVcc() / 1000.0); display.println("V");

        display.setCursor(0, 48);
        display.println(String("Connecting to: ") + WIFI_SSID);
        display.display();

        delay(500);
    }

    // Display IP
    display.clearDisplay();
    display.setCursor(0, 48);
    display.println("Connected.");
    display.println(String("IP: ") + WiFi.localIP().toString());
    display.display();

    delay(1000);

    // Query Torn travel info and set vars
    struct TravelInfo tInfo = getTravelInfo();
    timer = tInfo.timeLeft;
    state = tInfo.state;

    display.clearDisplay();
    display.setCursor(0, 0);

    // Travelling...
    if (timer > 0) {
        display.println(String("> ") + tInfo.destination);
    }
    // Arrived at destination!
    else if (tInfo.destination != "Torn") {
        display.println(String("Arrived: ") + tInfo.destination);
    }
    // In Torn, not travelling
    else {
        display.println("At: Torn City");
        display.println("No travel detected :(");
    }

    display.display();  // display all of above
}

void loop() {
    timeNow = millis();   // for timing

    // Sleep indefinitely (until RESET pressed) if FLASH pressed
    if (digitalRead(0) == LOW) {
        display.clearDisplay();
        ESP.deepSleep(0);
    }

    // Set display settings for big text
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);

    // Travelling... display travel timer
    if (timer > 0) {
        // Calc hours, minutes, seconds
        uint16_t s = timer;
        uint8_t hours = s / 3600; s = s % 3600;
        uint8_t minutes = s / 60; s = s % 60;
        uint8_t seconds = s;

        // Format timer output string (0-pad minutes and seconds)
        char buf[12];
        sprintf(buf, "T: %d:%02d:%02d", hours, minutes, seconds);

        display.setCursor(0, 24);
        display.println(buf);

        timer--;  // decrement timer by a second
    }
    // Not travelling... display status instead
    else {
        display.setCursor(0, 20);   // move cursor slightly higher for extra line

        display.setTextSize(1);
        display.println("Status:");
        display.setTextSize(2);
        display.println(state);
    }

    display.display();  // update display

    while (millis() < timeNow + 1000) {}  // loop checks for whether 1 sec has passed
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
            doc["travel"]["time_left"].as<int>()
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
