/*
  xdrv_150_neghbanyar.ino — نگهبان‌یار برای Tasmota (برد ESP-07 چهار رله)
  ------------------------------------------------------------------
  | صفحهٔ مدیریت مدرن فارسی + ورود با Google Authenticator (TOTP RFC 6238)
  | + API امضاشده با HMAC برای کنترل رله‌ها از اپلیکیشن «نگهبان‌یار»
  |
  | نصب:
  |   1) این فایل را در tasmota/tasmota_xdrv_driver/ کپی کنید
  |   2) فایل qrjs_ngh.h (پوشه addons همین ریپو) را کنار آن کپی کنید
  |   3) در user_config_override.h مقدار USE_XDRV_150 را فعال کنید
  |   4) بیلد Gitpod/PlatformIO → فلش — راهنمای کامل در README.md
  |
  | تست‌شده برای درخت development اکتبر ۲۰۲۶ —
  | در صورت تغییر API نسخه‌های دیگر، بخش «SELF-ADAPTIVE NOTES» ته فایل را ببینید.
*/

#ifdef USE_XDRV_150
#define XDRV_150 150

#include "qrjs_ngh.h"

/* ===================== تنظیمات قابل‌تغییر ===================== */
#define NGH_CFG_FILE   "/ngh.cfg"       // فایل دائمی ساختار NGH_CFG در LittleFS
#define NGH_COOKIE     "NGH"            // نام کوکی نشست
#define NGH_SES_TTL    600              // عمر نشست ادمین (ثانیه)
#define NGH_MAX_FAIL   5                // تعداد خطای TOTP قبل از قفل
#define NGH_LOCK_SEC   180              // مدت قفل (ثانیه)
//#define NGH_DEBUG_SERIAL              // فقط روی میز کار: چاپ TOTP جاری در سریال ⚠ هرگز در تولید فعال نکنید
//#define NGH_DEBUG_TOKEN

static const uint8_t NGH_RELAY_GPIO[4] = {4,5,12,13};  // نگاشت تأییدشده برد ESP-07

/* ===================== SHA-1 خودخوان (بدون وابستگی) ===================== */
struct sha1_ctx { uint32_t h[5]; uint64_t len; uint8_t blk[64]; uint8_t blen; };
static inline uint32_t rol32(uint32_t v, uint8_t n){ return (v<<n)|(v>>(32-n)); }
static void sha1_process(sha1_ctx* c, const uint8_t* p){
  uint32_t w[80];
  for (int i=0;i<16;i++)
    w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
  for (int i=16;i<80;i++) w[i]=rol32(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
  uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4];
  for (int i=0;i<80;i++){
    uint32_t f,k;
    if (i<20){ f=(b&cc)|((~b)&d); k=0x5A827999; }
    else if (i<40){ f=b^cc^d; k=0x6ED9EBA1; }
    else if (i<60){ f=(b&cc)|(b&d)|(cc&d); k=0x8F1BBCDC; }
    else { f=b^cc^d; k=0xCA62C1D6; }
    uint32_t t=rol32(a,5)+f+e+k+w[i]; e=d; d=cc; cc=rol32(b,30); b=a; a=t;
  }
  c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e;
}
static void sha1_init(sha1_ctx* c){
  c->h[0]=0x67452301; c->h[1]=0xEFCDAB89; c->h[2]=0x98BADCFE;
  c->h[3]=0x10325476; c->h[4]=0xC3D2E1F0; c->len=0; c->blen=0;
}
static void sha1_update(sha1_ctx* c, const uint8_t* data, uint32_t len){
  c->len+=len;
  while (len){
    uint32_t n=64-c->blen; if (n>len) n=len;
    memcpy(c->blk+c->blen,data,n); c->blen+=n; data+=n; len-=n;
    if (c->blen==64){ sha1_process(c,c->blk); c->blen=0; }
  }
}
static void sha1_final(sha1_ctx* c, uint8_t out[20]){
  uint64_t bits=c->len*8;
  uint8_t pad=0x80; sha1_update(c,&pad,1);
  while (c->blen!=56){ pad=0; sha1_update(c,&pad,1); }
  uint8_t be[8]; for (int i=0;i<8;i++) be[7-i]=(bits>>(8*i))&0xff;
  sha1_update(c,be,8);
  for (int i=0;i<5;i++){
    out[i*4]=(c->h[i]>>24)&0xff; out[i*4+1]=(c->h[i]>>16)&0xff;
    out[i*4+2]=(c->h[i]>>8)&0xff; out[i*4+3]=c->h[i]&0xff;
  }
}
static void hmac_sha1(const uint8_t* key,uint8_t klen,const uint8_t* msg,uint32_t mlen,uint8_t out[20]){
  uint8_t k0[64]; memset(k0,0,64);
  if (klen>64){ sha1_ctx t; sha1_init(&t); sha1_update(&t,key,klen); sha1_final(&t,k0); }
  else memcpy(k0,key,klen);
  uint8_t ipad[64],opad[64];
  uint8_t kl=(klen>64)?20:klen;
  for (int i=0;i<64;i++){ ipad[i]=k0[i]^0x36; opad[i]=k0[i]^0x5c; }
  (void)kl;
  sha1_ctx t; uint8_t inner[20];
  sha1_init(&t); sha1_update(&t,ipad,64); sha1_update(&t,msg,mlen); sha1_final(&t,inner);
  sha1_init(&t); sha1_update(&t,opad,64); sha1_update(&t,inner,20); sha1_final(&t,out);
}

