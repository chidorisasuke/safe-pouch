#include <Wire.h>
#include <MPU9250.h>
#include <WiFi.h>
#include <HTTPClient.h>

const int BUZZER_PIN = 18;
const int CANCEL_BUTTON_PIN = 19;

const char* ssid = "bakpao";
const char* password = "juraganlombok";

String webNotificationURL = "";
// Anda bisa menambahkan parameter query jika server Anda memerlukannya, contoh:
// String webNotificationURL = "http://yourserver.com/notify_fall?deviceId=esp32c6_alatJatuh_01";

const float FALL_THRESHOLD_G = 3.0;     // Ambang batas G-force untuk benturan (misal >3G)
const float FREEFALL_THRESHOLD_G = 0.5; // Ambang batas G-force untuk jatuh bebas (misal <0.5G)
const unsigned long MIN_POST_FALL_NO_MOTION_MS = 5000; // Waktu minimal tidak ada gerakan signifikan setelah benturan (ms)
const float NO_MOTION_THRESHOLD_G = 0.2;// Ambang batas untuk mendeteksi "tidak ada gerakan" (perubahan G)

MPU9250 mpu; //Objek MPU9250

enum AlarmStateType {
  IDLE,             // Kondisi normal
  FREEFALL_DETECTED,// Terdeteksi kondisi jatuh bebas
  IMPACT_DETECTED,   // Terdeteksi benturan setelah jatuh bebas
  ALARM_TRIGGERED,  // Buzzer aktif, menunggu tombol batal
  NOTIFICATION_SENT // Notifikasi telah dikirim
};

AlarmStateType currentAlarmState = IDLE;

unsigned long fallDetectTimestamp = 0; // Timestamp untuk berbagai tahap deteksi/alarm
unsigned long impactTimestamp = 0;        // Timestamp saat benturan terdeteksi
const unsigned long ALARM_CANCEL_WINDOW_MS = 20000; // Waktu (ms) bagi pengguna untuk menekan tombol batal (20 detik)

//Variabel untuk data MPU
float ax, ay, az; //Data akselerometer
float prev_ax = 0, prev_ay = 0, prev_az = 0; //Nilai akselerasi

void setup() {
  Serial.begin(115200);
  // while (!Serial); //Tunggu koneksi serial

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); //Kondisi awal buzzer harusnnya mati, coba cek kode buzzer HIGH atau LOW dulu

  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP); //Tombol terhubung ke GND saat di tekan
  Wire.begin(1, 2); //SDA = GPIO1, SCL = GPIO9
  delay(1000);

  if (!mpu.setup(0x68)) { //Alamat umum I2C ketika ADO terhubung ke GND
    Serial.println("Koneksi MPU9250 gagal. periksa kabel.");
    while(1); //Hentikan program jika MPU Gagal
  }

  Serial.println("MPU9250 berhasil diinisialisasi.");
  // Konfigurasi MPU9250 jika perlu (misal, rentang akselerometer)
  // mpu.setAccelRange(MPU9250::ACCEL_RANGE_8G); // Contoh set rentang akselerometer ke +/- 8G
  // mpu.setGyroRange(MPU9250::GYRO_RANGE_500DPS); // Contoh set rentang giroskop
  // mpu.setSampleRate(100); // Atur sample rate (Hz)

  //Inisialisasi WIFI
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 30){ // Coba koneksi selama maks ~15 detik
    delay(500);
    Serial.print(".");
    wifi_retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi terhubung!");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nKoneksi WiFi gagal. Notifikasi web tidak berfungsi.");
    // Di sini Anda bisa mengimplementasikan fallback menggunakan SIM800L GPRS jika diinginkan.
  }
}

