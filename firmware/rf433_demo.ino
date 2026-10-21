/*
 * rf433_demo.ino — دموی کامل برد V2 : کنترل ۴ رله از وب + دریافت/ارسال RF433
 *
 * سخت‌افزار (مطابق schematic_v2.svg — نگاشت رله‌ها از سیلک برد سازنده):
 *   RL1..RL4  <- GPIO4, GPIO5, GPIO12, GPIO13  (فعال HIGH)
 *   گیرنده RF433 DATA -> GPIO14 (از طریق تقسیم 10k/18k — تا ~5V)
 *   فرستنده RF433 DATA <- GPIO16 (از طریق شیفتر سطح 2N7002 — بدون وارونگی)
 *
 * کتابخانه‌ها: rc-switch (نسخه پشتیبان ESP8266) — از Library Manager نصب کنید.
 *
 * وب‌سرور روی پورت 80:
 *   /                      صفحه وضعیت + دکمه‌ها
 *   /relay?ch=1..4&set=on|off|toggle
 *   /rf?code=12345&bits=24 ارسال کد RF از فرستنده
 * ریموت 433MHz: هر کد دریافتی در سریال لاگ می‌شود و اگر کد ریموت را به
 *   آرایه LEARNED اضافه کنید، کانال متناظر toggle می‌شود.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RCSwitch.h>

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// ---- نگاشت پین‌ها (تأییدشده از سیلک برد: RELAY1..4 = D04/D05/D12/D13) ----
const uint8_t RELAY_PINS[4] = {4, 5, 12, 13};
const uint8_t RF_RX_PIN = 14;  // GPIO14 — تنها پایه آزاد با وقفه خارجی
const uint8_t RF_TX_PIN = 16;  // GPIO16 — خروجی ساده (وقفه لازم ندارد)

RCSwitch rf = RCSwitch();
ESP8266WebServer server(80);
bool relayState[4] = {false, false, false, false};

// کدهای ریموت آموخته‌شده → شماره کانال (۱..۴)؛ مقادیر نمونه را عوض کنید
struct Learned { unsigned long code; uint8_t channel; };
Learned LEARNED[] = {
  // {5510456, 1}, {5510457, 2}, {5510458, 3}, {5510459, 4},
};

void setRelay(uint8_t ch, bool on) {
  relayState[ch - 1] = on;
  digitalWrite(RELAY_PINS[ch - 1], on ? HIGH : LOW);
}

void handleRoot() {
  String h = F("<!doctype html><html dir='rtl'><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width'>"
               "<style>body{font-family:tahoma;max-width:420px;margin:auto}"
               ".b{font-size:22px;padding:14px;margin:6px;width:45%;border-radius:10px;border:1px solid #888}"
               ".on{background:#1e9c4a;color:#fff}.off{background:#eee}</style>"
               "<h3>کنترل ۴ رله — ESP-07 + RF433</h3>");
  for (uint8_t i = 1; i <= 4; i++) {
    h += "<div>کانال " + String(i) + ": ";
    h += "<a href='/relay?ch=" + String(i) + "&set=toggle'>"
         "<button class='b " + String(relayState[i-1] ? "on" : "off") + "'>"
         + (relayState[i-1] ? "روشن" : "خاموش") + "</button></a></div>";
  }
  h += F("<p><a href='/rf?code=5526868&bits=24'>ارسال کد RF نمونه (5526868/24)</a></p>");
  server.send(200, "text/html", h);
}

void handleRelay() {
  int ch = server.arg("ch").toInt();
  String set = server.arg("set");
  if (ch >= 1 && ch <= 4) {
    if (set == "on") setRelay(ch, true);
    else if (set == "off") setRelay(ch, false);
    else setRelay(ch, !relayState[ch - 1]);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRfSend() {
  unsigned long code = server.arg("code").toInt();
  int bits = server.arg("bits").toInt();
  if (bits <= 0) bits = 24;
  rf.send(code, bits);
  server.send(200, "text/plain", "RF sent: " + String(code) + " / " + String(bits) + "bit");
}

void setup() {
  // مهم: قبل از هر چیز خروجی‌های رله را LOW کن (پالس‌های بوت GPIO12..14)
  for (uint8_t i = 0; i < 4; i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print('.'); }
  Serial.println(); Serial.println(WiFi.localIP());

  rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN)); // GPIO4 پشتیبانی وقفه دارد
  rf.enableTransmit(RF_TX_PIN);
  rf.setProtocol(1); rf.setPulseLength(350);        // متداول‌ترین ریموت‌های 433 ؛ در صورت نیاز تغییر بده

  server.on("/", handleRoot);
  server.on("/relay", handleRelay);
  server.on("/rf", handleRfSend);
  server.begin();
  Serial.println(F("Web server + RF ready"));
}

void loop() {
  server.handleClient();
  MDNS.update();

  if (rf.available()) {
    unsigned long code = rf.getReceivedValue();
    Serial.printf("RF RX: %lu / %d bit, protocol %d\n",
                  code, rf.getReceivedBitlength(), rf.getReceivedProtocol());
    for (auto &L : LEARNED) if (L.code == code) {
      setRelay(L.channel, !relayState[L.channel - 1]);
      Serial.printf("Remote matched -> toggle channel %d\n", L.channel);
    }
    rf.resetAvailable();
  }
}