/* ===================== Base32 ===================== */
static const char* B32="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static uint8_t b32v(char c){
  if (c>='a'&&c<='z') c-=32;
  if (c>='A'&&c<='Z') return c-'A';
  if (c>='2'&&c<='7') return c-'2'+26;
  return 255;
}
static uint8_t base32_dec(const char* in,uint8_t* out,uint8_t max){
  uint16_t buf=0; int bits=0; uint8_t n=0;
  for (const char* p=in;*p;p++){
    char c=*p;
    if (c=='='||c==' '||c=='-'||c=='\n'||c=='\r') continue;
    uint8_t v=b32v(c); if (v==255) return 0;
    buf=(uint16_t)((buf<<5)|v); bits+=5;
    if (bits>=8){ out[n++]=(buf>>(bits-8))&0xff; if (n>=max) return n; bits-=8; }
  }
  return n;
}
static void base32_enc(const uint8_t* in,uint8_t len,char* out){
  uint32_t buf=0; int bits=0; char* o=out;
  for (uint8_t i=0;i<len;i++){
    buf=(buf<<8)|in[i]; bits+=8;
    while (bits>=5){ *o++=B32[(buf>>(bits-5))&31]; bits-=5; }
  }
  if (bits>0) *o++=B32[(buf<<(5-bits))&31];
  *o='\0';
}

/* ===================== ابزار امنیتی ===================== */
static bool ctime_eq(const uint8_t* a,const uint8_t* b,uint8_t n){
  uint8_t v=0; for (uint8_t i=0;i<n;i++) v|=a[i]^b[i]; return v==0;
}
static void to_hex(const uint8_t* in,uint8_t len,char* out){
  static const char* H="0123456789abcdef";
  for (uint8_t i=0;i<len;i++){ out[i*2]=H[in[i]>>4]; out[i*2+1]=H[in[i]&15]; }
  out[len*2]='\0';
}
static uint8_t hex_dec(const char* in,uint8_t* out,uint8_t max){
  uint8_t n=0;
  auto hv=[](char c)->int8_t{
    if (c>='0'&&c<='9') return (int8_t)(c-'0');
    if (c>='a'&&c<='f') return (int8_t)(c-'a'+10);
    if (c>='A'&&c<='F') return (int8_t)(c-'A'+10);
    return (int8_t)-1; };
  while (in[0]&&in[1]){
    int8_t a=hv(in[0]),b=hv(in[1]);
    if (a<0||b<0) return 0;
    out[n++]=(uint8_t)((a<<4)|b); if (n>=max) return n; in+=2;
  }
  return n;
}

/* ===================== TOTP ===================== */
static uint32_t ngh_utc(){ return RtcTime.utc_time; }
static bool ngh_time_ok(){ return RtcTime.year>=100; } // 2000+
static bool totp_code(const uint8_t* sec,uint8_t slen,uint32_t counter,uint32_t* code){
  if (!slen) return false;
  uint8_t msg[8]; for (int i=0;i<8;i++) msg[7-i]=(counter>>(8*i))&0xff;
  uint8_t mac[20]; hmac_sha1(sec,slen,msg,8,mac);
  uint8_t off=mac[19]&0x0f;
  uint32_t bin=((uint32_t)(mac[off]&0x7f)<<24)|((uint32_t)mac[off+1]<<16)|((uint32_t)mac[off+2]<<8)|(uint32_t)mac[off+3];
  *code=bin%1000000UL;
  return true;
}
static bool totp_check(const uint8_t* sec,uint8_t slen,uint32_t now,const char* code){
  if (!code||strlen(code)!=6) return false;
  uint32_t want=strtoul(code,nullptr,10);
  uint32_t c=now/30;
  for (int8_t drift=-1;drift<=1;drift++){
    uint32_t got;
    if (totp_code(sec,slen,(uint32_t)((int64_t)c+drift),&got) && got==want) return true;
  }
  return false;
}

/* ===================== تنظیمات دائمی ===================== */
struct NGH_CFG {
  uint32_t magic;            // "NGH!"
  uint8_t  version;          // 1
  uint8_t  secret_len;       // 0..24
  uint8_t  secret_raw[24];
  uint8_t  api_key[20];
  char     site[20];
  char     bid[10];
  char     rname[4][14];
  char     webhook[60];
  uint8_t  interlock;
} Ngh;
#define NGH_MAGIC 0x4E474821UL

