// =================================================================
// ==      KODE DETEKSI JATUH (KONEKSI KHUSUS GSM + FIREBASE)     ==
// =================================================================

// --- PENGATURAN LIBRARY ---
#define TINY_GSM_DEBUG Serial
#define TINY_GSM_MODEM_SIM800

#include <MPU9250_asukiaaa.h>
#include <TinyGsmClient.h>         // Pustaka utama untuk koneksi GSM

#define FIREBASE_ESP_CLIENT_ENABLE_GSM 

#include <Firebase_ESP_Client.h>   // Untuk koneksi ke Firebase
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// --- KONFIGURASI FIREBASE & JARINGAN ---
#define API_KEY "AIzaSyBFNWuTAS0G1C2QRtF8ZCaX4wrlEXC_F3s" // Ganti dengan API Key Anda
#define DATABASE_URL "https://safepouch-302b1-default-rtdb.asia-southeast1.firebasedatabase.app/" // Ganti dengan URL DB Anda
String deviceId = "ESP32-GSM-01"; // ID unik untuk perangkat GSM ini

// --- PINOUT & HARDWARE ---
#define SDA_PIN 2
#define SCL_PIN 1
#define SerialAT Serial1 // Gunakan Serial1 untuk ESP32-C6
const int BUZZER_PIN = 18;
const int CANCEL_BUTTON_PIN = 4;

// --- KONFIGURASI JARINGAN SELULER ---
const char apn[]  = "internet"; // APN provider (sesuaikan jika perlu, "internet" umum untuk Telkomsel/Indosat)
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- OBJEK & VARIABEL GLOBAL ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
MPU9250_asukiaaa mySensor;
TinyGsm modem(SerialAT);
TinyGsmClient gsm_client(modem); // Client untuk koneksi internet via GSM

bool firebaseReady = false;
int trig = 0;
unsigned long sendDataPrevMillis = 0;

enum AlarmStateType { IDLE, FREEFALL_DETECTED, IMPACT_DETECTED, ALARM_TRIGGERED, NOTIFICATION_SENT };
AlarmStateType currentAlarmState = IDLE;
unsigned long stateChangeTimestamp = 0;

// ... (Variabel threshold dan logika jatuh lainnya tetap sama) ...
const float FALL_THRESHOLD_G = 2.8;
const float FREEFALL_THRESHOLD_G = 0.5;
// ... (sisa variabel Anda) ...

// --- DEKLARASI FUNGSI ---
void changeState(AlarmStateType newState);
void runFallDetectionStateMachine();

// =================================================================
// ==                          SETUP                                ==
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Booting perangkat Deteksi Jatuh (Mode GSM)...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  mySensor.setWire(&Wire);
  mySensor.beginAccel();

  // --- LANGKAH 1: INISIALISASI MODEM SIM800L ---
  SerialAT.begin(9600); // Mulai komunikasi dengan modem
  delay(6000); // Beri waktu modem untuk boot dan mencari sinyal

  Serial.println("Menginisialisasi modem...");
  if (!modem.init()) {
    Serial.println("Gagal menginisialisasi modem, akan restart dalam 10 detik...");
    delay(10000);
    ESP.restart();
  }
  
  Serial.print("Menunggu jaringan seluler...");
  if (!modem.waitForNetwork()) {
    Serial.println(" Gagal. Periksa antena dan kartu SIM.");
    delay(10000);
    ESP.restart();
  }
  Serial.println(" OK");

  Serial.print("Menghubungkan ke GPRS...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" Gagal.");
    delay(10000);
    ESP.restart();
  }
  Serial.println(" OK. GPRS Terhubung.");

  // --- LANGKAH 2: SETUP FIREBASE SETELAH GPRS AKTIF ---
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Berhasil mendaftar (Sign Up) ke Firebase.");
    firebaseReady = true;
  } else {
    Serial.printf("Gagal Sign Up Firebase: %s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;

  // Beritahu Firebase untuk menggunakan client GSM kita
  // config.tcp_client = &gsm_client; 

  Firebase.begin(&config, &auth);
  // Tidak perlu memanggil reconnectWiFi karena kita tidak pakai WiFi
}

// =================================================================
// ==                           LOOP                                ==
// =================================================================
void loop() {
  // Selalu jalankan modem.maintain() untuk menjaga koneksi GPRS
  modem.maintain();

  if (Firebase.ready() && firebaseReady) {
    // Logika pembatalan alarm
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
  // ... (Logika di dalam fungsi ini tidak berubah sama sekali) ...
  // ... (Ia akan memanggil changeState(), yang sudah diubah untuk Firebase) ...
}

void changeState(AlarmStateType newState) {
  if (currentAlarmState != newState) {
    currentAlarmState = newState;
    stateChangeTimestamp = millis();
    
    String stateStr;
    int stateInt;
    switch (newState) {
      case IDLE: stateStr = "IDLE"; stateInt = 0; break;
      case FREEFALL_DETECTED: stateStr = "FREEFALL_DETECTED"; stateInt = 1; break;
      case IMPACT_DETECTED: stateStr = "IMPACT_DETECTED"; stateInt = 2; break;
      case ALARM_TRIGGERED: stateStr = "ALARM_TRIGGERED"; stateInt = 3; break;
      case NOTIFICATION_SENT: stateStr = "NOTIFICATION_SENT"; stateInt = 4; break;
    }
    Serial.println("Mengubah status ke: " + stateStr);

    // Kirim update ke Firebase
    String path = deviceId + "/trig"; // Path di database Anda
    if (Firebase.RTDB.setInt(&fbdo, path, stateInt)) {
      Serial.printf("SUKSES: Mengirim nilai %d ke Firebase.\n", stateInt);
    } else {
      Serial.println("GAGAL: Kirim ke Firebase: " + fbdo.errorReason());
    }
  }
}