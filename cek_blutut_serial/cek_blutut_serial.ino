// ==========================================================
// ==      CONTOH BLUETOOTH SERIAL (BLE) UNTUK ESP32-C6     ==
// ==========================================================

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Deklarasi pointer untuk Server dan Karakteristik
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL; // Karakteristik untuk mengirim data (TX)
BLECharacteristic* pRxCharacteristic = NULL; // Karakteristik untuk menerima data (RX)

bool deviceConnected = false;
unsigned long lastMsgTime = 0;

// UUID (Alamat unik) untuk layanan dan karakteristik UART
// Ini adalah UUID standar yang banyak digunakan untuk serial-over-BLE
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


// Callback untuk event koneksi server
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Perangkat Terhubung");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Koneksi Terputus");
      // Mulai advertising lagi agar bisa ditemukan
      pServer->getAdvertising()->start();
    }
};

// Callback untuk event penulisan data (saat ESP32 menerima data)
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue();      // BENAR

      if (rxValue.length() > 0) {
        Serial.print("Menerima data dari Bluetooth: ");
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        Serial.println();
      }
    }
};


void setup() {
  Serial.begin(115200);
  Serial.println("Memulai BLE Server...");

  // 1. Buat perangkat BLE dan beri nama
  BLEDevice::init("ESP32-C6 Fall Detector");

  // 2. Buat Server BLE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 3. Buat Layanan (Service) UART
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 4. Buat Karakteristik TX (untuk ESP32 mengirim data)
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // 5. Buat Karakteristik RX (untuk ESP32 menerima data)
  pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_WRITE
                     );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // 6. Mulai layanan dan advertising
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Perangkat siap di-scan! Cari nama 'ESP32-C6 Fall Detector'");
}

void loop() {
  // Kirim pesan ke client yang terhubung setiap 2 detik
  if (deviceConnected) {
    if (millis() - lastMsgTime > 2000) {
      String message = "Hello from ESP32-C6! Time: " + String(millis()/1000);
      pTxCharacteristic->setValue(message.c_str());
      pTxCharacteristic->notify(); // Kirim notifikasi ke client
      Serial.printf("Mengirim pesan: %s\n", message.c_str());
      lastMsgTime = millis();
    }
  }
}