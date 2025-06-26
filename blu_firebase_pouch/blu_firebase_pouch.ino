// =================================================================
// ==      KODE FINAL DETEKSI JATUH (TANPA WEBSOCKET)             ==
// =================================================================
#define TINY_GSM_DEBUG Serial
#define TINY_GSM_MODEM_SIM800

#include <MPU9250_asukiaaa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <TinyGsmClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- PINOUT & HARDWARE ---
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 2
#define SCL_PIN 1
#endif
#define SerialAT Serial1 // Gunakan Serial1 untuk ESP32-C6

// --- KONFIGURASI FIREBASE & JARINGAN ---
#define API_KEY "AIzaSyBFNWuTAS0G1C2QRtF8ZCaX4wrlEXC_F3s"
#define DATABASE_URL "https://safepouch-302b1-default-rtdb.asia-southeast1.firebasedatabase.app/"
String deviceId = "ESP32-DUAL-02";

const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- OBJEK & VARIABEL GLOBAL ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

MPU9250_asukiaaa mySensor;
TinyGsm modem(SerialAT);
// Catatan: TinyGsmClient hanya akan digunakan jika Firebase memerlukan
// koneksi client, namun inisialisasinya tetap dibutuhkan untuk modem.
TinyGsmClient gsm_client(modem);

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
int trig = 0;

const int BUZZER_PIN = 18;
const int CANCEL_BUTTON_PIN = 4;

enum ConnectionMode { MODE_WIFI,
                      MODE_GSM };
enum AlarmStateType { IDLE,
                      FREEFALL_DETECTED,
                      IMPACT_DETECTED,
                      ALARM_TRIGGERED,
                      NOTIFICATION_SENT };
ConnectionMode currentMode;
AlarmStateType currentAlarmState = IDLE;
unsigned long stateChangeTimestamp = 0;

const float FALL_THRESHOLD_G = 2.8;
const float FREEFALL_THRESHOLD_G = 0.5;
const unsigned long MIN_POST_FALL_NO_MOTION_MS = 5000;
const float NO_MOTION_THRESHOLD_G = 0.2;
const unsigned long ALARM_CANCEL_WINDOW_MS = 20000;
float prev_ax = 0, prev_ay = 0, prev_az = 0;

// --- DEKLARASI FUNGSI ---
void changeState(AlarmStateType newState);
void runFallDetectionStateMachine();


// =================================================================
// ==                           SETUP                             ==
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("Booting perangkat Deteksi Jatuh...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  mySensor.setWire(&Wire);
  mySensor.beginAccel();

  // Logika Koneksi (WiFiManager + Fallback GSM)
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (wm.autoConnect("Alat-Jatuh-Setup", "password123")) {
    Serial.println("Berhasil terhubung via WiFi!");
    currentMode = MODE_WIFI;
  } else {
    Serial.println("Gagal terhubung via WiFi. Beralih ke mode GSM...");
    currentMode = MODE_GSM;
    WiFi.mode(WIFI_OFF);
    SerialAT.begin(9600);
    delay(6000);
    if (!modem.init() || !modem.waitForNetwork() || !modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("Gagal memulai mode GSM. Restarting...");
      delay(10000);
      ESP.restart();
    }
    Serial.println("Mode GSM Siap.");
  }

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
  
  // Menghubungkan kembali WiFi/GSM untuk Firebase
  if (currentMode == MODE_WIFI) {
      Firebase.reconnectWiFi(true);
  } else { // MODE_GSM
      // Koneksi GSM sudah ditangani, Firebase akan menggunakan client yang ada
  }
}

// =================================================================
// ==                            LOOP                             ==
// =================================================================
void loop() {
  // Jika dalam mode GSM, jalankan proses maintenance koneksi
  if (currentMode == MODE_GSM) {
    modem.maintain();
  }

  // Logika pembatalan alarm universal
  if (currentAlarmState != IDLE && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
    Serial.println("Tombol batal ditekan. Alarm dibatalkan.");
    digitalWrite(BUZZER_PIN, LOW);
    changeState(IDLE);
  }

  // Menjalankan state machine deteksi
  runFallDetectionStateMachine();

  delay(100);
}

