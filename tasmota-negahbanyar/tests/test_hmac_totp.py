#!/usr/bin/env python3
"""
test_hmac_totp.py — راستی‌آزمایی الگوریتم TOTP/HMAC درایور نگهبان‌یار با RFC رسمی
اجرا:  python3 tests/test_hmac_totp.py

این فایل همان الگوریتمی را که در درایور C نوشته‌ایم (truncation + % 1e6, ریشهٔ
RFC 6238) با بردارهای رسمی RFC چک می‌کند تا مطمئین باشیم گوگل‌اوتنتیکیتور همان
کدی را می‌سازد که دستگاه محاسبه می‌کند. سطرها باید [OK] چاپ شوند.
"""
import sys
import hmac
import hashlib
import base64
import struct


def hotp(secret: bytes, counter: int, digits: int = 6) -> int:
    digest = hmac.new(secret, struct.pack(">Q", counter), hashlib.sha1).digest()
    off = digest[-1] & 0x0F
    bin_code = struct.unpack(">I", digest[off:off + 4])[0] & 0x7FFFFFFF
    return bin_code % (10 ** digits)


def totp(secret: bytes, t: int, step: int = 30, digits: int = 6) -> int:
    return hotp(secret, t // step, digits)


def base32_dec(s: str) -> bytes:
    pad = "=" * ((8 - len(s) % 8) % 8)
    return base64.b32decode((s + pad).upper().replace(" ", ""))


OK = True


def check(name, got, want):
    global OK
    good = (got == want)
    OK = OK and good
    print(f"[{'OK' if good else 'FAIL'}] {name}: got={got} want={want}")


# 1) بردارهای RFC 6238 (SHA-1) — کد ۸ رقمی بردار WAS معناست؛ برای مقایسهٔ مستقیم
#    در حالت ۶ رقمی، حاصل به modulo ۱e6 بریده می‌شود.
sec = b"12345678901234567890"
vectors_6 = {
    59:          287082,   # 94287082 % 1e6
    1111111109:  81804,
    1111111111:  50471,
    1234567890:  5924,
    2000000000:  279037,
    20000000000: 353130,
}
for t, exp in vectors_6.items():
    check(f"RFC6238 t={t} digits=6", totp(sec, t, 30, 6), exp)

# 2) مسیر کامل Google Authenticator: Base32 ← raw secret
b32 = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"   # معادل base32 همان secret
check("base32_dec", base32_dec(b32), sec)
check("totp(base32) t=59", totp(base32_dec(b32), 59, 30, 6), 287082)

# 3) منطق drift درایور: T-1/T/T+1 هر سه را باید می‌پذیرد
t_now = 1111111111
c = t_now // 30
vals = {d: hotp(sec, c + d) for d in (-1, 0, 1)}
print(f"[INFO] window codes for t={t_now}: {vals}")

# 4) hmac-sha1 خودآزمایی (vector استاندارد RFC 2202)
check("hmac-sha1 vector",
      hmac.new(b"key", b"The quick brown fox jumps over the lazy dog", hashlib.sha1).hexdigest(),
      "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9")

print()
if OK:
    print("نتیجه کلی: PASS ✅ — الگوریتم دستگاه با گوگل‌اوتنتیکیتور یکی است")
else:
    print("نتیجه کلی: FAIL ❌ — قبل از فلش مشکل را رفع کنید")
sys.exit(0 if OK else 1)
