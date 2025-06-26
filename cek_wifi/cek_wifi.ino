// ======================================================
// ==         SKETSA TES MINIMAL WIFI MANAGER          ==
// ======================================================

#include <WiFi.h>
#include <WiFiManager.h>

void setup() {
  // Mulai Serial Monitor untuk melihat log
  Serial.begin(115200);
  while(!Serial);
  
  Serial.println("\nMemulai Sketsa Tes WiFiManager...");

  // Buat objek WiFiManager
  WiFiManager wm;

  // Opsi ini akan menghapus semua kredensial WiFi yang tersimpan
  // Aktifkan baris ini HANYA jika Anda ingin memaksa portal konfigurasi muncul
  // Setelah berhasil, beri komentar lagi agar tidak selalu reset.
  // wm.resetSettings();

  // Mengatur batas waktu portal menjadi 3 menit (180 detik)
  wm.setConfigPortalTimeout(180);

  Serial.println("Mencoba memulai autoConnect...");

  // autoConnect akan mencoba terhubung ke WiFi terakhir.
  // Jika gagal, ia akan membuat AP dengan nama "Alat-Jatuh-Setup"
  bool res = wm.autoConnect("Alat-Jatuh-Setup", "password123");
    
  if(!res) {
    Serial.println("Gagal terhubung dan waktu habis. Restarting...");
    delay(3000);
    ESP.restart();
  } else {
    // Jika sampai di sini, artinya koneksi WiFi berhasil!
    Serial.println("WiFi berhasil terhubung!");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  // Loop sengaja dibiarkan kosong untuk tes ini
}