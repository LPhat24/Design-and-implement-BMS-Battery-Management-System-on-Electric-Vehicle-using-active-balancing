#include <Wire.h>
#include <OneWire.h>
#include "MKL_DS18B20.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ====== WIFI + FIREBASE ======
#define WIFI_SSID "Phat dzai"
#define WIFI_PASSWORD "phat2406"
#define FIREBASE_HOST "energy-management-course-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "eATtphjUlmIPT2ZzyBBUgR6BMS17i4YqkVGKvWSh"

FirebaseData firebaseData;

// ===== LCD2004 =====
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ===== OUTPUT =====
#define PIN_COOLING   37
#define PIN_BUZZER    38
#define PIN_DISCHARGE 36
#define PIN_CHARGE    35

// ===== INPUT =====
#define PIN_BALANCING 46
#define PIN_REVERSE   9
#define PIN_FIREBASE  10
#define PIN_LEFT      11
#define PIN_RIGHT     12
#define PIN_LCD       1

// ===== ADC (Potentiometers) =====
#define PIN_V_DISCH_MIN  16
#define PIN_I_DISCH_MAX  15
#define PIN_V_CHARGE_MAX 3
#define PIN_I_CHARGE_MAX 8

// ===== CẢM BIẾN DÒNG =====
#define PIN_CURRENT 7
#define NUM_SAMPLES_CURR 50
const float VREF_ADC   = 3.3f;
const int   ADC_RES    = 4095;
const float ZERO_CURRENT_VOLTAGE = 2.588f;
const float SENSITIVITY = 0.04f;
const float DEADZONE_LOW  = 2.583f;
const float DEADZONE_HIGH = 2.592f;
float offsetCurrent = 0.0f;

// ===== DS18B20 =====
#define DS18B20_PIN 6
OneWire oneWire_10(DS18B20_PIN);
MKL_DS18B20 sensor(&oneWire_10);
float temperatureC = 0;

// ===== CELL VOLTAGE (2xSTM32) =====
float Vnode[15] = {0};
float Vcell[15] = {0};
int   cell_mV[15] = {3700};
int   Vmin_mV = 0, Vmax_mV = 0, deltaV_mV = 0;
long  Vtot_mV = 0;

// ===== UART BUFFER =====
String uartBuf1 = "";
String uartBuf2 = "";
bool frame1_ok = false, frame2_ok = false;
unsigned long lastFrame1 = 0, lastFrame2 = 0;

// ===== GLOBAL =====
int currentPage = 1;
bool lastLeft = HIGH, lastRight = HIGH;
unsigned long prevMillis = 0;
const unsigned long refreshInterval = 200;

// ===== Smoothing ADC =====
const int numADCReadings = 3;
int readingsADC[4][numADCReadings];
int indexADC = 0;
long totalADC[4] = {0,0,0,0};
int  averageADC[4] = {0,0,0,0};

// ===== FREE RTOS STRUCT =====
typedef struct {
  int   cell_mV[15];
  int   Vmin_mV, Vmax_mV, deltaV_mV;
  long  Vtot_mV;
  float currentA, temperatureC;
  int   averageADC[4];
  bool  state_bal, state_rev, state_fb;
} FbPacket;

QueueHandle_t fbQueue;

// ================= UART RECEIVE =================
bool receiveUART(HardwareSerial &serialPort, String &buffer, int startIndex, int count) {
  bool frameReady = false;
  while (serialPort.available()) {
    char c = serialPort.read();
    if (c == '\n') {
      String frame = buffer;
      buffer = "";
      frame.trim();
      if (frame.startsWith("S1,") || frame.startsWith("S2,")) frame = frame.substring(3);
      int idx = 0;
      bool validFrame = true;
      while (idx < count && frame.length() > 0) {
        int comma = frame.indexOf(',');
        String token = (comma == -1) ? frame : frame.substring(0, comma);
        float val = token.toFloat();
        if (val < 0.0f || val > 63.0f) { validFrame = false; break; }
        Vnode[startIndex + idx] = val;
        if (comma == -1) break;
        frame = frame.substring(comma + 1);
        idx++;
      }
      if (validFrame) frameReady = true;
    } else buffer += c;
  }
  return frameReady;
}

