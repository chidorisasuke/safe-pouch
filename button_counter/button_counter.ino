#include <ezButton.h>

const int BUTTON_PIN = 15;
const int BUZZER_PIN = 18; // Pin baru untuk buzzer

ezButton button(BUTTON_PIN);
unsigned long lastCount = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Memulai program Button Counter di Pin 15...");
  Serial.println("Menunggu tekanan tombol untuk membunyikan buzzer...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  button.setDebounceTime(50);
  button.setCountMode(COUNT_FALLING);
}

void loop() {
  button.loop(); 

  unsigned long count = button.getCount();
  
  if (lastCount != count) {
    Serial.print("Jumlah Tekanan Tombol: ");
    Serial.println(count);

    if((count > 0) && (count % 2 == 0)){
      Serial.println(" -> Hitungan Genap, bunyikan buzzer!");
      digitalWrite(BUZZER_PIN, LOW);
      delay(1000);
      digitalWrite(BUZZER_PIN, HIGH);
    }
    lastCount = count; 
  }
}