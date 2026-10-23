/*
  ESP32 -> phone hotspot -> HTTPS GET -> parse JSON -> print date and time

  API: https://worldtimeapi.org/api/timezone/<Area>/<Location>  (no API key)
  Example response (trimmed):
    { "timezone": "America/New_York", "abbreviation": "EDT",
      "datetime": "2026-10-22T14:30:05.123456-04:00", ... }

  Libraries needed (Arduino IDE > Library Manager):
    - ArduinoJson (by Benoit Blanchon, version 7.x)
  WiFi, HTTPClient and WiFiClientSecure come with the ESP32 board package.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ---- Change these ---------------------------------------------------------
const char *ssid = "iPhone (173)";
const char *password = "hello123";
const char *timeUrl = "https://utctime.app/api/now";
const unsigned long pollPeriodMs = 10000;
// ---------------------------------------------------------------------------

void fetchCurrentTime() {
  WiFiClientSecure client;
  client.setInsecure();  // skips certificate checking: fine for a public time API, not for private data

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, timeUrl)) {
    Serial.println("HTTP begin failed");
    return;
  }

  int code = http.GET();  // the HTTP GET request
  Serial.printf("HTTP GET status = %d\n", code);

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("JSON parse failed: ");
      Serial.println(err.c_str());
    } else {
      //Serial.print(doc);
      String datetime = doc["utc_iso"] | "";      // "2026-10-22T14:30:05.123456-04:00"
      // String tz = doc["timezone"] | "";
      // String abbr = doc["abbreviation"] | "";
      
      Serial.println(datetime);
      if (datetime.length() >= 19) {
        Serial.println("-----------------------------");
        
        Serial.println("-----------------------------");
      } else {
        Serial.println("\"datetime\" field missing or malformed");
      }
    }
  } else if (code < 0) {
    Serial.printf("Request failed: %s\n", http.errorToString(code).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);  // station mode
  WiFi.begin(ssid, password);

  Serial.printf("Connecting to hotspot \"%s\"", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost, reconnecting...");
    WiFi.reconnect();
    delay(3000);
    return;
  }

  fetchCurrentTime();
  delay(pollPeriodMs);
}