static void ngh_defaults(){
  memset(&Ngh,0,sizeof(Ngh));
  Ngh.magic=NGH_MAGIC; Ngh.version=1;
  strncpy_P(Ngh.site,PSTR("مجتمع شماره یک"),sizeof(Ngh.site)-1);
  strncpy_P(Ngh.bid,PSTR("BLD-001"),sizeof(Ngh.bid)-1);
  strncpy_P(Ngh.rname[0],PSTR("چراغ ورودی"),sizeof(Ngh.rname[0])-1);
  strncpy_P(Ngh.rname[1],PSTR("پمپ/شیربرقی"),sizeof(Ngh.rname[1])-1);
  strncpy_P(Ngh.rname[2],PSTR("درب پارکینگ"),sizeof(Ngh.rname[2])-1);
  strncpy_P(Ngh.rname[3],PSTR("آژیر"),sizeof(Ngh.rname[3])-1);
}
static bool ngh_save(){
#ifdef USE_UFILESYS
  return TfsSaveFile(NGH_CFG_FILE,(char*)&Ngh,sizeof(NGH_CFG));
#else
  return false;
#endif
}
static bool ngh_load(){
#ifdef USE_UFILESYS
  char buf[sizeof(NGH_CFG)];
  if (TfsLoadFile(NGH_CFG_FILE,buf,sizeof(buf))>=4 && ((uint32_t*)buf)[0]==NGH_MAGIC){
    memcpy(&Ngh,buf,sizeof(NGH_CFG));
    if (Ngh.version>1) ngh_defaults();
    return true;
  }
#endif
  return false;
}

/* ===================== نشست ادمین ===================== */
static void ngh_make_token(uint32_t exp,char* out){ // "exp_hex~mac_hex(12)"
  char body[10]; snprintf_P(body,sizeof(body),PSTR("%08lx"),(unsigned long)exp);
  uint8_t mac[20]; hmac_sha1(Ngh.secret_raw,Ngh.secret_len,(const uint8_t*)body,8,mac);
  char mh[13]; to_hex(mac,6,mh);
  snprintf(out,32,"%s~%s",body,mh);
}
static bool ngh_token_ok(const char* tok){
  if (!tok||Ngh.secret_len==0) return false;
  uint32_t exp=strtoul(tok,nullptr,16);
  if (!ngh_time_ok()||exp<ngh_utc()) return false;
  const char* p=strchr(tok,'~'); if (!p) return false;
  uint8_t got[20]; if (hex_dec(p+1,got,20)!=6) return false;
  char body[10]; snprintf_P(body,sizeof(body),PSTR("%08lx"),(unsigned long)exp);
  uint8_t mac[20]; hmac_sha1(Ngh.secret_raw,Ngh.secret_len,(const uint8_t*)body,8,mac);
  return ctime_eq(got,mac,6);
}
static bool ngh_session(){
  if (!Webserver->hasHeader("Cookie")) return false;
  String ck=Webserver->header("Cookie");
  int a=ck.indexOf(NGH_COOKIE "=");
  if (a<0) return false;
  a+=4;
  int b=ck.indexOf(';',a); if (b<0) b=ck.length();
  return ngh_token_ok(ck.substring(a,b).c_str());
}

static uint8_t ngh_fail_cnt=0; static uint32_t ngh_locked_until=0;
static bool ngh_locked(){ return ngh_locked_until && millis()<ngh_locked_until; }
static void ngh_reg_fail(){
  if (++ngh_fail_cnt>=NGH_MAX_FAIL){
    ngh_locked_until=millis()+NGH_LOCK_SEC*1000UL; ngh_fail_cnt=0;
    AddLog(LOG_LEVEL_INFO,PSTR("NGH: قفل ورود (%u ثانیه)"),NGH_LOCK_SEC);
  }
}

