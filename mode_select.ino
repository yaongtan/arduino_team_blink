#include <LiquidCrystal.h>
#include <SPI.h>  // SD카드 통신용
#include <SD.h>   // SD카드 라이브러리

LiquidCrystal lcd(44, 45, 46, 47, 48, 49);  // LCD 연결

const int chipSelect = 53; // SD카드 CS핀

int led_pins[] = { 8, 9, 10, 11 };
int button_pins[] = { 14, 15, 16, 17 };
int ifbuttons[] = { 0, 0, 0, 0 };

// mode 상수 집합 정의
enum {
  MODE_MENU = 0,
  MODE_LED = 1,
  MODE_RHYTHM = 2
};
int mode = MODE_MENU;

//LED 순서와 버튼 클릭 순서 기록을 위한 변수
int sequence[256];
int my_button_click[256];
int* p = my_button_click;
int sequence_num = 4;

//delay를 사용할 시, 센서 감지, 연산 등에서 오류 발생 가능성이 많기에
//TIMER 구조체를 따로 생성하여 사용함
struct Timer {
  unsigned long previous = 0;
  unsigned long interval;

  Timer(unsigned long p = 0, unsigned long i = 1000) {
    previous = p;
    interval = i;
  }
};

//interval에 해당하는 초 마다 true를 반환하는 함수(struct(구조체)를 이용하여 각각의 previous를 독립적으로 연산함)
bool per_time_interval(Timer& t) {
  int time_current = millis();
  if (time_current - t.previous >= t.interval) {
    t.previous = time_current;
    return true;
  }
  return false;
}

//LED 순서 랜덤 생성(num에 해당하는 만큼 순서를 생성함)
void make_random_sequence(int num) {
  for (int i = 0; i < num; i++) {
    sequence[i] = random(1, 5);
  }
}

//버튼 클릭 감지(i에 해당하는 인덱스의 버튼을 감지함)
bool button_ifclicked(int i) {
  if (digitalRead(button_pins[i])) {
    if (!ifbuttons[i]++)
      return true;
    delay(50);
  } else ifbuttons[i] = 0;
  return false;
}

//sequence의 값을 하나씩 반환함
int* psequence = sequence;
int led_pins_from_sequence() {
  if (*psequence)
    return led_pins[*psequence++ - 1];
  return 0;
}

//led 온오프 관리 함수
void led_onoff(int pin) {
  for (int i = 8; i <= 11; i++)
    digitalWrite(i, i == pin);
}

// SD카드 데이터 시리얼 모니터에 출력
void printSDLogs() {
  File dataFile = SD.open("game_log.txt");

  if (dataFile) {
    Serial.println("----------- SD Logs -----------");
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
    Serial.println("\n-----------------------------");
  } else {
    Serial.println("no logs.");
  }
}

//셋업(LED 출력 시퀀스를 임시로 출력하게 해놓았기에 확인 가능(추후 삭제 예정))
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // NodeMCU와 통신 (19-RX, 18-TX)
  lcd.begin(16, 2);
  randomSeed(analogRead(0));
  /*make_random_sequence(sequence_num);
  
  Serial.println();
  for (int i = 0; i < sequence_num; i++)
    Serial.print(sequence[i]);
  Serial.println();
*/
  for (int i = 0; i < 4; i++) {
    pinMode(led_pins[i], OUTPUT);
    pinMode(button_pins[i], INPUT);
  }
  // SD카드 통신 시작
  Serial.print("SD Connecting...");
  pinMode(53, OUTPUT); 
  // 연결 결과 출력
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Connection Failed.");
  } else {
    Serial.println("SD Connected.");
  }

  printSDLogs(); // SD카드 데이터 출력
  
  showMenu();
  delay(1000);  //시작 전 딜레이 추후 삭제 예정
}

// TIMER 생성(LED 출력 주기와 켜진 이후 꺼질 때까지의 딜레이 2개)
// 추후 가장 적합한 딜레이로 수정 예정
Timer led_timer1(0, 700);
Timer led_timer2(0, 400);
Timer button_timer1(0, 10000);

// [게임 결과 저장 및 전송]

// SD카드에 저장
void saveToSD(String type, float score, float playTime) {
  // "game_log.txt"에 저장 (없으면 알아서 만듦)
  File dataFile = SD.open("game_log.txt", FILE_WRITE);

  if (dataFile) {
    // 형식: LED,scroe,playTime
    dataFile.print(type);
    dataFile.print(",");
    dataFile.print(score, 1);
    dataFile.print(",");
    dataFile.println(playTime, 1);
    
    dataFile.close(); 
    Serial.println("SD카드 저장 완료");
  } else {
    Serial.println("game_log.txt 파일을 열 수 없음");
  }
}

// 사용법: sendResult("LED", 95.5, 20.3);
void sendResult(String type, float score, float playTime) {
  String data = type + "," + String(score, 1) + "," + String(playTime, 1); // 데이터 합치기 (예: "LED,95.5,20.3")
  Serial1.println(data); // NodeMCU로 전송 (Serial1 사용)
  
  Serial.print("NodeMCU 전송 완료 : ");
  Serial.println(data);
}

