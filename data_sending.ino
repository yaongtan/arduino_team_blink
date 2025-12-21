#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h> 

// ----------- 업로드 시 수정하기 -----------
#define WIFI_SSID "[WIFI id]"
#define WIFI_PASSWORD "[WIFI pw]"
#define API_KEY "[API Key]"
// ------------------------------------------
#define DATABASE_URL "https://blink-7a4c4-default-rtdb.asia-southeast1.firebasedatabase.app/"


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");

  // 시간 동기화
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 1000) delay(100);

  // Firebase 시작
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void sendToFirebase(String data) {
  // 데이터 분해 (LED,95.5,20.0)
  int first = data.indexOf(',');
  int second = data.indexOf(',', first + 1);
  if (first == -1 || second == -1) return;

  String type = data.substring(0, first);
  String acc = data.substring(first + 1, second);
  String timeStr = data.substring(second + 1);
  timeStr.trim(); // 시간 데이터 공백 제거

  // JSON 포장 및 전송
  FirebaseJson json;
  json.set("game_type", type);
  json.set("accuracy", acc.toFloat());
  json.set("play_time", timeStr.toFloat());
  json.set("timestamp", (double)time(nullptr) * 1000);

  if (Firebase.RTDB.pushJSON(&fbdo, "/game_logs", &json)) {
    Serial.println("Data Uploaded.");
  } else {
    Serial.println(fbdo.errorReason());
  }
}

// 메인루프
// 메가에서 데이터 전송되면 firebase로 전송
void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() > 0) sendToFirebase(data);
  }
}