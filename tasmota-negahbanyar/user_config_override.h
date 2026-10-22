/*
  user_config_override.h — پیکربندی آمادهٔ بیلد Tasmota برای:
  برد ESP-07 چهار رله (ECA-22665) + درایور نگهبان‌یار

  کاربرد: این فایل را در ریشهٔ سورس Tasmota کنار my_user_config.h کپی کنید
  (Gitpod/PlatformIO آن را خودکار می‌خورد: #ifdef USE_CONFIG_OVERRIDE در my_user_config.h)

  پیش‌تنظیم‌ها — خلاصه:
  • تمپلیت برد: RELAY1..4 ← GPIO4/5/12/13 + کلید FLASH روی GPIO0 = Button1
  • نام دستگاه/موضوع MQTT: neghbanyar
  • منطقهٔ زمانی ایران UTC+3:30 (TIME_STD/DST هر دو +210 دقیقه؛ تاسموتا نیم‌ساعت با Timezone نمی‌گیرد)
  • فایل‌سیستم LittleFS فعال (ذخیرهٔ کلید TOTP/API)
  • حذف عمدهٔ ماژول‌های بلااستفاده برای جا شدن روی فلش ۱MB (ESP-07!) با حفظ MQTT/HTTP/Rules/Timers
  • تعریف USE_XDRV_150 → کامپایل درایور نگهبان‌یار
*/

#ifndef _USER_CONFIG_OVERRIDE_H_
#define _USER_CONFIG_OVERRIDE_H_

// ─── هویت ────────────────────────────────────────────────
#define PROJECT                "neghbanyar"            // نام باینری (می‌توانید نگه ندارید؛ تعریف‌اش اختیاری است)
#define FRIENDLY_NAME          "نگهبان‌یار"             // نام نمایشی وب
#define MQTT_TOPIC             "neghbanyar"            // موضوع MQTT پیش‌فرض

// ─── تمپلیت برد: RELAY1..4 ← GPIO4/5/12/13 ، Button1 ← GPIO0 ───
#undef USER_TEMPLATE
#define USER_TEMPLATE "{\"NAME\":\"IRANIC ESP07 4Relay\",\"GPIO\":[32,1,0,1,224,225,0,0,0,0,0,0,226,227,0,0,0],\"FLAG\":0,\"BASE\":18}"
#undef FALLBACK_MODULE
#define FALLBACK_MODULE        USER_MODULE
// اگر نسخه V2 با RF433 است (گیرنده GPIO14/فرستنده GPIO16):
// از قالب RF از پوشه tasmota/ (template_esp07_4relay_rf.json) استفاده و USER_TEMPLATE بالا را با مقدار آن جایگزین کنید
// + فعال کردن خط زیر می‌خواهد RcSwitch را هم بسازد:

//#define USE_RC_SWITCH                    // (+2.7KB) برای برد V2 با گیرنده/فرستنده 433MHz

// ─── وب/شبکه ─────────────────────────────────────────────
#define WEB_SERVER             2         // ادمین
#define MDNS_ENABLED           true
#define WEB_PASSWORD           ""        // سطح امنیت: نگهبان‌یار خودش TOTP دارد؛ پسورد وب Tasmota را خالی می‌گذاریم تا flow تمیز بماند
// (در محصول نهایی می‌توانید WebPassword هم بگذارید — هر دو سیستم کنار هم کار می‌کنند)

// ─── زمان ایران (نمایش محلی/تایمرها) ─────────────────────
#define APP_TIMEZONE           99                       // از TIME_STD/DST استفاده کن
#define TIME_STD_HEMISPHERE    North
#define TIME_STD_WEEK          Last
#define TIME_STD_DAY           Sun
#define TIME_STD_MONTH         Oct
#define TIME_STD_HOUR          0
#define TIME_STD_OFFSET        +210                     // UTC+3:30
#define TIME_DST_HEMISPHERE    North
#define TIME_DST_WEEK          Last
#define TIME_DST_DAY           Sun
#define TIME_DST_MONTH         Mar
#define TIME_DST_HOUR          0
#define TIME_DST_OFFSET        +210                     // ایران DST ندارد (از ۱۴۰۱ رسمی)

// ─── فایل‌سیستم (الزامی برای ذخیرهٔ کلید TOTP و تنظیمات نگهبان‌یار) ───
#define USE_UFILESYS
// (LittleFS روی ESP8266 در این شاخه با تعریف بالا فعال می‌شود)

// ─── درایور نگهبان‌یار ────────────────────────────────────
#define USE_XDRV_150

// ─── امکانات نگه‌داشته ────────────────────────────────
#define USE_RULES                        // Rules — پایهٔ سناریوهای اپ
#define USE_TIMERS
#define USE_SUNRISE
#define USE_DOMOTICZ  false
#undef USE_DOMOTICZ
#undef USE_HOME_ASSISTANT    // اگر HA دارید این خط را کامنت کنید تا Discovery فعال بماند
#define USE_MQTT_TLS false
#undef USE_KNX

// ─── صرع برای تراکم روی فلش ۱MB ───────────────────────────
#undef USE_LIGHT
#undef USE_WS2812
#undef USE_COUNTER
#undef USE_IR_REMOTE
#undef USE_IR_HVAC
#undef USE_ZIGBEE
#undef USE_DS18x20
#undef USE_DHT
#undef USE_I2C
#undef USE_SHT
#undef USE_HTU
#undef USE_BMP
#undef USE_ENERGY_SENSOR
#undef USE_DISPLAY
#undef USE_WEBCAM
#undef USE_PROMETHEUS
#undef USE_MQTT_AWS_IOT
#undef USE_CORS
#undef USE_ESP32_WROOM32     // (این برد ESP8266 است)
// نکتهٔ OTA برای ESP-07 (1MB): فلش مستقیم سریال با tasmota.bin (build خودتان)
// یا دو مرحله: Upload tasmota-minimal → OTA tasmota.bin (دقیق در README).

#endif  // _USER_CONFIG_OVERRIDE_H_
