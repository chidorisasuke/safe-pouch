// ==========================================================
// ==  SKETSA DEBUGGING TOMBOL MULTIFUNGSI (VERSI PERBAIKAN) ==
// ==========================================================

const int BUTTON_PIN = 4;
const int BUZZER_PIN = 18;

// --- Variabel untuk logika tombol ---
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long LONG_PRESS_TIME = 2000;
const unsigned long DOUBLE_PRESS_WINDOW = 400;

int buttonState;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonDownTime = 0;
bool longPressTriggered = false; // Flag untuk memastikan long press hanya terpicu sekali
unsigned long lastPressTime = 0;
int pressCount = 0;

// Variabel untuk status alarm
bool alarmOn = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("Mulai Debugging Tombol (Versi Perbaikan)...");
}

void loop() {
  handleButton(); // Panggil fungsi untuk memproses logika tombol
  handleAlarm();  // Panggil fungsi untuk mengontrol buzzer
}

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  lastButtonState = reading;

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // Tombol BARU saja ditekan
        buttonDownTime = millis();
        longPressTriggered = false; // Reset flag long press
      } else { // Tombol BARU saja dilepas
        if (!longPressTriggered) { // Hanya proses jika bukan hasil dari long press
           pressCount++;
           lastPressTime = millis();
        }
      }
    }
  }

  // Cek Long Press (harus dilakukan saat tombol masih ditekan)
  if (buttonState == LOW && !longPressTriggered && (millis() - buttonDownTime > LONG_PRESS_TIME)) {
    Serial.println("AKSI: Tekan Tahan -> Memicu Alarm Panik!");
    alarmOn = true;
    longPressTriggered = true; // Set flag agar aksi ini tidak terulang
  }

  // Cek Double Press (diproses setelah tombol dilepas)
  if (pressCount > 0 && (millis() - lastPressTime > DOUBLE_PRESS_WINDOW)) {
    if (pressCount == 1) { // Aksi untuk tekan singkat
      if (alarmOn) {
        Serial.println("AKSI: Tekan Singkat -> Alarm Dibatalkan!");
        alarmOn = false;
      }
    } else if (pressCount == 2) { // Aksi untuk tekan dua kali
      Serial.println("AKSI: Tekan Dua Kali -> Memicu Cek Baterai!");
      // Di sini Anda bisa memanggil fungsi cek baterai
    }
    pressCount = 0; // Reset hitungan
  }
}

void handleAlarm() {
  if (alarmOn) {
    // Buat buzzer berkedip
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    // Pastikan buzzer mati jika tidak ada alarm
    digitalWrite(BUZZER_PIN, HIGH);
  }
}