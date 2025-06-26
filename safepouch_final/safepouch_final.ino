// =================================================================
// ==      KODE FINAL: DETEKSI JATUH (WIFI + FIREBASE + TOMBOL)   ==
// =================================================================

#include <MPU9250_asukiaaa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- PINOUT & HARDWARE ---
#define SDA_PIN 2
#define SCL_PIN 1
const int BUZZER_PIN = 15;
const int MULTI_FUNCTION_BUTTON_PIN = 4; // Tombol sekarang multifungsi
const int BATT_ADC_PIN = 5;              // Pin untuk monitor baterai

// --- KONFIGURASI FIREBASE & JARINGAN ---
#define API_KEY "AIzaSyBFNWuTAS0G1C2QRtF8ZCaX4wrlEXC_F3s"
#define DATABASE_URL "https://safepouch-302b1-default-rtdb.asia-southeast1.firebasedatabase.app/"
String deviceId = "ESP32-WIFI-02";

// --- OBJEK & VARIABEL GLOBAL ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
MPU9250_asukiaaa mySensor;
bool signupOK = false;

// Variabel untuk State Machine Deteksi Jatuh
enum AlarmStateType { IDLE, FREEFALL_DETECTED, IMPACT_DETECTED, ALARM_TRIGGERED };
AlarmStateType currentAlarmState = IDLE;
unsigned long stateChangeTimestamp = 0;
const float FALL_THRESHOLD_G = 3.0;
const float FREEFALL_THRESHOLD_G = 0.4;
const unsigned long MIN_POST_FALL_NO_MOTION_MS = 5000;
const float NO_MOTION_THRESHOLD_G = 0.2;
float prev_ax = 0, prev_ay = 0, prev_az = 0;
int trig = 0; // Variabel untuk mengirim status ke Firebase
unsigned long sendDataPrevMillis = 0;
// Variabel baru untuk melacak durasi kondisi diam
unsigned long stillnessStartTime = 0;

// Variabel untuk Monitor Baterai
unsigned long lastBatteryCheck = 0;
const long batteryCheckInterval = 60000; // Cek baterai setiap 60 detik

// Variabel untuk Logika Tombol Multifungsi
const unsigned long DEBOUNCE_DELAY_BTN = 50;
const unsigned long LONG_PRESS_TIME_BTN = 2000;
const unsigned long DOUBLE_PRESS_WINDOW_BTN = 400;
int buttonState;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonDownTime = 0;
bool longPressTriggered = false;
unsigned long lastPressTime = 0;
int pressCount = 0;


// --- DEKLARASI FUNGSI ---
void handleButton();
void checkBattery(bool forceSend);
void changeState(AlarmStateType newState, bool panic = false);
void runFallDetectionStateMachine();

