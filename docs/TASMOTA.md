# Tasmota — چیست و چه چیزهایی‌اش برای این بردها به درد می‌خورد

## ۱) خلاصه
[Tasmota](https://github.com/arendst/Tasmota) فریمور آزاد و محلی برای دستگاه‌های ESP8266/ESP32 است: کنترل کامل از طریق WebUI، MQTT، HTTP، Serial و KNX با تایمر، قوانین (Rules)، و چندصد درایور سنسور — بدون نیاز به فلش کردن کد جدید برای منطق ساده.
- نسخه ESP8266: `tasmota.bin` / نسخه ESP32: `tasmota32.bin` (نصب با [Web Installer](https://tasmota.github.io/install/) در ۲ دقیقه، عرای فریمور اولیه TS-...)
- در زمان این سند (Oct 2026): ریپو فعال، آخرین کامیت 8 ساعت قبل، 25100+ PR.

## ۲) چه چیزهایی برای بردهای ما «مستقیم به درد می‌خورد»

| ویژگی | کاربرد روی بردهای ایرانیک |
|---|---|
| **Templates** (نگاشت GPIO بی‌نیاز از کامپایل) | رله‌ها GPIO4/5/12/13 (ESP-07) و 4/2/13/12 (ESP32) با تمپلیت آماده این ریپو تنظیم می‌شوند |
| Relay/Switch/Button کامپوننت | ۴ رله + کلید FLASH = Button1 (جفت فیزیکی و نرم‌افزاری) |
| **RFSend / RFrecv (کامپوننت 1120/1152)** | RC-Switch برای گیرنده/فرستنده 433MHz در نسخه V2 برد ESP-07 (RX→GPIO14 ، TX→GPIO16) |
| موتور Rules | مثلاً «کد RF دریافت شد → رله X تغییر وضعیت» بدون یک خط کد |
| تایمر (۱۶ عدد + طلوع/غروب خورشید) | زمان‌بندی کانال‌ها |
| Interlock (SO68/SO112) | قفل متقابل رله‌ها (محرک‌های جرثقیل/شیرهای دولته) |
| PulseTime | پالس برای قفل برقی/موتور |
| Home Assistant Discovery (SO19) | ظهور خودکار ۴ کانال در HA |
| Alexa Hue/WeMo emulation | کنترل صوتی بدون ساخت |
| MQTT کامل | `cmnd/<topic>/POWER1..4` |
| HTTP API | `http://<ip>/cm?cmnd=Power1%20TOGGLE` |
| Power-on state (SO…) | حالت رله‌ها بعد از برق‌رفتگی |
| OTA + safeboot | آپدیت بی‌سیم و رکاوری |
| Berry script (فقط ESP32) | زبان اسکریپتی برای منطق پیچیده روی IX4 |
| Matter (ESP32, Fork/توسعه) | تبدیل IX4 به vsat Matter |

## ۳) نقاط توجه مخصوص بردهای ایرانیک
- **ESP-07 فقط ۱MB فلش دارد:** tasmota استاندارد (~750KB) با OTA دوست ندارد → همیشه ابتدا `tasmota-minimal.bin` (~500KB) فلش/OTA کنید و بعد نسخه نهایی را آپلود کنید؛ یا فلش مستقیم سریال با tasmota کامل.
- **rc-switch (433MHz) در بیلد پیش‌فرض tasmota فعال نیست:** در `my_user_config.h` به صورت `//#define USE_RC_SWITCH` کامنت است (+2.7KB کد، 460B IRAM). برای برد V2 با دستور‌های Gitpod/PlatformIO سریع خودتان بیلد کنید و `#define USE_RC_SWITCH` را فعال کنید؛ بعد در Configure Module پایه‌ها را RFSend/RFrecv کنید. کدهای دریافتی روی `tele/<topic>/RESULT` می‌آیند (`{"RfReceived":{"Data":"0x…","Bits":24,…}}`).
- **GPIO2 و GPIO12 روی IX4 رله هستند و پایه استرپینگ بوت ESP32اند:** در عمل سالم کار می‌کند (درایو رله LOW/NPN) اما در Rules/boot رفتار پایه را در نظر بگیرید (Pull-up داخلی GPIO0 مانع نمی‌شود).
- بازخورد LED: تاسموتا به صورت پیش‌فرض LED‌های رله را هم مدیریت می‌کند اگر کامپوننت Led_i جداگانه روی همان رله‌ها تعریف کنید؛ بهتر است LEDها موازی روی خط رله باشند و در تمپلیت LED تعریف نکنید.

## ۴) تمپلیت‌های آماده (در پوشه `tasmota/`)

- `template_esp07_4relay.json` — برد ESP-07 چهار رله (R1..4 ← GPIO4/5/12/13 ، کلید FLASH روی GPIO0)
- `template_esp07_4relay_rf.json` — همان + RFFrec on GPIO14 + RFSend on GPIO16 (پس از بیلد با USE_RC_SWITCH)
- `template_esp32_ix4.json` — برد Esp32 IX4 (R1..4 ← GPIO4/2/13/12 ، کلید FLASH روی GPIO0)

نصب: Menu → Configuration → Configure Other → جعبه Template ← Active ✓ → ذخیره (ری‌بوت خودکار).

## ۵) دستورهای آماده جالب برای این بردها

```
# قفل‌متقابل ۴ کانال (جفت-به-جفت)
SetOption112 1

# پالس ۲ ثانیه روی رله ۱ (قفل برقی)
PulseTime1 20

# قانون: دریافت کد RF 0x7028D5 → رله ۱ Toggle
Rule1 ON RfReceived#Data=0x7028D5 DO Power1 TOGGLE ENDON
Rule1 1

# وضعیت پس از برگشت برق: همه خاموش
PowerOnState 0

# نام دوستانه برای Alexa/HA
FriendlyName1 Living Room Light
```

## ۶) تجربه عملی پیشنهادی برای شروع
1. از [https://tasmota.github.io/install/](https://tasmota.github.io/install/) با اتصال USB، `tasmota32.bin` برای IX4 و `tasmota.bin` برای ESP-07 نصب کنید.
2. بعد از وارد کردن WiFi، تمپلیت برد خود را از پوشه `tasmota/` روی برد اعمال کنید.
3. با WebUI رله‌ها را کنتrol و از منوی Timers زمان‌بندی کنید؛ با Rules رفتار RF/سنسور اضافه کنید.
4. برای RF در برد ESP-07 V2، بیلد سفارشی با USE_RC_SWITCH بسازید (Gitpod رایگان).
