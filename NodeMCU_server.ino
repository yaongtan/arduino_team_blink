#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// ▼▼▼ 와이파이 설정 ▼▼▼
const char* ssid = "YOUR_WIFI_ID";
const char* password = "YOUR_WIFI_PW";
// ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

// 메가와 통신 (RX=D1, TX=D2)
SoftwareSerial megaSerial(5, 4); // GPIO 5(D1), 4(D2)

ESP8266WebServer server(80);

const char MAIN_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BLINK_web service</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: sans-serif; padding: 20px; background: #f4f7f6; }
        .container { max-width: 1000px; margin: 0 auto; }
        .chart-container { background: white; padding: 20px; margin-bottom: 20px; border-radius: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>[실시간] 게임 기록 대시보드</h1>
        
        <div class="chart-container">
            <canvas id="accuracyChart"></canvas>
        </div>
        
        <div class="chart-container">
            <h3>상세 기록</h3>
            <table border="1" style="width:100%; border-collapse: collapse;">
                <thead><tr><th>타입</th><th>정확도</th><th>시간</th></tr></thead>
                <tbody id="logTableBody"></tbody>
            </table>
        </div>
    </div>

<script>
    // --- [데이터 로드 함수] ---
    async function loadData() {
        try {
            const response = await fetch('/history'); 
            const text = await response.text();
            
            console.log("받은 데이터:", text); // 디버깅용

            // 데이터 파싱 및 차트 그리기
            const lines = text.trim().split('\n');
            const logs = [];

            lines.forEach(line => {
                const parts = line.split(',');
                if (parts.length >= 3) {
                    logs.push({
                        type: parts[0],
                        score: parseFloat(parts[1]),
                        time: parseFloat(parts[2])
                    });
                }
            });

            updateDashboard(logs);

        } catch (error) {
            console.error("데이터 로드 실패:", error);
            alert("데이터를 가져오는 중 오류가 발생했습니다.");
        }
    }

    function updateDashboard(logs) {
        // 테이블 갱신
        const tbody = document.getElementById('logTableBody');
        tbody.innerHTML = "";
        
        // 차트 데이터 준비
        const labels = logs.map((_, i) => i + 1);
        const ledScores = logs.map(l => l.type === 'LED' ? l.score : null);
        const rhythmScores = logs.map(l => l.type === 'RHYTHM' ? l.score : null);

        // 테이블 채우기
        logs.forEach(log => {
            const row = `<tr><td>${log.type}</td><td>${log.score}</td><td>${log.time}</td></tr>`;
            tbody.innerHTML += row;
        });

        // 차트 그리기 (간소화됨, 본인 코드 넣으셔도 됩니다)
        new Chart(document.getElementById('accuracyChart'), {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    { label: 'LED 정확도', data: ledScores, borderColor: '#FF6384', spanGaps: true },
                    { label: '리듬 정확도', data: rhythmScores, borderColor: '#36A2EB', spanGaps: true }
                ]
            }
        });
    }

    window.onload = loadData;
</script>
</body>
</html>

)rawliteral"; 
// ▲▲▲▲▲ HTML 끝 ▲▲▲▲▲


void setup() {
  Serial.begin(115200);
  megaSerial.begin(9600); // 메가와 통신 속도

  WiFi.begin(ssid, password);
  Serial.print("WiFi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: ");
  Serial.println(WiFi.localIP());

  // 1. 웹페이지 접속 시 HTML 코드 보여주기
  server.on("/", []() {
    server.send(200, "text/html", MAIN_PAGE);
  });

  // 2. 데이터 요청 (/history)이 오면 메가한테 받아오기
  server.on("/history", []() {
    Serial.println("[Web] Request History...");
    
    // (1) 메가한테 'R' 명령 전송
    megaSerial.write('R'); 
    
    // (2) 메가 응답 기다리기 (최대 4초)
    String fullData = "";
    unsigned long timeout = millis();
    
    while (millis() - timeout < 4000) {
      if (megaSerial.available()) {
        char c = megaSerial.read();
        fullData += c;
        timeout = millis(); // 데이터가 들어오는 동안은 타임아웃 연장
      }
      yield(); // 와이파이 끊김 방지
    }
    
    Serial.println("[Web] Data Received & Sent to Client");
    // (3) 받은 데이터를 웹브라우저로 전송
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", fullData);
  });

  server.begin();
  Serial.println("Server Started");
}

void loop() {
  server.handleClient();
}