/* ===================== ظاهر مدرن (دارک RTL) ===================== */
static const char NGH_CSS[] PROGMEM =
  "*{box-sizing:border-box;margin:0;padding:0}"
  "body{font-family:Tahoma,Vazirmatn,sans-serif;direction:rtl;text-align:right;"
  "background:radial-gradient(1200px 600px at 80% -10%,#1c3a5e 0%,#0b1420 55%,#070c14 100%);"
  "min-height:100vh;color:#e7eef7;padding:24px 14px 40px}"
  ".wrap{max-width:460px;margin:0 auto}"
  ".brand{display:flex;align-items:center;gap:12px;margin-bottom:18px}"
  ".brand .logo{width:44px;height:44px;border-radius:13px;"
  "background:linear-gradient(135deg,#2fd6a8,#1d6fd8);display:flex;align-items:center;"
  "justify-content:center;font-size:22px;box-shadow:0 8px 24px rgba(31,111,216,.35)}"
  ".brand h1{font-size:1.25rem;color:#fff}.brand small{color:#8fa5bd;display:block;font-size:.72rem}"
  ".card{background:rgba(22,35,58,.72);backdrop-filter:blur(8px);border:1px solid #26375a;"
  "border-radius:16px;padding:18px;margin:0 0 14px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
  ".card h2{font-size:.95rem;color:#7dd6a8;margin-bottom:12px}"
  "label{font-size:.78rem;color:#b9c8dd;display:block;margin:10px 0 4px}"
  "input,select{width:100%;background:#0e1a2c;border:1px solid #33456b;color:#eaf1fa;"
  "border-radius:10px;padding:10px;font-family:inherit;font-size:.85rem}"
  "input:focus{outline:none;border-color:#7dd6a8;box-shadow:0 0 0 3px rgba(125,214,168,.15)}"
  "button,.btn{display:inline-block;background:linear-gradient(135deg,#2fd6a8,#1d6fd8);color:#fff;border:0;"
  "border-radius:11px;padding:11px 20px;font-family:inherit;font-size:.9rem;cursor:pointer;width:100%;margin-top:12px}"
  ".btn{text-align:center;text-decoration:none}"
  "button:hover,.btn:hover{filter:brightness(1.12)}"
  "button.ghost,.btn.ghost{background:#0e1a2c;border:1px solid #33456b}"
  ".relay{display:flex;align-items:center;justify-content:space-between;padding:12px 4px;border-bottom:1px solid #20304d}"
  ".relay:last-child{border-bottom:0}.relay b{font-size:.9rem}.relay small{color:#8fa5bd;font-size:.7rem;display:block}"
  ".sw{position:relative;width:52px;height:28px;flex:0 0 auto}.sw input{display:none}"
  ".sw i{position:absolute;inset:0;background:#33456b;border-radius:99px;transition:.2s;cursor:pointer}"
  ".sw i:before{content:'';position:absolute;right:3px;top:3px;width:22px;height:22px;border-radius:50%;background:#8fa5bd;transition:.2s}"
  ".sw input:checked+i{background:#16a34a}.sw input:checked+i:before{transform:translateX(-24px);background:#fff}"
  ".badge{display:inline-block;font-size:.68rem;padding:3px 10px;border-radius:99px;margin-top:6px}"
  ".ok{background:rgba(15,157,88,.18);color:#5eead4;border:1px solid rgba(94,234,212,.3)}"
  ".bad{background:rgba(244,63,94,.12);color:#fda4af;border:1px solid rgba(253,164,175,.3)}"
  ".mut{background:#1a2740;color:#8fa5bd;font-size:.7rem;padding:8px 10px;border-radius:9px;margin-top:10px;line-height:1.7;word-break:break-all}"
  ".grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
  "#qr{background:#fff;padding:12px;border-radius:12px;display:inline-block;margin-top:10px}"
  ".footer{color:#5f7392;font-size:.68rem;text-align:center;margin-top:8px;line-height:1.8}";

/* ===================== HTTP helpers ===================== */
static void ngh_send(int code,const String& body){
  Webserver->sendHeader("Cache-Control","no-store");
  Webserver->send(code,"text/html; charset=utf-8",body);
}
static void ngh_redirect(const String& url){
  Webserver->sendHeader("Location",url,true);
  Webserver->send(303);
}
static String ngh_esc(const char* s){
  String o;
  for (const char* p=s;*p;p++){
    char c=*p;
    if (c=='&') o+=F("&amp;"); else if (c=='<') o+=F("&lt;");
    else if (c=='>') o+=F("&gt;"); else if (c=='\"') o+=F("&quot;");
    else o+=c;
  }
  return o;
}
static String ngh_power_json(){
  String j=F("[");
  for (uint8_t k=0;k<4;k++){ j+=bitRead(TasmotaGlobal.power,k)?"1":"0"; if (k<3) j+=","; }
  return j+F("]");
}
static String ngh_head(const char* sub){
  String s=F("<!DOCTYPE html><html lang='fa' dir='rtl'><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='theme-color' content='#0b1420'><title>نگهبان‌یار — ");
  s+=sub;
  s+=F("</title><style>"); s+=FPSTR(NGH_CSS);
  s+=F("</style></head><body><div class='wrap'>"
    "<div class='brand'><div class='logo'>🛡</div><div><h1>نگهبان‌یار</h1><small>");
  s+=ngh_esc(Ngh.site); s+=F(" • "); s+=ngh_esc(Ngh.bid);
  s+=F("</small></div></div>");
  return s;
}
static String ngh_foot(){
  return F("<p class='footer'>نگهبان‌یار × Tasmota — برد ESP-07 4Relay<br>"
    "API اپ: /ngh/api?op=state (امضای HMAC-SHA1 در هدر X-NGH-Key)</p></div></body></html>");
}
static void ngh_full_page(int code,const char* sub,const String& body){ ngh_send(code,ngh_head(sub)+body+ngh_foot()); }

