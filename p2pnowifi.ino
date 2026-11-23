#include <ESP32Servo.h>

// ================== UART với Arduino ==================
#define UART_RX 16   // RX2, nối với A5 (TX) của Arduino, QUA CẦU CHIA ÁP
#define UART_TX 17   // TX2, nối với A4 (RX) của Arduino

// ================== Servo ==================
Servo servo1;   // GPIO 19
Servo servo2;   // GPIO 21

#define SERVO1_PIN 19
#define SERVO2_PIN 21

#define S1_START 70
#define S1_WORK  145
#define S2_START 120
#define S2_WORK  30

#define STEP_DELAY 30   // ms mỗi độ

Servo* servos[2] = { &servo1, &servo2 };
int pos[2] = { S1_START, S2_START };

// ================== Các chu trình ==================
struct Step {
  uint8_t servo;
  int target;
  unsigned long pauseAfter;
};

// RUN: về vị trí đầu + Bước 1 -> 3
Step seqRun[] = {
  {0, S1_START, 300},   // về đầu
  {1, S2_START, 500},   // về đầu
  {0, S1_WORK,  500},   // Bước 1: Servo1 70 -> 145
  {1, S2_WORK,  500},   // Bước 2: Servo2 120 -> 30
  {0, S1_START, 0}      // Bước 3: Servo1 145 -> 70
};

// DROP: Bước 4
Step seqDrop[] = {
  {1, S2_START, 0}      // Bước 4: Servo2 30 -> 120
};

Step* curSeq = nullptr;
int curCount = 0;
int seqStep = 0;
bool running = false;
unsigned long waitUntil = 0;

void startSequence(Step* seq, int count, const char* name) {
  curSeq = seq;
  curCount = count;
  seqStep = 0;
  waitUntil = 0;
  running = true;
  Serial.print("Bat dau: ");
  Serial.println(name);
}

void runSequence() {
  unsigned long now = millis();
  if (now < waitUntil) return;

  Step &s = curSeq[seqStep];

  // Đã tới đích -> sang bước kế
  if (pos[s.servo] == s.target) {
    seqStep++;
    if (seqStep >= curCount) {
      running = false;
      Serial2.println("DONE");
      Serial.println("Xong -> gui DONE");
      return;
    }
    waitUntil = now + s.pauseAfter;
    return;
  }

  // Di chuyển 1 độ
  if (pos[s.servo] < s.target) pos[s.servo]++;
  else                         pos[s.servo]--;

  servos[s.servo]->write(pos[s.servo]);
  waitUntil = now + STEP_DELAY;
}

// ================== Nhận lệnh từ Arduino ==================
char buf[24];
byte bufLen = 0;

void handleCommand(const char* cmd) {
  Serial.print("Nhan tu Arduino: ");
  Serial.println(cmd);

  if (strcmp(cmd, "PING") == 0) {
    Serial2.println("PONG");
  }
  else if (strcmp(cmd, "RUN") == 0) {
    if (running) {
      Serial2.println("BUSY");
    } else {
      Serial2.println("OK");   // trả OK ngay, trước khi servo chạy
      startSequence(seqRun, sizeof(seqRun) / sizeof(seqRun[0]), "RUN (buoc 1-3)");
    }
  }
  else if (strcmp(cmd, "DROP") == 0) {
    if (running) {
      Serial2.println("BUSY");
    } else {
      Serial2.println("OK");
      startSequence(seqDrop, sizeof(seqDrop) / sizeof(seqDrop[0]), "DROP (buoc 4)");
    }
  }
}

// Đọc từng ký tự, ghép thành dòng, không chặn chương trình
void readUART() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[bufLen] = '\0';
      if (bufLen > 0) handleCommand(buf);
      bufLen = 0;
    }
    else if (bufLen < sizeof(buf) - 1) {
      buf[bufLen++] = c;
    }
  }
}

// ================== setup / loop ==================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);

  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  servo1.write(S1_START);
  servo2.write(S2_START);
  delay(1500);

  Serial.println("San sang - cho RUN / DROP tu Arduino");
}

void loop() {
  readUART();
  if (running) runSequence();
}