/*
  WiFi Access Point + web button that cycles the built-in RGB LED:
  orange -> red -> blue -> green -> yellow -> purple -> orange

  1. Connect to the access point "yourAP"
  2. Open http://192.168.4.1/ in your browser
  3. Tap the button to switch to the next color

  Based on the arduino-esp32 WiFiAccessPoint example.
  Requires arduino-esp32 core 3.x (neopixelWrite / RGB_BUILTIN).
*/

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 8  // onboard WS2812 on the ESP32-C6 DevKitC-1
#endif

// Set these to your desired credentials (password must be 8+ characters).
const char *ssid = "yourAP";
const char *password = "yourPassword";

NetworkServer server(80);

struct LedColor {
  const char *name;
  uint8_t r, g, b;        // what the LED shows (kept dim, the onboard LED is bright)
  const char *cssColor;   // what the web page uses for its background
};

const LedColor colors[] = {
  {"Orange", 60, 12,  0, "#ff6a00"},
  {"Red",    60,  0,  0, "#e53935"},
  {"Blue",    0,  0, 60, "#1e63ff"},
  {"Green",   0, 60,  0, "#2ecc40"},
  {"Yellow", 60, 40,  0, "#ffd400"},
  {"Purple", 40,  0, 40, "#8e24aa"}
};
const int NUM_COLORS = sizeof(colors) / sizeof(colors[0]);
int colorIndex = 0;

void setColor(int index) {
  neopixelWrite(RGB_BUILTIN, colors[index].r, colors[index].g, colors[index].b);
}

void sendPage(NetworkClient &client) {
  const LedColor &c = colors[colorIndex];
  const LedColor &next = colors[(colorIndex + 1) % NUM_COLORS];

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  client.print("<!DOCTYPE html><html><head>");
  client.print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.print("<title>LED Color</title><style>");
  client.print("body{font-family:sans-serif;text-align:center;margin:0;padding:40px 16px;");
  client.print("background:#111;color:#fff}");
  client.print(".swatch{width:140px;height:140px;border-radius:50%;margin:24px auto;");
  client.print("background:");
  client.print(c.cssColor);
  client.print(";box-shadow:0 0 40px ");
  client.print(c.cssColor);
  client.print("}");
  client.print(".btn{display:inline-block;padding:18px 36px;font-size:20px;color:#fff;");
  client.print("background:#333;border:2px solid #fff;border-radius:12px;text-decoration:none}");
  client.print(".btn:active{background:#555}");
  client.print("</style></head><body>");
  client.print("<h1>LED Color</h1>");
  client.print("<div class=\"swatch\"></div>");
  client.print("<h2>");
  client.print(c.name);
  client.print("</h2>");
  client.print("<a class=\"btn\" href=\"/next\">Next color: ");
  client.print(next.name);
  client.print("</a>");
  client.print("</body></html>");
  client.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  setColor(colorIndex);  // start on orange

  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");
}

void loop() {
  NetworkClient client = server.accept();  // listen for incoming clients

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    String requestLine = "";  // first line of the request, e.g. "GET /next HTTP/1.1"

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        if (c == '\n') {
          if (requestLine.length() == 0) {
            requestLine = currentLine;  // remember the first line
          }

          // Blank line = end of the HTTP request headers, so respond.
          if (currentLine.length() == 0) {
            if (requestLine.startsWith("GET /next")) {
              colorIndex = (colorIndex + 1) % NUM_COLORS;
              setColor(colorIndex);
              // Redirect back to the main page so refreshing doesn't change the color again.
              client.println("HTTP/1.1 303 See Other");
              client.println("Location: /");
              client.println("Connection: close");
              client.println();
            } else {
              sendPage(client);
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}