/* ===================== صفحات ===================== */
static void ngh_handle_setup(){
  if (Ngh.secret_len>0){ ngh_redirect(F("/ngh/login")); return; }
  if (Webserver->method()==HTTP_POST){
    String sec=Webserver->arg(F("sec")); sec.toUpperCase(); sec.trim();
    uint8_t raw[24]; uint8_t n=base32_dec(sec.c_str(),raw,24);
    if (n>=10){
      memcpy(Ngh.secret_raw,raw,n); Ngh.secret_len=n;
      uint8_t macb[6]; WiFi.macAddress(macb);
      uint8_t mk[34]; memcpy(mk,raw,n); memcpy(mk+n,macb,6);
      uint8_t out[20]; hmac_sha1(raw,n,mk,n+6,out);   // api_key = HMAC(secret, secret||MAC)
      memcpy(Ngh.api_key,out,20);
      if (!ngh_save()) AddLog(LOG_LEVEL_ERROR,PSTR("NGH: ذخیره فایل ناموفق — USE_UFILESYS فعال است؟"));
      AddLog(LOG_LEVEL_INFO,PSTR("NGH: کلید TOTP و api_key ثبت شد"));
      ngh_redirect(F("/ngh/login")); return;
    }
  }
  /* نمایش کلید تصادفی پیشنهادی */
  uint8_t rnd[20];
  for (int i=0;i<20;i+=4){ uint32_t r=os_random();
    rnd[i]=r&0xff; rnd[i+1]=(r>>8)&0xff; rnd[i+2]=(r>>16)&0xff; rnd[i+3]=(r>>24)&0xff; }
  uint32_t chip=ESP.getChipId();
  for (int i=0;i<20;i++) rnd[i]^=(chip>>((i%4)*8))&0xff;
  char b32[41]; base32_enc(rnd,20,b32);
  String body=F("<div class='card'><h2>۱) ثبت کلید Google Authenticator</h2>"
    "<p class='mut'>دو راه دارید:<br>• <b>پیشنهاد امن:</b> با اسکریپت <code>tools/gen_secret.py</code> روی کامپیوتر یک Base32 قوی بسازید و اینجا جای‌گذاری کنید<br>"
    "• یا کلید پیشنهادی زیر را عیناً در گوگل‌اوتنتیکیتور (دسته‌ای وارد کردن کلید) وارد کنید</p>");
  body+=F("<label>کلید پیشنهادی (Base32)</label><input readonly value='"); body+=b32;
  body+=F("'><div class='mut'>با برنامهٔ Google Authenticator گزینهٔ «+» ← «Or enter code manually» ← همین کد را وارد کنید؛"
    "پس از ذخیره، دگمه‌ها شبکهٔ QR را هم می‌سازیم (نیاز به اینترنت نیست).</div>"
    "<div id='qr'></div><form method='post'><label>Base32 نهایی (باید با آنچه در گوشی ثبت کردید یکی باشد)</label>"
    "<input name='sec' maxlength='32' minlength='16' pattern='[A-Z2-7]+' value='"); body+=b32;
  body+=F("'><button>ذخیره کلید</button></form></div>"
    "<script src='/ngh/qr.js'></script><script>function draw(){var i=document.querySelector('[name=sec]');"
    "var u='otpauth://totp/NegahbanYar:'+encodeURIComponent('"); body+=ngh_esc(Ngh.bid);
  body+=F("')+'?secret='+i.value.trim().toUpperCase()+'&issuer=NegahbanYar&digits=6&period=30';"
    "document.getElementById('qr').innerHTML='';new QRCode(document.getElementById('qr'),{text:u,width:200,height:200,colorDark:'#0b1420'});}"
    "draw();document.querySelector('[name=sec]').addEventListener('input',draw);</script>");
  ngh_full_page(200,"راه‌اندازی",body);
}

static void ngh_handle_login(){
  if (Ngh.secret_len==0){ ngh_redirect(F("/ngh/setup")); return; }
  if (ngh_locked()){
    ngh_full_page(403,"مسدود",F("<div class='card'><h2>⏳ قفل امنیتی</h2>"
      "<p class='mut'>تعداد تلاش ناموفق از حد گذشت؛ تا چند دقیقه ورود غیرفعال است.</p></div>"));
    return;
  }
  if (Webserver->method()==HTTP_POST){
    if (!ngh_time_ok()){
      ngh_full_page(200,"ساعت سینک نشده",F("<div class='card'><h2>⚠ ساعت دستگاه تنظیم نیست</h2>"
        "<p class='mut'>TOTP به زمان دقیق نیاز دارد. دستگاه را به اینترنت متصل کنید تا NTP کار کند؛ "
        "بعد از اتصال WiFi چند ده ثانیه صبر و دوباره تلاش کنید.</p></div>"));
      return;
    }
    String code=Webserver->arg(F("code"));
#ifdef NGH_DEBUG_SERIAL
    uint32_t cc; totp_code(Ngh.secret_raw,Ngh.secret_len,ngh_utc()/30,&cc);
    AddLog(LOG_LEVEL_DEBUG,PSTR("NGH: current totp=%06lu, got=%s"),(unsigned long)cc,code.c_str());
#endif
    if (totp_check(Ngh.secret_raw,Ngh.secret_len,ngh_utc(),code.c_str())){
      char tok[48]; ngh_make_token(ngh_utc()+NGH_SES_TTL,tok);
      Webserver->sendHeader("Set-Cookie",String(NGH_COOKIE)+"="+tok+"; Max-Age="+String(NGH_SES_TTL)+"; HttpOnly; SameSite=Strict");
      ngh_fail_cnt=0;
      AddLog(LOG_LEVEL_INFO,PSTR("NGH: ورود موفق ادمین"));
      ngh_redirect(F("/ngh")); return;
    }
    ngh_reg_fail();
  }
  ngh_full_page(200,"ورود",
    F("<div class='card'><h2>ورود ادمین — رمز یکبار مصرف گوگل</h2>"
      "<p class='mut'>کد ۶ رقمی فعلی Google Authenticator را وارد کنید. "
      "بعد از ورود به تنظیمات سامانهٔ نگهبان‌یار و رله‌ها دسترسی باز می‌شود.</p>"
      "<form method='post'><label>کد ۶ رقمی TOTP</label>"
      "<input name='code' inputmode='numeric' pattern='[0-9]{6}' maxlength='6' autocomplete='one-time-code' autofocus required>"
      "<button>ورود</button></form></div>"));
}

