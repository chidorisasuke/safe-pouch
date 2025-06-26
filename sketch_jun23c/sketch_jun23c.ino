// =================================================================
// ==      KODE DETEKSI JATUH (KONEKSI WIFI + FIREBASE)           ==
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

// --- KONFIGURASI FIREBASE & JARINGAN ---
#define API_KEY "AIzaSyBFNWuTAS0G1C2QRtF8ZCaX4wrlEXC_F3s" // Ganti dengan API Key Anda
#define DATABASE_URL "https://safepouch-302b1-default-rtdb.asia-southeast1.firebasedatabase.app/" // Ganti dengan URL DB Anda
String deviceId = "ESP32-WIFI-02"; // ID unik untuk perangkat ini

// --- OBJEK & VARIABEL GLOBAL ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
MPU9250_asukiaaa mySensor;

bool signupOK = false;
int trig = 0;
unsigned long sendDataPrevMillis = 0;

const int BUZZER_PIN = 18;
const int CANCEL_BUTTON_PIN = 4;

enum AlarmStateType { IDLE, FREEFALL_DETECTED, IMPACT_DETECTED, ALARM_TRIGGERED, NOTIFICATION_SENT };
AlarmStateType currentAlarmState = IDLE;
unsigned long stateChangeTimestamp = 0;

const float FALL_THRESHOLD_G = 2.8;
const float FREEFALL_THRESHOLD_G = 0.5;
const unsigned long MIN_POST_FALL_NO_MOTION_MS = 5000;
const float NO_MOTION_THRESHOLD_G = 0.2;
float prev_ax = 0, prev_ay = 0, prev_az = 0;

// --- DEKLARASI FUNGSI ---
void changeState(AlarmStateType newState);
void runFallDetectionStateMachine();


// =================================================================
// ==                          SETUP                                ==
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Booting perangkat Deteksi Jatuh (Mode WiFi)...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  mySensor.setWire(&Wire);
  mySensor.beginAccel();

  // --- KONEKSI WIFI VIA WIFIMANAGER ---
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Alat-Jatuh-Setup", "password123")) {
    Serial.println("Gagal terhubung. Restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("Berhasil terhubung via WiFi!");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  // --- SETUP FIREBASE ---
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Berhasil mendaftar (Sign Up) ke Firebase.");
    signupOK = true;
  } else {
    Serial.printf("Gagal Sign Up Firebase: %s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// =================================================================
// ==                           LOOP                                ==
// =================================================================
void loop() {
  // Hanya jalankan logika jika koneksi Firebase siap
  if (Firebase.ready() && signupOK) {
    // Logika pembatalan alarm universal
    if (currentAlarmState != IDLE && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
      Serial.println("Tombol batal ditekan. Alarm dibatalkan.");
      digitalWrite(BUZZER_PIN, LOW);
      changeState(IDLE);
    }
    // Menjalankan state machine deteksi
    runFallDetectionStateMachine();
  } else {
    Serial.println("Menunggu koneksi Firebase siap...");
    delay(2000);
  }
  delay(100);
}

// =================================================================
// ==                  FUNGSI-FUNGSI UTAMA                        ==
// =================================================================

void runFallDetectionStateMachine() {
  uint8_t result = mySensor.accelUpdate();
  if (result == 0) {
    float aX = mySensor.accelX(), aY = mySensor.accelY(), aZ = mySensor.accelZ();
    float mag = sqrt(aX * aX + aY * aY + aZ * aZ);

    switch (currentAlarmState) {
      case IDLE:
        digitalWrite(BUZZER_PIN, LOW); // Pastikan buzzer mati saat IDLE
        // Set nilai 'trig' ke 0 di Firebase secara berkala
        if (millis() - sendDataPrevMillis > 5000) {
          sendDataPrevMillis = millis();
          trig = 0;
          Firebase.RTDB.setIntAsync(&fbdo, deviceId + "/trig", trig);
        }
        // Cek kondisi jatuh bebas
        if (mag < FREEFALL_THRESHOLD_G) {
          changeState(FREEFALL_DETECTED);
        }
        break;

      case FREEFALL_DETECTED:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        // Set nilai 'trig' ke 1 di Firebase (hanya sekali)
        if (trig != 1) {
          trig = 1;
          Firebase.RTDB.setIntAsync(&fbdo, deviceId + "/trig", trig);
        }
        // Cek kondisi benturan
        if (mag > FALL_THRESHOLD_G) {
          prev_ax = aX; prev_ay = aY; prev_az = aZ;
          changeState(IMPACT_DETECTED);
        } else if (millis() - stateChangeTimestamp > 2000) {
          changeState(IDLE);
        }
        break;

      case IMPACT_DETECTED:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        // Set nilai 'trig' ke 2 di Firebase (hanya sekali)
        if (trig != 2) {
          trig = 2;
          Firebase.RTDB.setIntAsync(&fbdo, deviceId + "/trig", trig);
        }
        // Cek kondisi tidak bergerak setelah benturan
        if (millis() - stateChangeTimestamp > MIN_POST_FALL_NO_MOTION_MS) {
          float motionDelta = sqrt(pow(aX - prev_ax, 2) + pow(aY - prev_ay, 2) + pow(aZ - prev_az, 2));
          if (motionDelta < NO_MOTION_THRESHOLD_G) {
            changeState(ALARM_TRIGGERED);
          } else {
            changeState(IDLE);
          }
        }
        break;

      case ALARM_TRIGGERED:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        // Set nilai 'trig' ke 3 di Firebase (hanya sekali)
        if (trig != 3) {
            trig = 3;
            Firebase.RTDB.setIntAsync(&fbdo, deviceId + "/trig", trig);
            Serial.println("Jatuh Terkonfirmasi. Notifikasi dikirim berdasarkan 'trig'=3.");
        }
        changeState(NOTIFICATION_SENT);
        break;

      case NOTIFICATION_SENT:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        // Buzzer akan terus berbunyi sampai tombol batal ditekan
        break;
    }
  }
}

void changeState(AlarmStateType newState) {
  if (currentAlarmState != newState) {
    currentAlarmState = newState;
    stateChangeTimestamp = millis();
    
    String stateStr;
    switch (newState) {
      case IDLE: stateStr = "IDLE"; break;
      case FREEFALL_DETECTED: stateStr = "FREEFALL_DETECTED"; break;
      case IMPACT_DETECTED: stateStr = "IMPACT_DETECTED"; break;
      case ALARM_TRIGGERED: stateStr = "ALARM_TRIGGERED"; break;
      case NOTIFICATION_SENT: stateStr = "NOTIFICATION_SENT"; break;
    }
    Serial.println("Mengubah status ke: " + stateStr);
  }
}