#!/usr/bin/env bash
# apply_theme.sh — بازنقش‌سازی ظاهر صفحات مركزی Tasmota به تم نگهبان‌یار (دارک-آبی-سبز)
# نحوهٔ اجرا (از ریشهٔ سورس Tasmota):
#    bash tasmota-negahbanyar/tools/apply_theme.sh
# در صورت نسخه متفاوت، فقط هشدار می‌دهد و فایل را خراب نمی‌کند.
set -e
FILE="tasmota/tasmota_xdrv_driver/xdrv_01_9_webserver.ino"
[ -f "$FILE" ] || { echo "فایل $FILE پیدا نشد؛ از ریشهٔ Tasmota اجرا کنید."; exit 1; }

BAK="$FILE.orig_$(date +%Y%m%d_%H%M%S)"
cp "$FILE" "$BAK"
CHANGED=0

safe_sed () { # $1=desc $2=pattern $3=replacement
  if grep -q "$2" "$FILE"; then
    sed -i "s/$2/$3/g" "$FILE" && echo "✓ $1" || echo "✗ $1 (نوشته نشد)"
    CHANGED=$((CHANGED+1))
  else
    echo "⚠ الگوی '$1' یافت نشد (نسخه متفاوت؟) — نادیده گرفته شد"
  fi
}

# پس‌زمینی صفحه
safe_sed "بدنه تیره" 'body{background:[^;]*' 'body{background:#0b1420;color:#e7eef7'
# رنگ متن
safe_sed "متن روشن" 'color:#[0-9a-fA-F]\{6\}' 'color:#e7eef7'
# سربرگ (ha or header bar)
safe_sed "سربرگ آبی-سبز" '#header{[^}]*}' '#header{background:linear-gradient(135deg,#12345e,#16233a);color:#fff}'
# دکمه‌ها
safe_sed "رنگ دکمه" 'background:#1fa3ec' 'background:linear-gradient(135deg,#2fd6a8,#1d6fd8)'
safe_sed "هاور دکمه" 'background:#0e70a4' 'background:#175a66'
# فرم‌ها و تشابه‌ها
safe_sed "کادر فرم" 'background:#f2f2f2' 'background:rgba(22,35,58,.72);border-radius:14px;border:1px solid #26375a'
safe_sed "پس‌زمینی خانه‌ها" 'background:#fff' 'background:#0e1a2c'
safe_sed "رنگ لینک" 'a{color:[^;]*' 'a{color:#7dd6a8'

if [ $CHANGED -gt 0 ]; then
  echo ""
  echo "تم در $CHANGED نکته اعمال شد. پشتیبان: $BAK"
  echo "با حذف یا ریستور فایل پشتیبان می‌توانید برگردانید."
else
  echo "هیچ تطابقی پیدا نشد. شاید در این نسخه رنگ‌ها در فایل دیگری باشند (جست‌وجو: grep -rn 'background:#[0-9a-f]')"
fi