static void ngh_handle_home(){
  if (Ngh.secret_len==0){ ngh_redirect(F("/ngh/setup")); return; }
  if (!ngh_session()){ ngh_redirect(F("/ngh/login")); return; }
  String body=F("<div class='card'><h2>خروجی‌های رله</h2>");
  for (uint8_t i=0;i<4;i++){
    bool on=bitRead(TasmotaGlobal.power,i);
    body+=F("<div class='relay'><div><b>"); body+=ngh_esc(Ngh.rname[i]);
    body+=F("</b><small>RELAY"); body+=String(i+1);
    body+=F(" · GPIO"); body+=String(NGH_RELAY_GPIO[i]);
    body+=F("</small></div>"
      "<form method='post' action='/ngh/api'>"
      "<input type='hidden' name='op' value='relay'><input type='hidden' name='i' value='");
    body+=String(i+1);
    body+=F("'><input type='hidden' name='act' value='toggle'>"
      "<label class='sw'><input type='checkbox' onchange='this.form.submit()'");
    if (on) body+=F(" checked");
    body+=F("><i></i></label></form></div>");
  }
  body+=F("</div><div class='card'><h2>وضعیت</h2><div class='grid2'><div class='mut'>IP: ");
  body+=WiFi.localIP().toString();
  body+=F("<br>رله‌های فعال: "); body+=String(TasmotaGlobal.devices_present);
  body+=F("</div><div class='mut'>ساعت UTC: ");
  if (ngh_time_ok()){ body+=String(1900+RtcTime.year)+"/"+String(RtcTime.month)+"/"+String(RtcTime.day_of_month)+
    " "+String(RtcTime.hour)+":"+String(RtcTime.minute); }
  else body+=F("⚠ (سینک نشده — NTP)");
  body+=F("<br>آپ‌تایم: "); body+=String(Rtc.uptime);
  body+=F("</div></div><p class='mut'>قفل‌متقابل: ");
  body+=Ngh.interlock?F("روشن (Interlock 1)"):F("خاموش");
  body+=F(" • وبهوک: "); body+=Ngh.webhook[0]?ngh_esc(Ngh.webhook):F("تنظیم نشده");
  body+=F("</p></div>");
  body+=F("<a class='btn ghost' href='/ngh/config'>⚙ تنظیمات نگهبان‌یار</a>"
    "<a class='btn ghost' href='/'>صفحه Tasmota</a>"
    "<a class='btn ghost' href='/ngh/logout'>خروج</a>");
  ngh_full_page(200,"داشبورد",body);
}

static void ngh_handle_config(){
  if (Ngh.secret_len==0){ ngh_redirect(F("/ngh/setup")); return; }
  if (!ngh_session()){ ngh_redirect(F("/ngh/login")); return; }
  if (Webserver->method()==HTTP_POST){
    if (Webserver->arg(F("do"))==F("rotate")){
      /* چرخش کلید اپ بدون دست‌زدن به کلید TOTP */
      uint8_t msg[28]; memcpy(msg,Ngh.api_key,20);
      uint32_t t=ngh_utc(); for (int i=0;i<4;i++) msg[20+i]=(t>>(8*(3-i)))&0xff;
      uint8_t out[20]; hmac_sha1(Ngh.secret_raw,Ngh.secret_len,msg,24,out);
      memcpy(Ngh.api_key,out,20); ngh_save();
      AddLog(LOG_LEVEL_INFO,PSTR("NGH: api_key چرخانده شد"));
      ngh_redirect(F("/ngh/config?rot=1")); return;
    }
    Webserver->arg("site").toCharArray(Ngh.site,sizeof(Ngh.site));
    Webserver->arg("bid").toCharArray(Ngh.bid,sizeof(Ngh.bid));
    for (uint8_t i=0;i<4;i++){ String k="rn"+String(i); Webserver->arg(k).toCharArray(Ngh.rname[i],sizeof(Ngh.rname[i])); }
    Webserver->arg("hook").toCharArray(Ngh.webhook,sizeof(Ngh.webhook));
    Ngh.interlock=Webserver->arg("il")==F("1")?1:0;
    ExecuteCommandP(Ngh.interlock?PSTR("Interlock 1"):PSTR("Interlock 0"));
    ngh_save();
    AddLog(LOG_LEVEL_INFO,PSTR("NGH: تنظیمات ذخیره شد"));
    ngh_redirect(F("/ngh/config?saved=1")); return;
  }
  String body;
  if (Webserver->hasArg("saved")) body+=F("<div class='badge ok' style='display:block;text-align:center;padding:8px'>✓ تنظیمات ذخیره شد</div>");
  if (Webserver->hasArg("rot")) body+=F("<div class='badge ok' style='display:block;text-align:center;padding:8px'>✓ کلید API چرخانده شد — اپ را با کلید جدید به‌روز کنید</div>");
  body+=F("<form method='post'>"
    "<div class='card'><h2>هویت سامانهٔ نگهبان‌یار</h2>"
    "<label>نام مجتمع / ساختمان</label><input name='site' maxlength='19' value='"); body+=ngh_esc(Ngh.site);
  body+=F("'><label>Building ID (کد ساختمان در اپ)</label><input name='bid' maxlength='9' value='"); body+=ngh_esc(Ngh.bid);
  body+=F("'><label>وبهوک رویداد رله (اختیاری — GET به این URL پس از هر تغییر)</label>"
    "<input name='hook' maxlength='59' placeholder='http://192.168.1.50:9000/ngh/event' value='"); body+=ngh_esc(Ngh.webhook);
  body+=F("'><label>قفل‌متقابل رله‌ها</label><select name='il'>"
    "<option value='0'"); if (!Ngh.interlock) body+=F(" selected");
  body+=F(">خاموش</option><option value='1'"); if (Ngh.interlock) body+=F(" selected");
  body+=F(">هر لحظه فقط یک رله روشن</option></select></div>");
  body+=F("<div class='card'><h2>نام‌های رله (در اپ نمایش داده می‌شود)</h2>");
  for (uint8_t i=0;i<4;i++){
    body+=F("<label>RELAY"); body+=String(i+1);
    body+=F(" (GPIO"); body+=String(NGH_RELAY_GPIO[i]); body+=F(")</label>"
      "<input name='rn"); body+=String(i);
    body+=F("' maxlength='13' value='"); body+=ngh_esc(Ngh.rname[i]); body+="'>";
  }
  body+=F("</div><button>ذخیره تنظیمات</button></form>");
  body+=F("<div class='card'><h2>کلید API اپ (X-NGH-Key)</h2>"
    "<p class='mut'>امضای اصلی اپلیکیشن = این کلید ۲۰بایتی. فقط در گوشی اصلی نگه دارید؛ "
    "با چرخش، کلاینت‌های قبلی از کار می‌افتند و TOTP دست‌نخورده می‌ماند.</p>"
    "<label>کلید فعلی (HEX)</label><input readonly value='");
  char kh[41]; to_hex(Ngh.api_key,20,kh); body+=kh;
  body+=F("'><form method='post'><input type='hidden' name='do' value='rotate'>"
    "<button class='ghost'>چرخش کلید API</button></form></div>");
  body+=F("<a class='btn ghost' href='/ngh'>بازگشت به داشبورد</a>");
  ngh_full_page(200,"تنظیمات",body);
}

