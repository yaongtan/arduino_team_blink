#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SD.h>

// ------------------- [와이파이 설정] -------------------
const char* ssid = "YOUR_WIFI_ID";      // 와이파이 이름
const char* password = "YOUR_WIFI_PW";  // 와이파이 비밀번호
// -----------------------------------------------------

const int sd_cs_pin = D8; // SD카드 CS핀
ESP8266WebServer server(80);

// HTML 폼 (파일 업로드 화면)
const char* htmlIndex = 
"<form method='POST' action='/upload' enctype='multipart/form-data'>"
"<input type='file' name='upload'><input type='submit' value='Upload'></form>"
"<p>File List:</p><div id='list'></div>"
"<script>fetch('/list').then(r=>r.text()).then(t=>document.getElementById('list').innerHTML=t)</script>";

void handleRoot() {
  server.send(200, "text/html", htmlIndex);
}

// 파일 목록 보여주기
void handleFileList() {
  String str = "";
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    str += String(entry.name()) + " (" + String(entry.size()) + " bytes)<br>";
    entry.close();
  }
  server.send(200, "text/plain", str);
}

// 파일 업로드 처리
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("Upload Start: "); Serial.println(filename);
    // 파일 열기 (쓰기 모드, 기존 내용 덮어씀)
    File file = SD.open(filename, FILE_WRITE); 
    if (!file) Serial.println("File Create Failed");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    File file = SD.open(filename, FILE_WRITE); 
    if (file) {
      file.write(upload.buf, upload.currentSize); // 데이터 쓰기
      file.close();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/html", "Upload Success! <a href='/'>Go Back</a>");
    Serial.print("Upload Size: "); Serial.println(upload.totalSize);
  }
}

void setup() {
  Serial.begin(115200);
  
  // SD 카드 초기화
  if (!SD.begin(sd_cs_pin)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  Serial.println("SD Card Initialized");

  // 와이파이 연결
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // 서버 설정
  server.on("/", HTTP_GET, handleRoot);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/upload", HTTP_POST, [](){ server.send(200); }, handleFileUpload);

  server.begin();
  Serial.println("Upload Server Started");
}

void loop() {
  server.handleClient();
}