#!/usr/bin/env python3
"""
gen_secret.py — تولید کلید TOTP قوی برای راه‌اندازی نگهبان‌یار
اجرا:  python3 gen_secret.py [BuildingID]
خروجی: Base32 + otpauth URI آماده

secret با CSPRNG سیستم (os.urandom) ساخته می‌شود — قوی‌تر از دکمهٔ «کلید پیشنهادی» صفحه.
"""
import os
import sys
import base64
from urllib.parse import quote

bid = sys.argv[1] if len(sys.argv) > 1 else "BLD-001"

raw = os.urandom(20)                      # ۲۰ بایت استاندارد Google Authenticator
b32 = base64.b32encode(raw).decode().rstrip("=")

print("=== کلید جدید نگهبان‌یار ===")
print("Base32 (این را در /ngh/setup روی برد وارد کنید):")
print("   " + b32)
print()
print("URI برای هر ابزار ساخت QR (یا ورود دستی در Google Authenticator):")
print("   otpauth://totp/NegahbanYar:{b}?secret={s}&issuer=NegahbanYar&digits=6&period=30".format(
    b=quote(bid), s=b32))
print()
print("در Google Authenticator: دکمهٔ  +  ←  «Or enter code manually» ←")
print("   نام: NegahbanYar:{b}   کلید: {s}   نوع: Time-based".format(b=bid, s=b32))
print()
print("هشدار: همان Base32 که در گوشی ثبت می‌کنید، باید عیناً در صفحهٔ /ngh/setup ذخیره شود؛")
print("بعد از ذخیره، کلید API اپ به‌صورت خودکار از روی secret و MAC دستگاه مشتق خواهد شد.")