// =================================================================
// ==                   FUNGSI-FUNGSI UTAMA                       ==
// =================================================================

void runFallDetectionStateMachine() {
  uint8_t result = mySensor.accelUpdate();
  if (result == 0) {
    float aX = mySensor.accelX(), aY = mySensor.accelY(), aZ = mySensor.accelZ();
    float mag = sqrt(aX * aX + aY * aY + aZ * aZ);

    switch (currentAlarmState) {
      case IDLE:
        // Set nilai 'trig' ke 0 di Firebase secara berkala
        if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
            sendDataPrevMillis = millis();
            trig = 0;
            if (Firebase.RTDB.setInt(&fbdo, deviceId + "/trig", trig)) {
              Serial.println("Status IDLE (trig=0) dikirim ke Firebase.");
            } else {
              Serial.println("Gagal kirim status IDLE: " + fbdo.errorReason());
            }
        }
        // Cek kondisi jatuh bebas
        if (mag < FREEFALL_THRESHOLD_G) {
          changeState(FREEFALL_DETECTED);
        }
        break;

      case FREEFALL_DETECTED:
        // Buzzer aktif saat jatuh bebas terdeteksi
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        
        // Set nilai 'trig' ke 1 di Firebase
        if (Firebase.ready() && signupOK && trig != 1) { // Kirim hanya sekali per state
            trig = 1;
            if (Firebase.RTDB.setInt(&fbdo, deviceId + "/trig", trig)) {
                Serial.println("Status FREEFALL_DETECTED (trig=1) dikirim ke Firebase.");
            } else {
                Serial.println("Gagal kirim status FREEFALL: " + fbdo.errorReason());
            }
        }

        // Cek kondisi benturan
        if (mag > FALL_THRESHOLD_G) {
          prev_ax = aX;
          prev_ay = aY;
          prev_az = aZ;
          changeState(IMPACT_DETECTED);
        } else if (millis() - stateChangeTimestamp > 2000) {
          Serial.println("Timeout jatuh bebas (tidak ada benturan), kembali ke IDLE.");
          digitalWrite(BUZZER_PIN, LOW); // Matikan buzzer jika alarm palsu
          changeState(IDLE);
        }
        break;

      case IMPACT_DETECTED:
        // Buzzer tetap aktif setelah benturan
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);

        // Set nilai 'trig' ke 2 di Firebase
        if (Firebase.ready() && signupOK && trig != 2) { // Kirim hanya sekali per state
            trig = 2;
            if (Firebase.RTDB.setInt(&fbdo, deviceId + "/trig", trig)) {
                Serial.println("Status IMPACT_DETECTED (trig=2) dikirim ke Firebase.");
            } else {
                Serial.println("Gagal kirim status IMPACT: " + fbdo.errorReason());
            }
        }

        // Cek kondisi tidak bergerak setelah benturan
        if (millis() - stateChangeTimestamp > MIN_POST_FALL_NO_MOTION_MS) {
          float motionDelta = sqrt(pow(aX - prev_ax, 2) + pow(aY - prev_ay, 2) + pow(aZ - prev_az, 2));
          if (motionDelta < NO_MOTION_THRESHOLD_G) {
            changeState(ALARM_TRIGGERED);
          } else {
            Serial.println("Gerakan terdeteksi setelah benturan, kembali ke IDLE.");
            digitalWrite(BUZZER_PIN, LOW); // Matikan buzzer jika alarm palsu
            changeState(IDLE);
          }
        }
        break;

      case ALARM_TRIGGERED:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        Serial.println("Jatuh Terkonfirmasi. Notifikasi harusnya dikirim berdasarkan nilai 'trig' di Firebase.");
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
    // Komunikasi ke server kini ditangani langsung di dalam state machine
    // menggunakan update Firebase berdasarkan nilai 'trig'
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