// [게임 로직]

// LED GAME
// ex) 3121 = 11 9 10 9
void LED_game() {
  int onoff = 0;
  int finish = 0;
  int LED_score = 0, my_count = 0, LED_finish = 0;

  // 초기화
  memset(my_button_click, 0, sizeof(my_button_click)); // 내 입력 기록 지우기
  memset(sequence, 0, sizeof(sequence));               // 이전 문제 지우기
  p = my_button_click;
  psequence = sequence;
  led_timer1.previous = millis();
  make_random_sequence(sequence_num);

  //게임 시작
  Serial.println("GAME START");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("GAME START");

  while (LED_finish == 0) {
    if (finish) { // 버튼 입력 시작
      for (int i = 0; i < 4; i++) {
        if (button_ifclicked(i)) {
          *p++ = i + 1;
          my_count++;
        }
      }
      // 게임 끝
      // 입력 시간 초과 또는 버튼 다 눌렀을 때
      if (per_time_interval(button_timer1) || my_count == sequence_num) {
        
        // 게임 소요 시간 계산 (초)
        unsigned long endTime = millis();
        float playTime = (float)(endTime - button_timer1.previous) / 1000.0;

        Serial.println("-----------------------------");
        for (int i = 0; i < sequence_num; i++) {
          Serial.print(sequence[i]);
        }
        Serial.println();
        Serial.println("finish");
        lcd.setCursor(0,0);
        lcd.print("GAME OVER  ");

        for (int i = 0; i < sequence_num; i++) {
          Serial.print(my_button_click[i]);
          if (sequence[i] == my_button_click[i]) LED_score++; // 맞춘 개수 저장
        }

        float score = (float)LED_score / sequence_num * 100;  // 점수 계산
        Serial.print("YOUR SCORE: ");
        Serial.println(score);
        Serial.println();
        Serial.println("-----------------------------");
        
        lcd.setCursor(0,1);
        lcd.print("YOUR SCORE: ");
        lcd.setCursor(11,1);
        lcd.print(score) ;

        // LED 게임 결과 저장 및 전송
        sendResult("LED", score, playTime);
        saveToSD("LED", score, playTime);

        // 15번핀 버튼 눌릴 때 까지 기다림
        while(!button_ifclicked(1));

        LED_finish = 1;   // 게임 끝내기
        mode = MODE_MENU; // 메뉴로 돌아가기
        showMenu();       // 메뉴 화면 띄우기
      }
    }
    if (per_time_interval(led_timer1) && LED_finish == 0) {  // 0.7초마다 실행
      int pin = led_pins_from_sequence();                    // 다음에 켤 LED 핀 번호
      if (pin == 0) { // 종료 조건 (sequence가 끝났을 때 입력 시작하도록 변경)
        finish = 1;
        button_timer1.previous = millis();  // 버튼 입력 타이머 시작시간
        // 입력 시작 알림
        lcd.setCursor(0,0);
        lcd.print("Start Input");
      } else {
        led_onoff(pin);                  // 해당 핀 켜기
        onoff = 1;                       // 상태 기록
        led_timer2.previous = millis();  // LED 끄는 타이머 시작시간
      }
    }

    if (onoff)
      if (per_time_interval(led_timer2)) {
        led_onoff(0);
        onoff = 0;
      }
  }
}

// RHYTHM GAME
void rhythm_game() {
  int rhythm_finish = 0;

  while (rhythm_finish == 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RHYTHM GAME");
    delay(5000);

    lcd.setCursor(0,0);
    lcd.print("Press button 1");

    // 임의로 데이터 저장 및 전송
    sendResult("RHYTHM", 80.0, 60.0); 
    saveToSD("RHYTHM", 80.0, 60.0);

    // 15번핀 버튼 눌릴 때 까지 기다림
    while(!button_ifclicked(1));

    rhythm_finish = 1;  // 게임 끝내기
    mode = MODE_MENU;   // 메뉴로 돌아가기
    showMenu();         // 메뉴 화면 띄우기
  }
}


// [모드 선택]

// 메뉴 변수 설정
int menuIndex = 0;
const int menuCount = 2;

// 메뉴창 lcd 화면 표시
void showMenu() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Select Game");
  lcd.setCursor(0, 1);

  if (menuIndex == 0) lcd.print("> LED GAME   ");
  else lcd.print("> RHYTHM GAME");
}

// 모드 선택 로직
void menuLoop() {
  if (button_ifclicked(0)) {
    menuIndex = (menuIndex + 1) % menuCount;
    showMenu();
  }

  if (button_ifclicked(1)) {
    if (menuIndex == 0) mode = MODE_LED;
    else if (menuIndex == 1) mode = MODE_RHYTHM;
  }
}


// 메인 루프
void loop() {
  switch (mode) {
    case MODE_MENU:
      menuLoop();
      break;
    case MODE_LED:
      LED_game();
      break;
    case MODE_RHYTHM:
      rhythm_game();
      break;
  }
}
