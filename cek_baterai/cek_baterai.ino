// ========================================================
// ==      SKETSA DEBUGGING MONITOR BATERAI              ==
// ========================================================

// --- KONFIGURASI PIN DAN PEMBAGI TEGANGAN ---
const int BATT_ADC_PIN = 5; // Pin ADC untuk membaca tegangan

// Masukkan nilai resistor yang Anda gunakan
// Nilai float (misal 100000.0) penting untuk akurasi perhitungan
const float R1 = 98600.0;
const float R2 = 96500.0;

// Konstanta untuk perhitungan ADC
const float V_REF = 3.3; // Tegangan referensi ADC ESP32.
                         // Beberapa board mungkin punya referensi berbeda, perlu dikalibrasi.
const int ADC_RESOLUTION = 4095; // Resolusi ADC 12-bit (0-4095)

// BARU: Faktor Koreksi dari hasil pengukuran Multimeter
// Hitungan kita: 3.9V (asli) / 3.17V (kalkulasi) = 1.23
const float FAKTOR_KALIBRASI = 1.23;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("\nMemulai program Monitor Baterai...");
  Serial.println("------------------------------------");
}

void loop() {
  // 1. Baca nilai mentah dari pin ADC (0-4095)
  int adcValue = analogRead(BATT_ADC_PIN);

  // 2. Ubah nilai ADC menjadi tegangan pada pin (V_out)
  float pinVoltage = (adcValue / (float)ADC_RESOLUTION) * V_REF;
  
  // 3. Hitung tegangan asli baterai (V_in) menggunakan rumus pembagi tegangan terbalik
  float rawBatteryVoltage = pinVoltage * ((R1 + R2) / R2);

  // 4. Perkirakan persentase baterai (mapping sederhana dari 3.2V=0% ke 4.2V=100%)

  // 4. TERAPKAN FAKTOR KALIBRASI
  float calibratedBatteryVoltage = rawBatteryVoltage * FAKTOR_KALIBRASI;

  // Kita kalikan dengan 100 agar bisa menggunakan fungsi map() dengan integer
  long batteryVoltageInt = calibratedBatteryVoltage * 100;
  long percentage = map(batteryVoltageInt, 320, 420, 0, 100);

  // Pastikan persentase tidak di luar rentang 0-100
  if (percentage < 0) {
    percentage = 0;
  }
  if (percentage > 100) {
    percentage = 100;
  }

  // 5. Tampilkan semua data ke Serial Monitor
  Serial.print("Nilai ADC Mentah: ");
  Serial.print(adcValue);
  Serial.print(" \t| Tegangan di Pin: ");
  Serial.print(pinVoltage, 2); // Tampilkan 2 angka di belakang koma
  Serial.print("V \t| Tegangan Baterai Asli: ");
  Serial.print(rawBatteryVoltage, 2);
  Serial.print("V | Tegangan Terkalibrasi: ");
  Serial.print(calibratedBatteryVoltage, 2); // <-- Tampilkan hasil akhir
  Serial.print("V \t| Perkiraan Persentase: ");
  Serial.print(percentage);
  Serial.println("%");
  
  delay(2000); // Tunggu 2 detik sebelum pembacaan berikutnya
}