/* وبهوک رویداد — GET ساده بدون کتابخانهٔ اضافی */
static void ngh_push_webhook(int8_t idx){
  if (!Ngh.webhook[0]) return;
  if (strncmp_P(Ngh.webhook,PSTR("http://"),7)!=0) return;
  String w=Ngh.webhook+7;
  int slash=w.indexOf('/');
  String hostport=(slash<0)?w:w.substring(0,slash);
  String path=(slash<0)?"/":w.substring(slash);
  int colon=hostport.indexOf(':');
  String host=(colon<0)?hostport:hostport.substring(0,colon);
  uint16_t port=(colon<0)?80:(uint16_t)hostport.substring(colon+1).toInt();
  WiFiClient cl; cl.setTimeout(1000);
  if (!cl.connect(host.c_str(),port)) return;
  String rq=F("GET "); rq+=path;
  rq+=F("?i="); rq+=String(idx);
  rq+=F("&st="); rq+=bitRead(TasmotaGlobal.power,idx-1)?"1":"0";
  rq+=F("&bid="); rq+=Ngh.bid;
  rq+=F(" HTTP/1.0\r\nHost: "); rq+=hostport; rq+=F("\r\nConnection: close\r\n\r\n");
  cl.print(rq);
  delay(20); cl.stop();
}