// =================================================================
// ==                          SETUP                                ==
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(500); // Ganti while(!Serial) agar tidak macet saat pakai baterai

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  playBeep(3, 60);
  pinMode(MULTI_FUNCTION_BUTTON_PIN, INPUT_PULLUP);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  mySensor.setWire(&Wire);
  mySensor.beginAccel();

  // Koneksi WiFi via WiFiManager
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Alat-Jatuh-Setup", "password123")) {
    Serial.println("Gagal terhubung. Restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("Berhasil terhubung via WiFi!");

  // Setup Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// =================================================================
// ==                           LOOP                                ==
// =================================================================
void loop() {
  if (Firebase.ready() && signupOK) {
    handleButton(); // Proses logika tombol multifungsi
    runFallDetectionStateMachine(); // Proses logika deteksi jatuh

    // Cek baterai secara berkala
    if (millis() - lastBatteryCheck > batteryCheckInterval) {
      lastBatteryCheck = millis();
      checkBattery(true);
    }
  } else {
    Serial.println("Menunggu koneksi Firebase siap...");
    delay(2000);
  }
  delay(50);
}

// =================================================================
// ==                  FUNGSI-FUNGSI UTAMA                        ==
// =================================================================

void handleButton() {
  int reading = digitalRead(MULTI_FUNCTION_BUTTON_PIN);

  // --- Debounce Logic ---
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  lastButtonState = reading;

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_BTN) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // Tombol BARU saja ditekan
        buttonDownTime = millis();
        longPressTriggered = false; // Reset flag long press setiap kali tombol ditekan
      } else { // Tombol BARU saja dilepas
        // PERBAIKAN: Hanya catat tekanan jika BUKAN hasil dari long press
        if (!longPressTriggered) {
          pressCount++;
          lastPressTime = millis();
        }
      }
    }
  }

  // --- Logika Aksi ---
  // Cek Long Press (harus dilakukan saat tombol masih ditekan)
  if (buttonState == LOW && !longPressTriggered && (millis() - buttonDownTime > LONG_PRESS_TIME_BTN)) {
    Serial.println("AKSI: Tekan Tahan -> Memicu Alarm Panik!");
    longPressTriggered = true; // Tandai agar aksi ini tidak terulang & short press diabaikan
    
    changeState(ALARM_TRIGGERED, true); // true menandakan ini adalah alarm panik
  }

  // Cek Short & Double Press (diproses SETELAH tombol dilepas)
  if (pressCount > 0 && (millis() - lastPressTime > DOUBLE_PRESS_WINDOW_BTN)) {
    if (pressCount == 1) { // Aksi untuk tekan singkat
      if (currentAlarmState != IDLE) {
        Serial.println("AKSI: Tekan Singkat -> Alarm Dibatalkan!");
        changeState(IDLE);
      }
    } else if (pressCount == 2) { // Aksi untuk tekan dua kali
      Serial.println("AKSI: Tekan Dua Kali -> Cek Status Baterai!");
      checkBattery(true);
    }
    pressCount = 0; // Reset hitungan untuk siklus berikutnya
  }
}

void runFallDetectionStateMachine() {
  uint8_t result = mySensor.accelUpdate();
  if (result == 0) {
    float aX = mySensor.accelX(), aY = mySensor.accelY(), aZ = mySensor.accelZ();
    float mag = sqrt(aX * aX + aY * aY + aZ * aZ);

    // Struktur switch-case yang benar
    switch (currentAlarmState) {
      case IDLE:
        // Saat IDLE, pastikan buzzer mati dan cek kondisi jatuh bebas
        digitalWrite(BUZZER_PIN, HIGH); 
        if (mag < FREEFALL_THRESHOLD_G) {
          changeState(FREEFALL_DETECTED);
        }
        break;
      
      case FREEFALL_DETECTED:
        // Saat jatuh bebas, bunyikan buzzer dan cek kondisi benturan
        digitalWrite(BUZZER_PIN, (millis() / 1000) % 2); 

        if (mag > FALL_THRESHOLD_G) {
          // Benturan terdeteksi! Simpan nilai akselerasi dan pindah state
          prev_ax = aX; prev_ay = aY; prev_az = aZ;
          changeState(IMPACT_DETECTED);
        } else if (millis() - stateChangeTimestamp > 2000) {
          // Jika tidak ada benturan dalam 2 detik, anggap alarm palsu
          Serial.println("Timeout jatuh bebas (tidak ada benturan), kembali ke IDLE.");
          changeState(IDLE); // Kembali ke IDLE akan otomatis mematikan buzzer
        }
        break;

      case IMPACT_DETECTED:
        // Setelah benturan, tetap bunyikan buzzer dan cek kondisi tidak bergerak
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);

        // Cek apakah perangkat dalam kondisi diam (terlentang/tertelungkup)
        if (mag > 0.95 && mag < 1.05) {
          // Jika diam, mulai hitung waktu
          if (stillnessStartTime == 0) { // Mulai timer jika ini pertama kalinya
            stillnessStartTime = millis();
            Serial.println("Perangkat terdeteksi diam, memulai timer 10 detik...");
          }
          
          // Cek apakah sudah diam selama 10 detik
          if (millis() - stillnessStartTime > 10000) {
            Serial.println("Perangkat diam selama 10 detik. JATUH TERKONFIRMASI!");
            changeState(ALARM_TRIGGERED);
          }
        } else {
          // Jika perangkat bergerak, reset timer "diam"
          if (stillnessStartTime > 0) {
             Serial.println("Gerakan terdeteksi, timer diam direset.");
          }
          stillnessStartTime = 0;
        }
        // Timeout pengaman: Jika setelah 30 detik dari benturan tidak ada kesimpulan,
        // anggap pengguna baik-baik saja dan kembali ke IDLE.
        if (millis() - stateChangeTimestamp > 30000) {
            Serial.println("Timeout state benturan. Diasumsikan alarm palsu, kembali ke IDLE.");
            changeState(IDLE);
        }
        break;

      case ALARM_TRIGGERED:
        // PERUBAHAN LOGIKA UTAMA: Alarm final dengan tempo cepat dan durasi 20 detik
        digitalWrite(BUZZER_PIN, (millis() / 150) % 2); // Tempo lebih cepat

        // Cek apakah durasi alarm 20 detik sudah lewat
        if (millis() - stateChangeTimestamp > 20000) {
          Serial.println("Durasi alarm 20 detik selesai. Kembali ke IDLE.");
          changeState(IDLE);
        }
        // Tombol batal akan ditangani di loop() utama secara universal
        break;
    }
  }
}

