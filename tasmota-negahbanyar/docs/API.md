# قرارداد API نگهبان‌یار روی برد (HTTP + HMAC-SHA1)

این API برای **اپلیکیشن نگهبان‌یار** طراحی شده تا رله‌ها را مستقیم روی شبکهٔ محلی کنترل کند — مهمان همان معماری «بدون سرور» همیشگی محصول است.

## ۱) احراز هویت اپلیکیشن

هر درخواست به `/ngh/api` باید این‌ها را داشته باشد:

| مورد | توضیح |
|---|---|
| `op` | `state` / `info` / `relay` |
| `i` | شماره رله (۱..۴) — صرفاً برای `op=relay`؛ برای state/info با مقدار `0` فرمول را پر کنید |
| `act` | `on` / `off` / `toggle` — صرفاً برای `op=relay`؛ برای state/info با مقدار `x` |
| `ts` | Unix epoch فعلی گوشی (ثانیه) — پنجرهٔ پذیرش **±۱۲۰ ثانیه** نسبت به ساعت برد |
| هدر **`X-NGH-Key`** | امضا: `hex( HMAC_SHA1(key=api_key, msg=base) )` |

**Base-string** (دقیقاً به همین ترتیب و قالب):
```
op=<op>&i=<i>&act=<act>&ts=<ts>
```
> ⚠ ترتیب پارامترها و مقدار خام آن‌ها (بدون trim/URL-encode/تغییر حروف) باید دقیقاً همین باشد؛ همان base-string را از سمت برد هم ساخته و با امضای دریافتی مقایسه می‌کنیم.

`api_key`: ۲۰ بایت (۴۰ کاراکتر HEX) که در `/ngh/config` دیده می‌شود و دقیقاً برای APP طراحی شده — متفاوت از کلید TOTP گوگل‌اوتنتیکیتور است و با دکمهٔ «چرخش کلید API» مستقل چرخانده می‌شود (TOTP دست‌نخورده می‌ماند).

### نمونهٔ درخواست صحیح (bash+python)
```bash
KEY="(۴۰ کاراکتر HEX api_key از صفحهٔ /ngh/config)"
TS=$(date +%s)
BASE="op=relay&i=1&act=toggle&ts=$TS"
SIG=$(python3 -c "import hmac,hashlib;print(hmac.new(bytes.fromhex('$KEY'),b'$BASE',hashlib.sha1).hexdigest())")
curl -s "http://192.168.1.44/ngh/api?$BASE" -H "X-NGH-Key: $SIG"
```
پاسخ موفق:
```json
{"ok":1,"power":[1,0,0,0]}
```

### خطاها
| کد | بدنه | معنی |
|---|---|---|
| 401 | `{"ok":0,"err":"auth"}` | امضا نامعتبر / `ts` خارج پنجره / کوکی نامعتبر / کلید تنظیم نشده |
| 400 | `{"ok":0,"err":"bad op"}` | مقدار op ناشناخته |

### نقاط ورود اپلیکیشن state/info
```json
{
  "ok": 1,
  "site": "مجتمع شماره یک",
  "bid": "BLD-001",
  "power": [1,0,0,0],
  "time": 1761700000,
  "names": ["چراغ ورودی","پمپ/شیربرقی","درب پارکینگ","آژիր"]
}
```
اپ پیشنهادی: در هر شروع، یک‌بار `op=state` را بخواند تا ۱) نام‌ها/وضعیت‌ها سنکرون شود ۲) اختلاف ساعت گوشی با فیلد `time` برد اندازه گرفته شود (اگر بیشتر از ~۹۰ ثانیه بود، در UI “زمان گوشی نادرست است” اعلام کنید).

## ۲) نقاط ورود کامل صفحات
| مسیر | متد | حفاظت | توضیح |
|---|---|---|---|
| `/ngh` | GET | کوکی نشست TOTP | داشبورد ادمین |
| `/ngh/setup` | GET/POST | فقط تا وقتی که کلید TOTP تنظیم نشده | ثبت Base32 + نمایش QA آفلاین |
| `/ngh/login` | GET/POST | نرخ‌محدود (۵ خطا ≠ قفل ۱۸₀ ثانیه) | ورود با کد ۶ رقمی |
| `/ngh/config` | GET/POST | کوکی نشست TOTP | تنظیمات سامانه + چرحان api_key |
| `/ngh/api` | GET/POST | HMAC اپ یا کوکی TOTP | API اپ و وب |
| `/ngh/qr.js` | GET | آزاد | کتابخانه QR آفلاین (cached) |
| `/ngh/logout` | GET | کوکی | خروج |

> وب‌ادمین می‌تواند با همان API کار کند؛ کافی کوکی نشست معتبر باشد (بدون ts/sig). اپ به‌جای آن HMAC می‌فرستد.

## ۳) شبه‌کد آمادهٔ اپ (Kotlin)
```kotlin
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

private fun hex(a: ByteArray) = a.joinToString("") { "%02x".format(it) }
private fun hmac(key: ByteArray, msg: String) = hex(
    Mac.getInstance("HmacSHA1").apply { init(SecretKeySpec(key, "HmacSHA1")) }
        .doFinal(msg.toByteArray()))

fun toggleRelay(ip: String, apiKey: ByteArray, i: Int) {
    val ts   = System.currentTimeMillis() / 1000
    val base = "op=relay&i=$i&act=toggle&ts=$ts"
    http.get("http://$ip/ngh/api?$base") {
        header("X-NGH-Key", hmac(apiKey, base))
    }   // ← {"ok":1,"power":[...]}
}
fun readState(ip: String, apiKey: ByteArray) {
    val ts   = System.currentTimeMillis() / 1000
    val base = "op=state&i=0&act=x&ts=$ts"
    http.get("http://$ip/ngh/api?$base") {
        header("X-NGH-Key", hmac(apiKey, base))
    }
}
```

## ۴) امنیت — چرا این الگو برای شما مناسب است
- مشابه امضای HMAC پلاک‌های موجود در نگهبان‌یار — بدون سرور، بدون PKI و بدون هزینهٔ عملیات.
- timestamp با پنجرهٔ دقیق: پکت دیده‌شده حداکثر ۲ دقیقه جلودار است.
- روی شبکهٔ WiFi محلی کار می‌کند؛ برای دسترسی از بیرون هرگز پورت را روی روتر فوروارد نکنید — Wireguard/Tailscale روی گوشی مدیر راه امن است.
- دسترسی منقضی‌دان‌ها: چرخش `api_key` از /ngh/config همهٔ گوشی‌های قبلی را فوراً می‌کشد.