// ================= CELL COMPUTE =================
void computeCellsAndStats() {
  static float cellFiltered[15] = {3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7,3.7};
  const float alpha = 0.8f;
  Vcell[0] = Vnode[0];
  for (int i = 1; i < 15; i++) Vcell[i] = Vnode[i] - Vnode[i - 1];

  Vtot_mV = 0; Vmin_mV = 99999; Vmax_mV = 0;
  for (int i = 0; i < 15; i++) {
    float cellV = Vcell[i];
    if (cellV < 2.5f || cellV > 4.8f) cellV = cellFiltered[i];
    cellFiltered[i] = alpha * cellFiltered[i] + (1.0f - alpha) * cellV;
    cell_mV[i] = (int)(cellFiltered[i] * 1000.0f);
    Vtot_mV += cell_mV[i];
    if (cell_mV[i] < Vmin_mV) Vmin_mV = cell_mV[i];
    if (cell_mV[i] > Vmax_mV) Vmax_mV = cell_mV[i];
  }
  deltaV_mV = Vmax_mV - Vmin_mV;
}

// ================= DÒNG ĐIỆN =================
float readCurrentOnce() {
  static unsigned long lastMicros = 0;
  static long adcSum = 0;
  static int sampleCount = 0;
  static float lastCurrent = 0.0;
  if (micros() - lastMicros >= 200) {
    lastMicros = micros();
    adcSum += analogRead(PIN_CURRENT);
    sampleCount++;
  }
  if (sampleCount >= NUM_SAMPLES_CURR) {
    float adcAvg = adcSum / (float)NUM_SAMPLES_CURR;
    adcSum = 0; sampleCount = 0;
    float Vout = (adcAvg / ADC_RES) * VREF_ADC;
    if (Vout >= DEADZONE_LOW && Vout <= DEADZONE_HIGH) lastCurrent = 0.0;
    else lastCurrent = ((Vout - ZERO_CURRENT_VOLTAGE) / SENSITIVITY - offsetCurrent)/10.0f;
  }
  return lastCurrent;
}

// ================= NHIỆT ĐỘ =================
void updateTemperature() {
  sensor.requestTemperatures();
  temperatureC = sensor.getTempC();
}

// ================= POTENTIOMETER =================
void updatePotentiometers() {
  int adcPins[4] = {PIN_V_DISCH_MIN, PIN_I_DISCH_MAX, PIN_V_CHARGE_MAX, PIN_I_CHARGE_MAX};
  for (int k=0; k<4; k++) {
    int rawADC = analogRead(adcPins[k]);
    totalADC[k] -= readingsADC[k][indexADC];
    readingsADC[k][indexADC] = rawADC;
    totalADC[k] += readingsADC[k][indexADC];
    averageADC[k] = totalADC[k] / numADCReadings;
  }
  indexADC = (indexADC + 1) % numADCReadings;
}

// ================= LCD HIỂN THỊ =================
void showSettingPage() {
  long Vdisch_min = map(averageADC[0], 0, 4095, 0, 4200);
  long Idisch_max = map(averageADC[1], 0, 4095, 0, 40000);
  long Vcharge_max = map(averageADC[2], 0, 4095, 0, 4200);
  long Icharge_max = map(averageADC[3], 0, 4095, 0, 11000);
  lcd.setCursor(0,0); lcd.print("Vchg_max: "); lcd.print(Vcharge_max); lcd.print("mV   ");
  lcd.setCursor(0,1); lcd.print("Ichg_max: "); lcd.print(Icharge_max); lcd.print("mA   ");
  lcd.setCursor(0,2); lcd.print("Vdis_min: "); lcd.print(Vdisch_min); lcd.print("mV   ");
  lcd.setCursor(0,3); lcd.print("Idis_max: "); lcd.print(Idisch_max); lcd.print("mA   ");
}

void showPage1() {
  float currentValue = readCurrentOnce();
  lcd.setCursor(0,0); lcd.print("Vtot: "); lcd.print(Vtot_mV); lcd.print("mV   ");
  lcd.setCursor(0,1); lcd.print("I: "); lcd.print(currentValue, 2); lcd.print("A ");
  lcd.print("T: "); lcd.print(temperatureC, 1); lcd.print("C   ");
  lcd.setCursor(0,2); lcd.print("Min:"); lcd.print(Vmin_mV); lcd.print("mV    ");
  lcd.setCursor(0,3); lcd.printf("Max:%dmV dv:%dmV ", Vmax_mV, deltaV_mV);
}

