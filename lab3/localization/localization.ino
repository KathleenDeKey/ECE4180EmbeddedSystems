#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Some environmental constants that I manually tuned in the lab room
const float EMA_ALPHA           = 0.25f;
const float PATH_LOSS_EXPONENT  = 2.2f;

const char *ssid = "ESP32Locator";
const char *password = "locator123";

NetworkServer server(80);
BLEScan* pBLEScan;

struct Point {
  float x;
  float y;
};

struct AnchorTrack {
  String mac;
  String name;
  float x;
  float y;
  int   txPower1m;

  float         filteredRssi;
  float         estimatedDist;
  uint32_t      sampleCount;
  unsigned long lastSeen;
};

AnchorTrack anchors[3] = {
  {"fc:01:2c:ff:95:d6", "Beacon A", 0.0f,  0.0f,  -59, -100.0f, 0.0f, 0, 0},
  {"58:8c:81:3b:c1:6a", "Beacon B", 0.0f, 5.48f, -59, -100.0f, 0.0f, 0, 0},
  {"40:4c:ca:5f:82:a6", "Beacon C", 2.41f, 2.09f, -59, -100.0f, 0.0f, 0, 0}
};

// Weighted Centroid Localization (WCL)
Point approximateTrilateration(const Point anchors[], const float distances[], int count) {
  float totalWeight = 0.0f;
  Point est = {0.0f, 0.0f};

  for (int i = 0; i < count; i++) {
    float weight = 1.0f / (distances[i] + 1e-6f);
    est.x += anchors[i].x * weight;
    est.y += anchors[i].y * weight;
    totalWeight += weight;
  }

  est.x /= totalWeight;
  est.y /= totalWeight;
  return est;
}

// ---------- BLE scan callback: runs for every advertisement received ----------
class BeaconCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {

    Serial.print("Found: ");
    Serial.print(advertisedDevice.getAddress().toString().c_str());
    Serial.print("  name: \"");
    Serial.print(advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "(none)");
    Serial.print("\"  rssi: ");
    Serial.println(advertisedDevice.getRSSI());

    String mac = String(advertisedDevice.getAddress().toString().c_str());
    mac.toLowerCase();

    int idx;

    for (int a = 0; a < 3; a++) {
      if (mac == anchors[a].mac) { idx = a; break; }
    }
    if (idx < 0) return;                       // ignore every other device

    int rawRssi = advertisedDevice.getRSSI();
    AnchorTrack& anc = anchors[idx];

    // Exponential Moving Average (EMA) Filtering
    if (anc.sampleCount == 0 || anc.filteredRssi < -95.0f) {
      anc.filteredRssi = (float)rawRssi;
    } else {
      anc.filteredRssi = (EMA_ALPHA * (float)rawRssi) + ((1.0f - EMA_ALPHA) * anc.filteredRssi);
    }

    anc.sampleCount++;
    anc.lastSeen = millis();

    // Log-Distance Path Loss Model
    float exponent = ((float)anc.txPower1m - anc.filteredRssi) / (10.0f * PATH_LOSS_EXPONENT);
    anc.estimatedDist = pow(10.0f, exponent);

    Serial.print(mac);
    Serial.print(" rawRssi: ");
    Serial.print(rawRssi);
    Serial.print("  dist: ");
    Serial.println(anc.estimatedDist, 2);
  }
};

// ---------- Web page ----------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{margin:0;background:#111;color:#eee;font-family:sans-serif;text-align:center}
canvas{background:#1c1c1c;width:95vw;max-width:400px}</style></head><body>
<p id="t">...</p><canvas id="c" width="400" height="400"></canvas>
<script>
const c=document.getElementById('c'),g=c.getContext('2d');
async function tick(){
 try{
  const d=await (await fetch('/pos')).json();
  const xs=d.a.map(a=>a.x),ys=d.a.map(a=>a.y);
  const x0=Math.min(...xs)-1,x1=Math.max(...xs)+1,y0=Math.min(...ys)-1,y1=Math.max(...ys)+1;
  const X=v=>20+(v-x0)/(x1-x0)*360, Y=v=>380-(v-y0)/(y1-y0)*360;
  g.clearRect(0,0,400,400);
  g.fillStyle='#4da3ff';
  d.a.forEach(a=>{g.fillRect(X(a.x)-5,Y(a.y)-5,10,10);g.fillText(a.n,X(a.x)+8,Y(a.y)-8);});
  if(d.fix){g.fillStyle='#f44';g.beginPath();g.arc(X(d.x),Y(d.y),8,0,7);g.fill();
   document.getElementById('t').textContent='x='+d.x.toFixed(2)+' y='+d.y.toFixed(2);}
  else document.getElementById('t').textContent='No fix';
 }catch(e){}
}
setInterval(tick,500);tick();
</script></body></html>
)rawliteral";

String buildPosJson() {
  Point pts[3];
  float dist[3];
  int cnt = 0;
  String j = "{\"a\":[";
  for (int i = 0; i < 3; i++) {
    if (i) j += ",";
    j += "{\"n\":\"" + anchors[i].name + "\",\"x\":" + String(anchors[i].x) +
         ",\"y\":" + String(anchors[i].y) + "}";
    if (anchors[i].sampleCount > 0 && millis() - anchors[i].lastSeen < 5000) {
      pts[cnt] = {anchors[i].x, anchors[i].y};
      dist[cnt] = anchors[i].estimatedDist;
      cnt++;
    }
  }
  j += "],\"fix\":";
  if (cnt >= 2) {
    Point p = approximateTrilateration(pts, dist, cnt);
    j += "true,\"x\":" + String(p.x) + ",\"y\":" + String(p.y);
  } else {
    j += "false";
  }
  j += "}";
  return j;
}

void handleClient() {
  NetworkClient client = server.accept();
  if (!client) return;

  String currentLine = "";
  String requestLine = "";
  unsigned long start = millis();

  while (client.connected() && millis() - start < 1000) {
    if (!client.available()) continue;
    char c = client.read();
    if (c == '\n') {
      if (currentLine.length() == 0) {
        if (requestLine.startsWith("GET /pos")) {
          String body = buildPosJson();
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:application/json");
          client.println("Connection: close");
          client.println();
          client.print(body);
        } else {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          client.print(PAGE);
        }
        client.println();
        break;
      } else {
        if (requestLine.length() == 0) requestLine = currentLine;
        currentLine = "";
      }
    } else if (c != '\r') {
      currentLine += c;
    }
  }
  client.stop();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  WiFi.mode(WIFI_AP);       // Wi-Fi is only used to host the page now

  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.begin();
  Serial.println("Server started");

  delay(1000);

  // BLE scanning
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new BeaconCallbacks(), true);  // true = report duplicates (needed for live RSSI)
  pBLEScan->setActiveScan(true);   // active scan so the device name (scan response) is received
  pBLEScan->setInterval(160);
  pBLEScan->setWindow(40);
  pBLEScan->start(0, nullptr, false);   // 0 = scan forever, non-blocking
}

void loop() {
  handleClient();

  // Duplicates are reported, so clear stored results regularly to avoid running out of RAM
  static unsigned long lastClear = 0;
  if (millis() - lastClear > 10000) {
    pBLEScan->clearResults();
    lastClear = millis();
  }
}