/* احراز هویت HMAC اپلیکیشن نگهبان‌یار */
static bool ngh_api_hmac_ok(){
  if (!Webserver->hasHeader("X-NGH-Key")||Ngh.secret_len==0||!Webserver->hasArg("ts")) return false;
  uint32_t ts=Webserver->arg("ts").toInt();
  if (ngh_time_ok() && (ngh_utc()>ts+120 || ts>ngh_utc()+120)) return false;
  String base=F("op=")+Webserver->arg("op")+F("&i=")+Webserver->arg("i")+F("&act=")+Webserver->arg("act")+F("&ts=")+Webserver->arg("ts");
  uint8_t mac[20]; hmac_sha1(Ngh.api_key,20,(const uint8_t*)base.c_str(),base.length(),mac);
  uint8_t got[20];
  if (hex_dec(Webserver->header("X-NGH-Key").c_str(),got,20)!=20) return false;
  bool ok=ctime_eq(got,mac,20);
#ifdef NGH_DEBUG_TOKEN
  AddLog(LOG_LEVEL_DEBUG,PSTR("NGH: api hmac=%d base=%s"),(int)ok,base.c_str());
#endif
  return ok;
}
static void ngh_handle_api(){
  bool human=Webserver->hasHeader("Cookie") && ngh_session();
  bool via_app=ngh_api_hmac_ok();
  if (!(human||via_app)){ Webserver->send(401,F("application/json"),F("{\"ok\":0,\"err\":\"auth\"}")); return; }
  String op=Webserver->arg("op");
  if (op==F("relay")&&Webserver->hasArg("i")){
    int i=Webserver->arg("i").toInt();
    String act=Webserver->arg("act");
    if (i>=1&&i<=4){
      if (act==F("on")) ExecuteCommandPower(i,POWER_ON,SRC_IGNORE);
      else if (act==F("off")) ExecuteCommandPower(i,POWER_OFF,SRC_IGNORE);
      else ExecuteCommandPower(i,POWER_TOGGLE,SRC_IGNORE);
      if (Ngh.interlock && bitRead(TasmotaGlobal.power,i-1))
        for (int k=1;k<=4;k++) if (k!=i) ExecuteCommandPower(k,POWER_OFF,SRC_IGNORE);
      AddLog(LOG_LEVEL_INFO,PSTR("NGH: relay %d %s (%s)"),i,act.c_str(),human?"web":"app");
      ngh_push_webhook(i);
    }
    if (human){ ngh_redirect(F("/ngh")); return; }
    String j=F("{\"ok\":1,\"power\":"); j+=ngh_power_json(); j+="}";
    Webserver->send(200,F("application/json"),j);
    return;
  }
  if (op==F("state")||op==F("info")){
    String j=F("{\"ok\":1,\"site\":\""); j+=ngh_esc(Ngh.site);
    j+=F("\",\"bid\":\""); j+=ngh_esc(Ngh.bid);
    j+=F("\",\"power\":"); j+=ngh_power_json();
    j+=F(",\"time\":"); j+=String(RtcTime.utc_time);
    j+=F(",\"names\":[");
    for (uint8_t k=0;k<4;k++){ j+="\""+ngh_esc(Ngh.rname[k])+"\""; if (k<3) j+=","; }
    j+="]}";
    Webserver->send(200,F("application/json"),j);
    return;
  }
  Webserver->send(400,F("application/json"),F("{\"ok\":0,\"err\":\"bad op\"}"));
}
static void ngh_handle_qrjs(){
  Webserver->sendHeader("Cache-Control","max-age=86400");
  Webserver->send(200,F("application/javascript"),FPSTR(NGH_QRJS));
}
static void ngh_handle_logout(){
  Webserver->sendHeader("Set-Cookie",String(NGH_COOKIE)+"=; Max-Age=0; Path=/");
  ngh_redirect(F("/ngh/login"));
}

/* ===================== تبعیت استاندارد Tasmota ===================== */
static bool ngh_web_registered=false;
static void ngh_web_init(){
  if (ngh_web_registered) return;
  Webserver->on(F("/ngh"),HTTP_GET,ngh_handle_home);
  Webserver->on(F("/ngh/login"),HTTP_ANY,ngh_handle_login);
  Webserver->on(F("/ngh/setup"),HTTP_ANY,ngh_handle_setup);
  Webserver->on(F("/ngh/config"),HTTP_ANY,ngh_handle_config);
  Webserver->on(F("/ngh/api"),HTTP_ANY,ngh_handle_api);
  Webserver->on(F("/ngh/qr.js"),HTTP_GET,ngh_handle_qrjs);
  Webserver->on(F("/ngh/logout"),HTTP_GET,ngh_handle_logout);
  ngh_web_registered=true;
}

/*************************************************************************************************\
 * دیسپچر اصلی درایور — قرارداد استاندارد Tasmota
\***                                                                                               */
bool Xdrv150(uint8_t function){
  bool result=false;
  switch (function){
    case FUNC_INIT:
      if (!ngh_load()){ ngh_defaults(); ngh_save(); }
      AddLog(LOG_LEVEL_INFO,PSTR("NGH: نگهبان‌یار آماده است (secret=%dB)"),Ngh.secret_len);
      break;
    case FUNC_WEB_ADD_HANDLER:
      ngh_web_init();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
    case FUNC_WEB_ADD_MANAGEMENT_BUTTON:
      WSContentSend_P(PSTR("<p><form action='/ngh' method='get'><button>🛡 نگهبان‌یار</button></form></p>"));
      result=true;
      break;
  }
  return result;
}

/* SELF-ADAPTIVE NOTES — اگر در نسخهٔ Tasmota شما چیزی کامپایل نشد:
 * 1) TasmotaGlobal.power: در بعضی نسخه‌ها نام/ساختار متفاوت است؛ در xdrv_01_9_webserver.ino
 *    دنبال "Devices.present" یا متغیر حالت-power بگردید و در ngh_power_json / handlers
 *    همان عبارت را جایگزین کنید (همین نکته در relay_rows HOME لازم است).
 * 2) ExecuteCommandP: اگر خطا خورد، با ExecuteCommand("Interlock 1") جایگزین کنید.
 * 3) Webserver->on(...) و هدرها: الگوی استاندارد dev 2026 است؛ در نسخه‌های قدیمی‌تر
 *    Webserver->on(uri, fn) بدون HTTP_ANY کار می‌کند.
 * 4) TfsLoadFile/TfsSaveFile نیازمند USE_UFILESYS است (پیش‌فرض فعال است) — بدون آن،
 *    تنظیمات فقط تا ریست بعدی در RAM می‌ماند.
 */

#endif  // USE_XDRV_150
