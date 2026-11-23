#include <AFMotor.h>
#include <SoftwareSerial.h>

// ===== CAM BIEN SIEU AM =====
const uint8_t TRIG_TRUOC = A0;
const uint8_t ECHO_TRUOC = A1;
const uint8_t TRIG_SAU   = A2;
const uint8_t ECHO_SAU   = A3;

// ===== UART VOI ESP32 =====
SoftwareSerial esp(A4, A5);   // RX = A4 (tu GPIO17), TX = A5 (toi GPIO16, qua cau chia ap)

// ===== CAU HINH =====
const uint8_t TOC_DO = 180;                      // PWM: 0..255
const float KHOANG_CACH_DUNG_CM = 5.0;           // Nguong dung
const unsigned long CHU_KY_DOC_MS = 70UL;
const unsigned long TIMEOUT_ECHO_US = 25000UL;
const unsigned long TIMEOUT_OK_MS = 1000UL;      // cho ESP32 tra "OK"
const unsigned long TIMEOUT_DONE_MS = 30000UL;   // cho ESP32 bao "DONE"

// Bo tri: M1 trai truoc, M2 phai truoc, M3 trai sau, M4 phai sau.
AF_DCMotor motor1(1);
AF_DCMotor motor2(2);
AF_DCMotor motor3(3);
AF_DCMotor motor4(4);

// Neu banh nao quay sai chieu, doi FORWARD <-> BACKWARD o gia tri tuong ung.
const uint8_t CHIEU_TIEN_M1 = FORWARD;
const uint8_t CHIEU_TIEN_M2 = FORWARD;
const uint8_t CHIEU_TIEN_M3 = FORWARD;
const uint8_t CHIEU_TIEN_M4 = FORWARD;

// TIEN    : xe tien, doc cam bien truoc
// CHO_GAP : xe dung, cho ESP32 thuc hien buoc 1-3
// LUI     : xe lui, doc cam bien sau
// CHO_THA : xe dung, cho ESP32 thuc hien buoc 4
// XONG    : ket thuc
// LOI     : ESP32 khong phan hoi, xe dung han
enum TrangThai { TIEN, CHO_GAP, LUI, CHO_THA, XONG, LOI };
TrangThai trangThai = TIEN;

unsigned long mocDoc = 0;
unsigned long mocGui = 0;
bool espOK = false;
bool espDone = false;

char buf[24];
byte doDai = 0;

uint8_t daoChieu(uint8_t chieu) {
  return (chieu == FORWARD) ? BACKWARD : FORWARD;
}

void dungXe() {
  // L293D Motor Shield V1 dung RELEASE: dong co quay troi, khong phanh dong.
  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
}

void diTien() {
  motor1.setSpeed(TOC_DO);
  motor2.setSpeed(TOC_DO);
  motor3.setSpeed(TOC_DO);
  motor4.setSpeed(TOC_DO);
  motor1.run(CHIEU_TIEN_M1);
  motor2.run(CHIEU_TIEN_M2);
  motor3.run(CHIEU_TIEN_M3);
  motor4.run(CHIEU_TIEN_M4);
  Serial.println(F("XE DI TIEN"));
}

void diLui() {
  motor1.setSpeed(TOC_DO);
  motor2.setSpeed(TOC_DO);
  motor3.setSpeed(TOC_DO);
  motor4.setSpeed(TOC_DO);
  motor1.run(daoChieu(CHIEU_TIEN_M1));
  motor2.run(daoChieu(CHIEU_TIEN_M2));
  motor3.run(daoChieu(CHIEU_TIEN_M3));
  motor4.run(daoChieu(CHIEU_TIEN_M4));
  Serial.println(F("XE DI LUI"));
}

// Tra ve cm. Neu timeout (khong co echo), tra ve -1.
float docKhoangCach(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long thoiGian = pulseIn(echoPin, HIGH, TIMEOUT_ECHO_US);
  if (thoiGian == 0) return -1.0;
  return thoiGian * 0.0343f / 2.0f;
}

void inKhoangCach(const __FlashStringHelper* ten, float d) {
  Serial.print(F("Khoang cach "));
  Serial.print(ten);
  Serial.print(F(": "));
  if (d < 0) Serial.println(F("Khong co echo"));
  else {
    Serial.print(d, 1);
    Serial.println(F(" cm"));
  }
}