void loop() {
  if (mpu.update()) {
    ax = mpu.getAccX(); // Dapatkan akselerasi sumbu X (dalam G)
    ay = mpu.getAccY(); // Dapatkan akselerasi sumbu Y (dalam G)
    az = mpu.getAccZ(); // Dapatkan akselerasi sumbu Z (dalam G)

    // Magnitudo akselerasi gabungan sederhana
    float currentAccelerationMagnitude = sqrt(ax * ax + ay * ay + az * az);

    // --- Logika Deteksi Jatuh (State Machine) ---
    switch (currentAlarmState) {
      case IDLE:
        if (currentAccelerationMagnitude < FREEFALL_THRESHOLD_G){
          Serial.println("Potensi Jatuh Bebas terdeteksi!");
          currentAlarmState = FREEFALL_DETECTED;
          fallDetectTimestamp = millis();
        }
        break;
      
      case FREEFALL_DETECTED:
        // Periksa benturan setelah jatuh bebas
        if (currentAccelerationMagnitude > FALL_THRESHOLD_G) {
          Serial.println("Benturan terdeteksi setelah jatuh bebas!");
          currentAlarmState = IMPACT_DETECTED;
          impactTimestamp = millis();// Catat waktu benturan
          // Simpan nilai akselerasi saat ini untuk pengecekan "tidak ada gerakan"
          prev_ax = ax; prev_ay = ay; prev_az = az;
        } else if (millis() - fallDetectTimestamp > 2000) {
          Serial.println("Timeout jatuh bebas, kembali ke IDLE.");
          currentAlarmState = IDLE;
        }
        break;

      case IMPACT_DETECTED:
        // Periksa periode tidak ada gerakan signifikan setelah benturan
        // Ini mengindikasikan orang tersebut mungkin tidak responsif
        if (millis() - impactTimestamp > MIN_POST_FALL_NO_MOTION_MS){
          //Hitung perubahan magnitude akselerasi dari saat benturan
          float motionDelta = sqrt(pow(ax - prev_ax, 2) + pow(ay - prev_ay, 2) + pow(az - prev_az, 2));
          if (motionDelta < NO_MOTION_THRESHOLD_G) {
            Serial.println("Jatuh terkonfirmasi (Jatuh Bebas + Benturan + Tidak ada gerakan). Memicu alarm");
            currentAlarmState = ALARM_TRIGGERED;
            fallDetectTimestamp = millis(); // Gunakan lagi timestamp untuk jendela alarm
            digitalWrite(BUZZER_PIN, HIGH); // Nyalakan buzzer
          } else {
            Serial.println("Gerakan terdeteksi setelah benturan, kemungkinan bukan jatuh parah atau orang sudah pulih");
            currentAlarmState = IDLE; //Reset jika terdeteksi gerakan signifikan
          }
        }
        // Selama jendela pengecekan "tidak ada geraka", bisa juga terus update prev_ax, dsb.
        // Untuk simplisitas, di sini kita menunggu MIN_POST_FALL_NO_MOTION_MS berlalu.
        break;

      case ALARM_TRIGGERED:
        Serial.println("Alarm aktif. Tekan tombol batal dalam " + String(ALARM_CANCEL_WINDOW_MS / 1000) + "detik.");
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2); // Kedipkan buzzer

        if (digitalRead(CANCEL_BUTTON_PIN) == LOW) {
          Serial.println("Tombol batal ditekan. Alarm dibatalkan");
          digitalWrite(BUZZER_PIN, LOW);
          currentAlarmState = IDLE;
        } else if (millis() - fallDetectTimestamp > ALARM_CANCEL_WINDOW_MS) {
          Serial.println("Jendela alarm berakhir. Mengirim notifikasi web.");
          sendWebNotification();
          digitalWrite(BUZZER_PIN, LOW);
          currentAlarmState = NOTIFICATION_SENT; //Pindah ke state notifikasi terkirim
        }
        break;
      
      case NOTIFICATION_SENT:
        // Alat dalam kondisi ini setelah mengirim notifikasi.
        // Bisa direset otomatis ke IDLE setelah beberapa saat, atau butuh reset manual.
        Serial.println("Notifikasi terkirim. Sistem menunggu atau akan re-arm otomatis.");
        delay(5000);
        currentAlarmState = IDLE; // Re-arm otomatis
        Serial.println("Sistem siap kembali (re-armed).");
        break;
    }
  } else {
    Serial.println("Gagal membaca data MPU9250."); // Aktifkan jika perlu debug
  }
  delay(100);

}

void sendWebNotification() {
  if (WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String fullUrl = webNotificationURL;
    // Anda bisa menambahkan data ke URL jika server Anda mengharapkannya
    // fullUrl += "?latitude=xxx&longitude=yyy&timestamp=" + String(millis());

    Serial.print("Mengirim permintaan GET ke: ");
    Serial.println(fullUrl);

    http.begin(fullUrl); // Tentukan URL target
    int httpCode = http.GET(); //Lakukan permintaan GET

    if (httpCode > 0){
      String payload = http.getString();
      Serial.println("Kode Respons HTTP: " + String(httpCode));
      Serial.println("Payload Respons: " + payload); 
    } else {
      Serial.print("Permintaan GET HTTP gagal, error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi tidak terhubung. Tidak dapat mengirim notifikasi web.");
    // Di sini Anda bisa memicu SIM800L untuk mengirim SMS sebagai backup
    // sendSmsViaSim800L("Terdeteksi jatuh! Notifikasi web gagal terkirim.");
  }
}

/*
// --- Contoh Fungsi untuk SIM800L (Placeholder) ---
// Anda memerlukan library SoftwareSerial atau menggunakan HardwareSerial untuk SIM800L
// #include <SoftwareSerial.h>
// SoftwareSerial sim800lSerial(SIM_TX_PIN, SIM_RX_PIN); // Ganti dengan pin TX/RX SIM800L Anda

void sendSmsViaSim800L(String message) {
  Serial.println("Mencoba mengirim SMS via SIM800L...");
  // Pastikan sim800lSerial sudah di-begin di setup()
  // sim800lSerial.begin(9600); // Atau baud rate yang sesuai
  // delay(1000);

  // sim800lSerial.println("AT"); // Cek komunikasi
  // delay(100);
  // sim800lSerial.println("AT+CMGF=1"); // Set mode SMS ke teks
  // delay(200);
  // sim800lSerial.println("AT+CMGS=\"+62NOMORTUJUAN\""); // Ganti dengan nomor telepon tujuan
  // delay(200);
  // sim800lSerial.print(message); // Isi pesan SMS
  // delay(200);
  // sim800lSerial.write(26); // Karakter Ctrl+Z untuk mengirim SMS
  // delay(1000);
  // Serial.println("SMS (diasumsikan) terkirim via SIM800L.");
}
*/