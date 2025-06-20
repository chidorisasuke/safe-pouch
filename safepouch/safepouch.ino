// =================================================================
// ==      KODE FINAL DETEKSI JATUH DENGAN BUZZER PROAKTIF        ==
// =================================================================
#define TINY_GSM_DEBUG Serial
#define TINY_GSM_MODEM_SIM800


#include <MPU9250_asukiaaa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <TinyGsmClient.h>
#include <ArduinoWebsockets.h>

using namespace websockets;

// --- KONFIGURASI UTAMA ---
// UBAH NILAI INI UNTUK BERALIH ANTARA TES LOKAL DAN SERVER PUBLIK
// 1 = Untuk tes di jaringan lokal (WS)
// 0 = Untuk server publik di internet (WSS)
#define LOKAL_DEBUG 1

// --- PINOUT & HARDWARE ---
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 2
#define SCL_PIN 1
#endif
#define SerialAT Serial1 // Gunakan Serial1 untuk ESP32-C6
const int BUZZER_PIN = 18;
const int CANCEL_BUTTON_PIN = 4;

// --- KONFIGURASI SERVER (OTOMATIS BERDASARKAN LOKAL_DEBUG) ---
#if LOKAL_DEBUG == 1
  const char* websocket_server_host = "192.168.1.7"; // Ganti dengan IP LOKAL ANDA
  const uint16_t websocket_server_port_lokal = 3000;
#else
  const char* websocket_server_host = "coral-rich-distinctly.ngrok-free.app"; // Ganti dengan URL PUBLIK ANDA
  const uint16_t websocket_server_port = 80;
  // const uint16_t websocket_server_port_ssl = 443;
  // const uint16_t websocket_server_port_non_ssl = 80;
#endif
String deviceId = "ESP32-DUAL-01";

// --- KONFIGURASI JARINGAN SELULER ---
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- OBJEK & VARIABEL GLOBAL ---
MPU9250_asukiaaa mySensor;
TinyGsm modem(SerialAT);
TinyGsmClient gsm_client(modem);
WebsocketsClient client;

enum ConnectionMode { MODE_WIFI, MODE_GSM };
enum AlarmStateType { IDLE, FREEFALL_DETECTED, IMPACT_DETECTED, ALARM_TRIGGERED, NOTIFICATION_SENT };
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
void sendStateUpdate(AlarmStateType newState);
void changeState(AlarmStateType newState);
void connectToWebSocket();
void runFallDetectionStateMachine();


// =================================================================
// ==                          SETUP                              ==
// =================================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);
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
    connectToWebSocket();
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
    connectToWebSocket();
  }
}

// =================================================================
// ==                           LOOP                              ==
// =================================================================
void loop() {
  if (currentMode == MODE_GSM) modem.maintain();
  
  if (!client.available()) {
    delay(5000);
    connectToWebSocket();
  }
  client.poll();

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
        if (mag < FREEFALL_THRESHOLD_G) {
          changeState(FREEFALL_DETECTED);
        }
        break;
      
      case FREEFALL_DETECTED:
        // BARU: Buzzer langsung aktif saat jatuh bebas
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2); 

        if (mag > FALL_THRESHOLD_G) {
          prev_ax = aX; prev_ay = aY; prev_az = aZ;
          changeState(IMPACT_DETECTED);
        } else if (millis() - stateChangeTimestamp > 2000) {
          Serial.println("Timeout jatuh bebas (tidak ada benturan), kembali ke IDLE.");
          digitalWrite(BUZZER_PIN, LOW); // Matikan buzzer jika alarm palsu
          changeState(IDLE);
        }
        break;

      case IMPACT_DETECTED:
        // BARU: Buzzer tetap aktif setelah benturan
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);

        if (millis() - stateChangeTimestamp > MIN_POST_FALL_NO_MOTION_MS) {
          float motionDelta = sqrt(pow(aX-prev_ax,2) + pow(aY-prev_ay,2) + pow(aZ-prev_az,2));
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
        Serial.println("Jatuh Terkonfirmasi. Notifikasi dikirim oleh server.");
        changeState(NOTIFICATION_SENT);
        break;
      
      case NOTIFICATION_SENT:
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
        // Buzzer akan terus berbunyi sampai tombol batal ditekan
        break;
    }
  }
}

void connectToWebSocket() {
  Serial.println("Mencoba terhubung ke WebSocket Server...");
  bool connected = false;

  #if LOKAL_DEBUG == 1
    Serial.println("Mode Debug Lokal (WS)...");
    connected = client.connect(websocket_server_host, websocket_server_port_lokal, "/");
  #else
    if (currentMode == MODE_WIFI) {
      Serial.println("Mode Produksi WiFi (WS)...");
      connected = client.connect(websocket_server_host, websocket_server_port, "/");
    } else { // MODE_GSM
      Serial.println("Mode Produksi GPRS (WS)...");
      connected = client.connect(websocket_server_host, websocket_server_port, "/");
    }
  #endif
  
  if (connected) {
    Serial.println("Berhasil terhubung ke WebSocket!");
    sendStateUpdate(currentAlarmState);
  } else {
    Serial.println("Gagal terhubung ke WebSocket.");
  }
}

void sendStateUpdate(AlarmStateType newState) {
  if (!client.available()) return;
  String stateStr;
  switch (newState) {
    case IDLE: stateStr = "IDLE"; break;
    case FREEFALL_DETECTED: stateStr = "FREEFALL_DETECTED"; break;
    case IMPACT_DETECTED: stateStr = "IMPACT_DETECTED"; break;
    case ALARM_TRIGGERED: stateStr = "ALARM_TRIGGERED"; break;
    case NOTIFICATION_SENT: stateStr = "NOTIFICATION_SENT"; break;
  }
  String jsonMessage = "{\"type\":\"state_update\",\"deviceId\":\"" + deviceId + "\",\"state\":\"" + stateStr + "\"}";
  Serial.println("Mengirim status baru: " + stateStr);
  client.send(jsonMessage);
}

void changeState(AlarmStateType newState) {
  if (currentAlarmState != newState) {
    currentAlarmState = newState;
    stateChangeTimestamp = millis();
    sendStateUpdate(currentAlarmState);
  }
}