void showPage2() {
  lcd.setCursor(0,0); lcd.printf("C1:%d  C2:%d   ", cell_mV[0], cell_mV[1]);
  lcd.setCursor(0,1); lcd.printf("C3:%d  C4:%d   ", cell_mV[2], cell_mV[3]);
  lcd.setCursor(0,2); lcd.printf("C5:%d  C6:%d   ", cell_mV[4], cell_mV[5]);
  lcd.setCursor(0,3); lcd.printf("C7:%d  C8:%d   ", cell_mV[6], cell_mV[7]);
}

void showPage3() {
  lcd.setCursor(0,0); lcd.printf("C9:%d   C10:%d  ", cell_mV[8], cell_mV[9]);
  lcd.setCursor(0,1); lcd.printf("C11:%d  C12:%d ", cell_mV[10], cell_mV[11]);
  lcd.setCursor(0,2); lcd.printf("C13:%d  C14:%d ", cell_mV[12], cell_mV[13]);
  lcd.setCursor(0,3); lcd.printf("C15:%d          ", cell_mV[14]);
}

void showPage4() {
  // ===== Hiển thị trạng thái công tắc =====
  lcd.setCursor(0,0); lcd.print("Balancing: "); lcd.print(digitalRead(PIN_BALANCING)==LOW ? "ON " : "OFF");
  lcd.setCursor(0,1); lcd.print("Reverse:   "); lcd.print(digitalRead(PIN_REVERSE)==LOW ? "ON " : "OFF");
  lcd.setCursor(0,2); lcd.print("Firebase:  "); lcd.print(digitalRead(PIN_FIREBASE)==LOW ? "ON " : "OFF");
  // ===== Dòng 4: Hiển thị trạng thái kết nối =====
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  bool fbOK   = Firebase.ready();
  lcd.setCursor(0,3); 
  lcd.print("WiFi:"); lcd.print(wifiOK ? "OK " : "OFF");
  lcd.print("  FB:"); lcd.print(fbOK ? "OK " : "OFF");
}


void showInfoPage() {
  if (currentPage==1) showPage1();
  else if (currentPage==2) showPage2();
  else if (currentPage==3) showPage3();
  else showPage4();
}

void handlePageSwitch() {
  bool leftState = digitalRead(PIN_LEFT);
  bool rightState = digitalRead(PIN_RIGHT);
  if (lastLeft==HIGH && leftState==LOW) { currentPage--; if(currentPage<1) currentPage=4; lcd.clear(); }
  if (lastRight==HIGH && rightState==LOW){ currentPage++; if(currentPage>4) currentPage=1; lcd.clear(); }
  lastLeft = leftState; lastRight = rightState;
}

// ================= WIFI =================
void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis()-start < 3000) vTaskDelay(100 / portTICK_PERIOD_MS);
    if (WiFi.status() == WL_CONNECTED) {
      Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
      Firebase.reconnectWiFi(true);
    }
  }
}

