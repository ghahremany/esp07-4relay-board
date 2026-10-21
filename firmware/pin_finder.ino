/*
 * pin_finder.ino — ابزار پیداکردن GPIO متصل به هر رله
 * برد: ۴ رله ESP8266 (ESP-07) — ECA 22665 / شماتیک بازسازی V1
 *
 * روش کار:
 *  ۱) این اسکچ را با هسته ESP8266 در Arduino IDE پروگرام کنید (برد Generic ESP8266، Flash 1MB).
 *  ۲) Serial Monitor با baud = 115200 باز کنید.
 *  ۳) روی هر GPIO کاندید، ۵ پالس ۱.۵ ثانیه‌ای اعمال می‌شود؛ سریال شماره GPIO را اعلام می‌کند.
 *  ۴) صدای «تک» رله یا روشن‌شدن LED کانال = همان نگاشت واقعی.
 *
 * نگاشت رسمی برد از سیلک (باید همین را ببینید): RL1←GPIO4 ، RL2←GPIO5 ، RL3←GPIO12 ، RL4←GPIO13
 * توجه: GPIO15 روی برد پول‌داون سخت‌افزاری دارد و GPIO0/GPIO2 در بوت نقش دارند.
 */

const uint8_t candidates[] = {4, 5, 12, 13, 14, 16, 0, 2, 15};
const uint8_t COUNT = sizeof(candidates) / sizeof(candidates[0]);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("=== Relay Pin Finder ==="));
  Serial.println(F("Watch the relays; note which GPIO toggles which channel."));
  delay(3000);
}

void loop() {
  for (uint8_t i = 0; i < COUNT; i++) {
    uint8_t pin = candidates[i];
    Serial.printf("\n>>> Testing GPIO%d for 8 seconds ...\n", pin);
    pinMode(pin, OUTPUT);
    for (uint8_t k = 0; k < 5; k++) {
      Serial.printf("    GPIO%d -> HIGH\n", pin);
      digitalWrite(pin, HIGH);
      delay(1500);
      Serial.printf("    GPIO%d -> LOW\n", pin);
      digitalWrite(pin, LOW);
      delay(1500);
    }
    pinMode(pin, INPUT);
  }
  Serial.println(F("\nAll candidates done. Restarting scan in 20 s (press RST to rerun now)."));
  delay(20000);
}
