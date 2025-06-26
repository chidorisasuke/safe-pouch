/*
  ESP32C6 Active Low Buzzer Test
  Buzzer on Pin 18
  Active Low: LOW = ON, HIGH = OFF
*/

#define BUZZER_PIN 18

void setup() {
  Serial.begin(115200); // Inisialisasi Serial Monitor
  delay(1000); // Beri waktu untuk Serial Monitor siap

  Serial.println("Setupulai...");
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("Mengatur buzzer ke OFF (HIGH untuk active low) awal.");
  digitalWrite(BUZZER_PIN, HIGH); // Awalnya buzzer MATI (karena active low)
  delay(2000); // Tahan kondisi mati selama 2 detik untuk verifikasi

  // --- Tes Dasar (opsional, uncomment salah satu untuk tes) ---
  // Serial.println("TES: Menyalakan buzzer secara permanen (LOW)");
  // digitalWrite(BUZZER_PIN, LOW);
  // while(1); // Loop tak terbatas, buzzer harusnya bunyi terus

  // Serial.println("TES: Mematikan buzzer secara permanen (HIGH)");
  // digitalWrite(BUZZER_PIN, HIGH);
  // while(1); // Loop tak terbatas, buzzer harusnya mati terus
  // --- Akhir Tes Dasar ---

  Serial.println("Setup selesai. Masuk ke loop utama.");
}

void loop() {
  buzzer_cycle();
}

// Fungsi led_blink() tidak dipanggil di loop utama,
// Anda bisa menghapusnya jika tidak digunakan atau memanggilnya jika diperlukan.
/*
void led_blink(){
  #ifdef RGB_BUILTIN
  digitalWrite(RGB_BUILTIN, HIGH);   // Turn the RGB LED white
  delay(1000);
  digitalWrite(RGB_BUILTIN, LOW);    // Turn the RGB LED off
  delay(1000);

  neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,0,0); // Red
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0); // Green
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS); // Blue
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,0,0); // Off / black
  delay(1000);
  #endif
}
*/

void buzzer_cycle(){
  Serial.println("Siklus Buzzer: Menyalakan (LOW)");
  digitalWrite(BUZZER_PIN, LOW); // Nyalakan buzzer (karena active low)
  delay(3000);                   // Biarkan menyala selama 3 detik

  Serial.println("Siklus Buzzer: Mematikan (HIGH)");
  digitalWrite(BUZZER_PIN, HIGH);  // Matikan buzzer (karena active low)
  delay(3000);                   // Biarkan mati selama 3 detik
}