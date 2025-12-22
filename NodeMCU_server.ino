#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <LittleFS.h>   // 파일 시스템
#include <WiFiUdp.h>
#include <NTPClient.h>  // 시간 가져오기용
#include "webpage.h"

// ▼▼▼ 와이파이 설정 (수정 필수) ▼▼▼
const char* ssid = "Gamtwi's phone";
const char* password = "potato!!!!!";
// ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

// 메가와 통신 (D1=RX, D2=TX) -> Mega의 TX1(18), RX1(19)과 연결
SoftwareSerial megaSerial(5, 4); 

ESP8266WebServer server(80);

// 시간 설정을 위한 NTP 객체 (한국 시간: UTC+9 -> 32400초)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);



void setup() {
  Serial.begin(115200);     // 컴퓨터와 디버깅용
  megaSerial.begin(9600);   // 메가와 통신용
  
  // LittleFS(파일시스템) 마운트
  if(!LittleFS.begin()){
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // 와이파이 연결
  WiFi.begin(ssid, password);
  Serial.print("WiFi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: ");
  Serial.println(WiFi.localIP());

  // NTP 시작
  timeClient.begin();
  timeClient.update();

  // 웹 서버 경로 설정
  server.on("/", []() {
    server.send(200, "text/html", MAIN_PAGE);
  });

  // 데이터 요청 시 log.txt 파일 읽어서 보내줌
  server.on("/data", []() {
    File file = LittleFS.open("/log.txt", "r");
    if (!file) {
      server.send(200, "text/plain", "");
      return;
    }
    server.streamFile(file, "text/plain");
    file.close();
  });

  // 데이터 초기화용 (필요시 브라우저 주소창에 /clear 입력)
  server.on("/clear", []() {
    LittleFS.remove("/log.txt");
    server.send(200, "text/plain", "Log Cleared");
  });

  server.begin();
  Serial.println("Server Started");
}

void loop() {
  server.handleClient();
  timeClient.update(); // 시간 업데이트

  // 메가에서 데이터가 들어오면 읽어서 파일에 저장
  if (megaSerial.available()) {
    String data = megaSerial.readStringUntil('\n');
    data.trim(); // 공백 제거

    if (data.length() > 0) {
      unsigned long epochTime = timeClient.getEpochTime(); // 현재 유닉스 시간
      
      // 파일에 "시간,데이터" 형식으로 저장
      File file = LittleFS.open("/log.txt", "a"); // 'a'는 append(이어쓰기)
      if (file) {
        file.print(epochTime); // 타임스탬프 추가
        file.print(",");
        file.println(data);    // 예: 1703243123,LED,90.5,20.1
        file.close();
        Serial.print("Data Saved: ");
        Serial.println(data);
      } else {
        Serial.println("File Open Failed");
      }
    }
  }
}