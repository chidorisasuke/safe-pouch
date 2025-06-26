// =================================================================
// ==      SKETSA DEBUGGING DENGAN OUTPUT LOG KE BAWAH            ==
// =================================================================

#include <MPU9250_asukiaaa.h>
#include <Wire.h>

// -- Pinout Sensor --
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 2
#define SCL_PIN 1
#endif

// -- Objek Sensor --
MPU9250_asukiaaa mySensor;


// =================================================================
// ==      SYARAT & AMBANG BATAS (THRESHOLD) DETEKSI JATUH        ==
// =================================================================
const float FALL_THRESHOLD_G = 2.0;
const float FREEFALL_THRESHOLD_G = 0.5;
const unsigned long MIN_POST_FALL_NO_MOTION_MS = 5000;
const float NO_MOTION_THRESHOLD_G = 0.2;

// === DEKLARASI TIPE DATA (ENUM) & VARIABEL GLOBAL ===
enum AlarmStateType { IDLE, FREEFALL_DETECTED, IMPACT_DETECTED, ALARM_TRIGGERED, NOTIFICATION_SENT };
AlarmStateType currentAlarmState = IDLE;
unsigned long stateChangeTimestamp = 0;
float prev_ax = 0, prev_ay = 0, prev_az = 0;

// =================================================================
// ==                          SETUP                                ==
// =================================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("\nMemulai Sketsa Debugging (Format Log)...");
  
#ifdef _ESP32_HAL_I2C_H_ // For ESP32
  Wire.begin(SDA_PIN, SCL_PIN);
  mySensor.setWire(&Wire);
  mySensor.beginAccel();
#endif
  
  Serial.println("Sensor MPU9250 siap.");
  Serial.println("-------------------------------------------------");
  delay(2000);
}


// =================================================================
// ==                           LOOP                                ==
// =================================================================
void loop() {
  uint8_t result = mySensor.accelUpdate();
  if (result == 0) {
    float aX = mySensor.accelX();
    float aY = mySensor.accelY();
    float aZ = mySensor.accelZ();
    float currentAccelerationMagnitude = sqrt(aX * aX + aY * aY + aZ * aZ);

    // === BAGIAN UTAMA YANG DIUBAH: FORMAT OUTPUT ===
    Serial.println("--- Frame Data Baru ---");
    
    switch (currentAlarmState) {
      case IDLE:
        Serial.println("State Saat Ini   : IDLE");
        Serial.print("  > Nilai Magnitudo: ");
        Serial.print(currentAccelerationMagnitude, 2);
        Serial.println(" G");
        Serial.print("  * Syarat Berikutnya (Jatuh Bebas): < ");
        Serial.print(FREEFALL_THRESHOLD_G, 2);
        Serial.println(" G");
        
        if (currentAccelerationMagnitude < FREEFALL_THRESHOLD_G) {
          Serial.println("\n>>> KONDISI TERPENUHI! Pindah ke JATUH BEBAS <<<\n");
          currentAlarmState = FREEFALL_DETECTED;
          stateChangeTimestamp = millis();
        }
        break;
      
      case FREEFALL_DETECTED:
        Serial.println("State Saat Ini   : JATUH BEBAS");
        Serial.print("  > Nilai Magnitudo: ");
        Serial.print(currentAccelerationMagnitude, 2);
        Serial.println(" G");
        Serial.print("  * Syarat Berikutnya (Benturan): > ");
        Serial.print(FALL_THRESHOLD_G, 2);
        Serial.println(" G");
        
        if (currentAccelerationMagnitude > FALL_THRESHOLD_G) {
          Serial.println("\n>>> KONDISI TERPENUHI! Pindah ke BENTURAN <<<\n");
          prev_ax = aX; prev_ay = aY; prev_az = aZ;
          currentAlarmState = IMPACT_DETECTED;
          stateChangeTimestamp = millis();
        } else if (millis() - stateChangeTimestamp > 2000) {
          Serial.println("\n>>> Timeout 2 detik. Kembali ke IDLE <<<\n");
          currentAlarmState = IDLE;
        }
        break;

      case IMPACT_DETECTED: {
        float motionDelta = sqrt(pow(aX - prev_ax, 2) + pow(aY - prev_ay, 2) + pow(aZ - prev_az, 2));
        Serial.println("State Saat Ini   : BENTURAN");
        Serial.print("  > Nilai MotionDelta: ");
        Serial.print(motionDelta, 2);
        Serial.println(" G");
        Serial.print("  * Syarat Berikutnya (Tak Bergerak): < ");
        Serial.print(NO_MOTION_THRESHOLD_G, 2);
        Serial.println(" G (selama 5 detik)");
        
        if (millis() - stateChangeTimestamp > MIN_POST_FALL_NO_MOTION_MS) {
          if (motionDelta < NO_MOTION_THRESHOLD_G) {
            Serial.println("\n>>> KONDISI TERPENUHI! Pindah ke ALARM <<<\n");
            currentAlarmState = ALARM_TRIGGERED;
            stateChangeTimestamp = millis();
          } else {
            Serial.println("\n>>> Gerakan masih terdeteksi. Kembali ke IDLE <<<\n");
            currentAlarmState = IDLE;
          }
        }
        break;
      }

      case ALARM_TRIGGERED:
        Serial.println("State Saat Ini   : ALARM AKTIF");
        Serial.println("--> Semua syarat jatuh telah terpenuhi!");
        // Untuk debug, kita bisa reset manual atau biarkan saja
        break;
      
      case NOTIFICATION_SENT:
        Serial.println("State Saat Ini   : NOTIFIKASI");
        if (millis() - stateChangeTimestamp > 5000) {
          Serial.println("\n>>> Reset otomatis. Kembali ke IDLE <<<\n");
          currentAlarmState = IDLE;
        }
        break;
    }
    Serial.println("-------------------------");
  }
  
  delay(300); // Jeda sedikit lebih lama agar lebih mudah dibaca
}