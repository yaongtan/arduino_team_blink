#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h> 
#include <WiFiUdp.h>
#include <NTPClient.h> 

// ▼▼▼ 와이파이 정보만 수정하세요 ▼▼▼
const char* ssid = "YOUR_WIFI_ID";      
const char* password = "YOUR_WIFI_PW";  
// ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

const int sd_cs_pin = D8; 
SoftwareSerial megaSerial(D1, D2); // RX=D1, TX=D2

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 32400);

// blink_web.html 파일 열기
void handleRoot() {
  File file = SD.open("blink_web.html", "r");
  if (!file) {
    server.send(404, "text/plain", "blink_web.html not found on SD Card!");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleData() {
  File file = SD.open("game_log.txt", "r");
  if (!file) {
    server.send(200, "text/plain", ""); 
    return;
  }
  server.streamFile(file, "text/plain");
  file.close();
}

void saveLog(String dataFromMega) {
  timeClient.update();
  String timestamp = String(timeClient.getEpochTime()) + "000"; 
  String finalLog = timestamp + "," + dataFromMega;
  
  File file = SD.open("game_log.txt", FILE_WRITE);
  if (file) {
    file.println(finalLog);
    file.close();
    Serial.println("[Saved] " + finalLog);
  }
}

void setup() {
  Serial.begin(115200);
  megaSerial.begin(9600);
  if (!SD.begin(sd_cs_pin)) { Serial.println("SD Fail"); return; }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("\nServer IP: " + WiFi.localIP().toString());

  timeClient.begin();
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  if (megaSerial.available()) {
    String data = megaSerial.readStringUntil('\n');
    data.trim();
    if (data.length() > 0) saveLog(data);
  }
}