// ================= FIREBASE TASK =================
void firebaseTask(void *param) {
  WiFi.setSleep(false);
  FbPacket pkt;
  for (;;) {
    if (xQueueReceive(fbQueue, &pkt, portMAX_DELAY) == pdTRUE) {
      ensureWiFiConnected();
      if (WiFi.status() != WL_CONNECTED) continue;

      FirebaseJson root, jBatt, jMain, jSet, jSwitch;
      float totalV = 0;
      for (int i=0; i<15; i++) {
        char key[8]; snprintf(key, sizeof(key), "Cell%02d", i+1);
        float v = pkt.cell_mV[i]/1000.0f;
        jBatt.set(String(key), v);
        totalV += v;
      }

      jMain.set("Total_Voltage_V", totalV);
      jMain.set("Battery_Current_A", pkt.currentA);
      jMain.set("Temperature_C", pkt.temperatureC);
      jMain.set("Cell_Vmin_V", pkt.Vmin_mV/1000.0f);
      jMain.set("Cell_Vmax_V", pkt.Vmax_mV/1000.0f);
      jMain.set("Cell_Delta_mV", pkt.deltaV_mV);   // ✅ đổi đơn vị & tên

      jSet.set("V_Discharge_Min_V", pkt.averageADC[0]*4.2f/4095.0f);
      jSet.set("I_Discharge_Max_A", pkt.averageADC[1]*40.0f/4095.0f);
      jSet.set("V_Charge_Max_V",    pkt.averageADC[2]*4.2f/4095.0f);
      jSet.set("I_Charge_Max_A",    pkt.averageADC[3]*11.0f/4095.0f);

      jSwitch.set("Balancing", pkt.state_bal?"ON":"OFF");
      jSwitch.set("Reverse",   pkt.state_rev?"ON":"OFF");
      jSwitch.set("Firebase",  pkt.state_fb ?"ON":"OFF");

      root.set("/Battery", jBatt);
      root.set("/Main",    jMain);
      root.set("/Setting", jSet);
      root.set("/Function_Switch", jSwitch);   // ✅ đổi tên nhóm

      bool ok = Firebase.setJSON(firebaseData, "/", root);
      Serial.println(ok ? "✅ Firebase Upload OK" : "❌ Upload FAIL");
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 18, 17);
  Serial2.begin(115200, SERIAL_8N1, 20, 19);
  Wire.begin(5,4);
  WiFi.setSleep(false);

  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.setCursor(4,0); lcd.print("BMS ACTIVE");
  lcd.setCursor(3,1); lcd.print("LE THANH PHAT");
  lcd.setCursor(5,2); lcd.print("23145165");
  lcd.setCursor(1,3); lcd.print("NCKH-SV2025-271");
  delay(2500); lcd.clear();

  pinMode(PIN_COOLING, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_DISCHARGE, OUTPUT);
  pinMode(PIN_CHARGE, OUTPUT);
  digitalWrite(PIN_COOLING, HIGH);

  pinMode(PIN_BALANCING, INPUT_PULLUP);
  pinMode(PIN_REVERSE,   INPUT_PULLUP);
  pinMode(PIN_FIREBASE,  INPUT_PULLUP);
  pinMode(PIN_LEFT,      INPUT_PULLUP);
  pinMode(PIN_RIGHT,     INPUT_PULLUP);
  pinMode(PIN_LCD,       INPUT_PULLUP);

  sensor.begin();
  fbQueue = xQueueCreate(1, sizeof(FbPacket));
  xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 8192, NULL, 1, NULL, 0);
}

// ================= LOOP =================
void loop() {
  unsigned long now = millis();
  if (now - prevMillis < refreshInterval) return;
  prevMillis = now;

  if (receiveUART(Serial1, uartBuf1, 0, 8)) { frame1_ok = true; lastFrame1 = millis(); }
  if (receiveUART(Serial2, uartBuf2, 8, 7)) { frame2_ok = true; lastFrame2 = millis(); }

  if (frame1_ok && frame2_ok && abs((long)lastFrame1 - (long)lastFrame2) < 300) {
    computeCellsAndStats();
    frame1_ok = frame2_ok = false;
  }

  float currentValue = readCurrentOnce();
  updateTemperature();
  updatePotentiometers();

  bool lcdMode = (digitalRead(PIN_LCD) == LOW);
  static bool lastMode = false;
  if (lcdMode != lastMode) { lcd.clear(); lastMode = lcdMode; }
  if (lcdMode) showSettingPage();
  else { handlePageSwitch(); showInfoPage(); }

  // ----- Firebase Task Trigger -----
  if (digitalRead(PIN_FIREBASE) == LOW) {
    static unsigned long prevFirebase = 0;
    if (millis() - prevFirebase >= 1000) {
      prevFirebase = millis();
      FbPacket pkt;
      memcpy(pkt.cell_mV, cell_mV, sizeof(cell_mV));
      pkt.Vmin_mV = Vmin_mV; pkt.Vmax_mV = Vmax_mV;
      pkt.deltaV_mV = deltaV_mV; pkt.Vtot_mV = Vtot_mV;
      pkt.currentA = currentValue; pkt.temperatureC = temperatureC;
      for (int i=0;i<4;i++) pkt.averageADC[i] = averageADC[i];
      pkt.state_bal  = (digitalRead(PIN_BALANCING)==LOW);
      pkt.state_rev  = (digitalRead(PIN_REVERSE)==LOW);
      pkt.state_fb   = (digitalRead(PIN_FIREBASE)==LOW);
      xQueueOverwrite(fbQueue, &pkt);
    }
  }
}