// ===== UART =====
void guiLenh(const char* lenh) {
  espOK = false;
  espDone = false;
  esp.println(lenh);
  mocGui = millis();
  Serial.print(F("GUI ESP32: "));
  Serial.println(lenh);
}

void xuLyDong(const char* dong) {
  Serial.print(F("ESP32: "));
  Serial.println(dong);
  if (strcmp(dong, "OK") == 0)        espOK = true;
  else if (strcmp(dong, "DONE") == 0) espDone = true;
}

// Doc tung ky tu, ghep thanh dong, khong chan chuong trinh
void docESP() {
  while (esp.available()) {
    char c = esp.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[doDai] = '\0';
      if (doDai > 0) xuLyDong(buf);
      doDai = 0;
    }
    else if (doDai < sizeof(buf) - 1) {
      buf[doDai++] = c;
    }
  }
}

// Tra ve true khi ESP32 da bao DONE. Neu ESP32 im lang qua lau -> sang trang thai LOI.
bool choESP() {
  if (espDone) {
    espDone = false;
    return true;
  }
  unsigned long now = millis();
  if (!espOK && now - mocGui >= TIMEOUT_OK_MS) {
    trangThai = LOI;
    Serial.println(F("LOI: ESP32 khong tra loi OK (kiem tra day UART, GND, cau chia ap)"));
  }
  else if (now - mocGui >= TIMEOUT_DONE_MS) {
    trangThai = LOI;
    Serial.println(F("LOI: ESP32 khong bao DONE"));
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  esp.begin(9600);

  pinMode(TRIG_TRUOC, OUTPUT);
  pinMode(ECHO_TRUOC, INPUT);
  pinMode(TRIG_SAU, OUTPUT);
  pinMode(ECHO_SAU, INPUT);
  digitalWrite(TRIG_TRUOC, LOW);
  digitalWrite(TRIG_SAU, LOW);

  dungXe();
  delay(1000); // Cho nguon va cam bien on dinh truoc khi khoi dong
  trangThai = TIEN;
  diTien();
  mocDoc = millis() - CHU_KY_DOC_MS;
}

void loop() {
  docESP();
  unsigned long hienTai = millis();

  // --- Cho ESP32 gap vat xong (buoc 1-3) -> lui ---
  if (trangThai == CHO_GAP) {
    if (choESP()) {
      Serial.println(F("ESP32 GAP XONG -> XE DI LUI"));
      trangThai = LUI;
      diLui();
      mocDoc = millis() - CHU_KY_DOC_MS;
    }
    return;
  }

  // --- Cho ESP32 tha vat xong (buoc 4) -> ket thuc ---
  if (trangThai == CHO_THA) {
    if (choESP()) {
      trangThai = XONG;
      Serial.println(F("ESP32 THA XONG -> HOAN THANH"));
    }
    return;
  }

  if (trangThai == XONG || trangThai == LOI) return;

  // --- Doc cam bien theo chu ky ---
  if (hienTai - mocDoc < CHU_KY_DOC_MS) return;
  mocDoc = hienTai;

  if (trangThai == TIEN) {
    float d = docKhoangCach(TRIG_TRUOC, ECHO_TRUOC);
    inKhoangCach(F("TRUOC"), d);

    if (d > 0 && d <= KHOANG_CACH_DUNG_CM) {
      dungXe();
      trangThai = CHO_GAP;
      Serial.println(F("GAP VAT PHIA TRUOC -> DUNG, BAO ESP32 GAP (BUOC 1-3)"));
      guiLenh("RUN");
    }
  }
  else if (trangThai == LUI) {
    float d = docKhoangCach(TRIG_SAU, ECHO_SAU);
    inKhoangCach(F("SAU"), d);

    if (d > 0 && d <= KHOANG_CACH_DUNG_CM) {
      dungXe();
      trangThai = CHO_THA;
      Serial.println(F("CHAM TUONG -> DUNG, BAO ESP32 THA (BUOC 4)"));
      guiLenh("DROP");
    }
  }
}