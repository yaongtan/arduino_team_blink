#include <LiquidCrystal.h>

// LCD가 연결되어 있다면 사용 (연결 안 되어 있어도 코드는 돌아갑니다)
LiquidCrystal lcd(44, 45, 46, 47, 48, 49); 

// ------------------- [핀 설정] -------------------
const int mic_pin = A0;        // 마이크 센서 (A1)
const int buzzer_pin = 12;     // 부저 (12)
const int mic_threshold = 500; // ★감도 조절 (400~800)

// ------------------- [변수 설정] -------------------
int rhythm[10];      // 문제 리듬
int my_rhythm[10];   // 내가 친 리듬
const int rhythm_num = 5; // 리듬 개수

// 타이머 구조체
struct Timer {
  unsigned long previous = 0;
  unsigned long interval;
  Timer(unsigned long p = 0, unsigned long i = 1000) {
    previous = p;
    interval = i;
  }
};
Timer buzzer_timer(0, 1000);

// ------------------- [함수] -------------------

// 시간 간격 체크
bool per_time_interval(Timer& t) {
  unsigned long time_current = millis();
  if (time_current - t.previous >= t.interval) {
    t.previous = time_current;
    return true;
  }
  return false;
}

// 부저 소리
void buzzer_on() {
  tone(buzzer_pin, 262, 100); // '도' 소리 0.1초
}

// 랜덤 리듬 생성
void make_random_rhythm(int num) {
  Serial.print("New Rhythm: ");
  for (int i = 0; i < num; i++) {
    rhythm[i] = random(400, 800); // 0.4초 ~ 0.8초 간격
    Serial.print(rhythm[i]);
    Serial.print(" ");
  }
  Serial.println();
}

// ★ 박수 감지 함수
bool clap_ifdetected() {
  int micValue = analogRead(mic_pin);
  
  if (micValue > mic_threshold) {
    // Serial.println("!!! CLAP !!!"); // 디버깅용
    delay(150); // 중복 입력 방지
    return true;
  }
  return false;
}

// ------------------- [SETUP] -------------------
void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(mic_pin, INPUT);
  
  randomSeed(analogRead(0));
  
  Serial.println("--- Rhythm Game Test Mode ---");
  Serial.println("Only Mic & Buzzer needed.");
  delay(1000);
}

// ------------------- [LOOP] -------------------
void loop() {
  // 1. 게임 초기화
  make_random_rhythm(rhythm_num);
  
  lcd.clear();
  lcd.print("Listen...");
  Serial.println("\n[Phase 1] Listen to the rhythm");
  
  int rhythm_index = 0;
  int player_index = 0;
  bool listen_phase = true;
  bool start_input = false;
  unsigned long last_input_time = 0;

  buzzer_on(); // 첫 음 시작
  buzzer_timer.previous = millis();
  buzzer_timer.interval = rhythm[0];

  // 2. 게임 진행 루프
  while (player_index < rhythm_num) {
    
    // [듣기 구간]
    if (listen_phase) {
      if (per_time_interval(buzzer_timer)) {
        rhythm_index++;
        
        if (rhythm_index >= rhythm_num) {
          listen_phase = false; // 듣기 끝
          Serial.println("[Phase 2] CLAP to Start!");
          lcd.setCursor(0, 0);
          lcd.print("Clap to Start!");
        } else {
          buzzer_on(); // 비트 소리
          buzzer_timer.previous = millis();
          buzzer_timer.interval = rhythm[rhythm_index];
        }
      }
    } 
    // [입력 구간]
    else {
      // 첫 박수 대기
      if (!start_input) {
        if (clap_ifdetected()) {
          start_input = true;
          last_input_time = millis();
          Serial.println(">> Game Started! Keep clapping!");
          lcd.setCursor(0, 1);
          lcd.print("Go!");
        }
      } 
      // 이후 박수 기록
      else {
        if (clap_ifdetected()) {
          unsigned long now = millis();
          my_rhythm[player_index] = now - last_input_time;
          
          Serial.print("Input [");
          Serial.print(player_index);
          Serial.print("]: ");
          Serial.print(my_rhythm[player_index]);
          Serial.println(" ms");

          last_input_time = now;
          player_index++;
        }
      }
    }
  }

  // 3. 결과 계산 및 출력
  long error_sum = 0;
  Serial.println("\n--- Result ---");
  for (int i = 0; i < rhythm_num; i++) {
    int diff = abs(rhythm[i] - my_rhythm[i]);
    error_sum += diff;
    Serial.print("Rhythm: "); Serial.print(rhythm[i]);
    Serial.print(" / Me: "); Serial.print(my_rhythm[i]);
    Serial.print(" (Diff: "); Serial.print(diff); Serial.println(")");
  }

  float avg_error = error_sum / (float)rhythm_num;
  Serial.print(">> Average Error: ");
  Serial.println(avg_error);
  
  lcd.clear();
  lcd.print("Avg Error:");
  lcd.setCursor(0, 1);
  lcd.print(avg_error);

  // 4. [변경됨] 엔터 키 입력 대기
  Serial.println("\n==================================");
  Serial.println("Press [ENTER] in Serial Monitor to restart...");
  Serial.println("==================================");

  // 입력 버퍼 비우기 (혹시 남아있을 찌꺼기 제거)
  while(Serial.available()) Serial.read(); 

  // 입력이 들어올 때까지 무한 대기
  while (Serial.available() == 0) {
    // 아무것도 안 하고 기다림
  }

  // 입력 들어오면 버퍼 비우고 다음 판으로 고고!
  while(Serial.available()) Serial.read(); 
}