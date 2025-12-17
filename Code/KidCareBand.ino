#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ---------- WiFi ----------
const char* ssid = "KidSafeBand";
const char* password = "kidsafe123";

// ---------- ThingSpeak ----------
String writeAPIKey = "0H45SXB08YIU61GG";   // For uploading GPS
String readAPIKey  = "IALLU5EHRDKRZN8R";    // For reading alerts
String channelID   = "3082453";

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Vibration Motor ----------
#define VIBRATION_PIN 23

// ---------- GPS ----------
TinyGPSPlus gps;
HardwareSerial SerialGPS(1); // use UART1
double latitude = 0.0, longitude = 0.0;

// ---------- Timing ----------
unsigned long lastCheck = 0;
const long checkInterval = 15000; // 15 sec

void setup() {
  Serial.begin(115200);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("KidCare Band Init...");
  display.display();

  // Vibration pin
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  display.println("WiFi OK");
  display.display();

  // GPS Serial (16 = RX, 17 = TX)
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
  // ---------- GPS Reading ----------
  while (SerialGPS.available() > 0) {
    char c = SerialGPS.read();
    gps.encode(c);

    if (gps.location.isUpdated()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      Serial.print("Lat: "); Serial.println(latitude, 6);
      Serial.print("Lng: "); Serial.println(longitude, 6);
    }
  }

  // ---------- Periodic Task ----------
  if (millis() - lastCheck > checkInterval) {
    lastCheck = millis();

    if (WiFi.status() == WL_CONNECTED) {
      // 1. Upload GPS to ThingSpeak (Fields 1 & 2)
      if (latitude != 0 && longitude != 0) {
        HTTPClient http;
        String url = "http://api.thingspeak.com/update?api_key=" + writeAPIKey +
                     "&field1=" + String(latitude, 6) +
                     "&field2=" + String(longitude, 6
                     );
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
          Serial.println("GPS uploaded to ThingSpeak");
        }
        http.end();
      }

      // 2. Read Alert Type (Field 3)
      String alertType = "";
      HTTPClient http;
      String url1 = "http://api.thingspeak.com/channels/" + channelID +
                    "/fields/3/last.txt?api_key=" + readAPIKey;
      http.begin(url1);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        alertType = http.getString();
        alertType.trim();
      }
      http.end();

      // 3. Read Alert Message (Field 4)
      String alertMsg = "";
      String url2 = "http://api.thingspeak.com/channels/" + channelID +
                    "/fields/4/last.txt?api_key=" + readAPIKey;
      http.begin(url2);
      httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        alertMsg = http.getString();
        alertMsg.trim();
      }
      http.end();

      // ---------- Handle Alerts ----------
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);

      if (alertType == "0") {
        Serial.println("Motivational Message Mode");

        display.clearDisplay();
        display.setCursor(20, 0);
        display.setTextSize(1);
        display.println("Kid Safe Band");
        display.setCursor(20, 20);
        display.setTextSize(1);
        display.println("Stay Happy,");
        display.setCursor(20, 35);
        display.println("Stay Hydrated,");
        display.setCursor(20, 50);
        display.println("Stay Healthy");
        display.display();

        // No vibration
        digitalWrite(VIBRATION_PIN, LOW);
      } 
      else if (alertType.length() > 0) {
        Serial.println("ALERT: " + alertType + " - " + alertMsg);

        display.clearDisplay();
        display.setCursor(20, 0);
        display.setTextSize(1);
        display.println("Kid Safe Band");
        display.setCursor(20, 20);
        display.setTextSize(1);
        display.println("ALERT RECEIVED");
        //display.println("Type: " + alertType);
        display.println("Msg: " + alertMsg);
        display.display();

        // Vibration alert
        digitalWrite(VIBRATION_PIN, HIGH);
        delay(1000);
        digitalWrite(VIBRATION_PIN, LOW);
      } 
      else {
        display.println("No Alerts");
      }

      // Show last GPS
      //display.println("----------------");
     // display.print("Lat: "); display.println(latitude, 4);
      //display.print("Lng: "); display.println(longitude, 4);

      display.display();
    }
  }
}