void checkBattery(bool forceSend) {
  // Nilai ini harus Anda kalibrasi ulang jika perlu
  const float R1 = 98600.0, R2 = 96500.0, V_REF = 3.3, FAKTOR_KALIBRASI = 1.23;
  const int ADC_RESOLUTION = 4095;
  
  int adcValue = analogRead(BATT_ADC_PIN);
  float pinVoltage = (adcValue / (float)ADC_RESOLUTION) * V_REF;
  float rawBatteryVoltage = pinVoltage * ((R1 + R2) / R2);
  float calibratedBatteryVoltage = rawBatteryVoltage * FAKTOR_KALIBRASI;
  long percentage = map(calibratedBatteryVoltage * 100, 320, 420, 0, 100);
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  
  if (forceSend) {
    Serial.printf("Mengirim status baterai: %.2fV (%d%%)\n", calibratedBatteryVoltage, percentage);
    if (Firebase.ready()) {
      String pathVolt = deviceId + "/battery/voltage";
      String pathPercent = deviceId + "/battery/percentage";
      int trigbat = 1;
      Firebase.RTDB.setFloatAsync(&fbdo, pathVolt, calibratedBatteryVoltage);
      Firebase.RTDB.setIntAsync(&fbdo, pathPercent, percentage);
    }
  }
}

// Modifikasi juga fungsi changeState Anda untuk menambahkan bunyi "cerdas"

void changeState(AlarmStateType newState, bool panic) {
  if (currentAlarmState != newState || panic) {
    // BUNYI CERDAS: Jika kembali ke IDLE dari state lain, bunyikan "bip" konfirmasi
    if (newState == IDLE && currentAlarmState != IDLE) {
        playBeep(3, 60); // 2x bip cepat
    }
    currentAlarmState = newState;
    stateChangeTimestamp = millis();
    stillnessStartTime = 0; // Reset timer diam setiap kali ganti state

    
    // Logika pengiriman data ke Firebase saat state berubah
    int stateInt;
    switch (newState) {
      case IDLE:                stateInt = 0; break;
      case FREEFALL_DETECTED:     stateInt = 1; break;
      case IMPACT_DETECTED:       stateInt = 2; break;
      case ALARM_TRIGGERED:       stateInt = 3; break;
    }

    if (panic) {
      stateInt = 3; // Paksa kirim state 3 (ALARM) jika panik
      Serial.println("Mengirim status PANIK ke Firebase.");
    }

    Firebase.RTDB.setIntAsync(&fbdo, deviceId + "/trig", stateInt);
    
    // Simulasi deteksi jatuh lanjutan (kecuali jika panik)
    if (!panic) {
      // Kode switch-case asli Anda bisa diringkas di sini jika perlu,
      // tapi struktur saat ini sudah cukup baik.
    }
  }
}

// Direkomendasikan: Buat fungsi untuk bunyi beep agar rapi
void playBeep(int count, int duration) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        delay(duration);
    }
}


