// ti89_combo_esp32.ino — 1 firmware, 2 chế độ cho TI-89 Titanium (hàn trong máy)
//   • AI (Claude)         : chạy gpt() trên máy rồi gõ câu hỏi (hoặc gpt("cau hoi"))
//   • Tay cầm Bluetooth   : chạy pad() trên máy, nhấn ON để thoát
// Board: ESP32-S3 Super Mini (Tools → Board → "ESP32S3 Dev Module") hoặc Seeed XIAO ESP32-C3.
//        Code tự nhận loại chip, không cần sửa khi đổi board.
// Thư viện: ArduinoJson (v7), ESP32-BLE-Gamepad (lemmingDev) + NimBLE-Arduino.
//
// Giao thức khung tin: [lenLo][lenHi][lệnh][dữ liệu...]
//   'Q' + text      : hỏi AI          → ESP trả lời 1 khung [lenLo][lenHi][text]
//   'P'             : vào chế độ tay cầm (bật Bluetooth)   → không trả lời
//   'K' + 10 byte   : trạng thái 10 hàng phím (_rowread)   → không trả lời
//   'X'             : thoát tay cầm, nhả hết nút           → không trả lời
// ESP KHÔNG bao giờ tự gửi khi máy không hỏi.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "esp_sleep.h"
#include "esp_system.h"
#include <stdarg.h>
#include "driver/gpio.h"

#define USE_BLE 1          // 1 = bản thật (có tay cầm Bluetooth); 0 = bỏ Bluetooth (để test trên Wokwi)
#if USE_BLE
#include <BleGamepad.h>
#include <NimBLEDevice.h>
#endif

// ====== CẤU HÌNH ======
const char* WIFI_SSID     = "TEN_WIFI";        // chỉ WiFi 2.4 GHz
const char* WIFI_PASS     = "MAT_KHAU_WIFI";
const char* CLAUDE_KEY    = "sk-ant-...";
const char* CLAUDE_MODEL  = "claude-sonnet-5-5";           // $2/$10 mỗi triệu token
// const char* CLAUDE_MODEL = "claude-haiku-4-5-20251001"; // rẻ hơn một nửa ($1/$5)
#define ECHO_TEST 0        // 1 = chỉ trả lại câu hỏi (test dây link, không cần WiFi), 0 = gọi Claude thật
#define KEY_DEBUG 0        // 1 = in phím bấm ra Serial Monitor để lập bảng gán nút
#define SERIAL_TEST 1      // 1 = gõ câu hỏi vào Serial Monitor để test AI không cần TI-89
#define FRAME_STABLE_US 20   // µs dây phải ổn định ở bit đầu khung (thử 10..200 nếu còn lỗi)
#define FALSE_START_FIX 0  // thử nghiệm, để 0
#define LED_DEBUG 0        // 1 = LED RGB báo trạng thái link (dò lỗi khi đã hàn vào máy); 0 = tắt LED cho đỡ pin
#define USE_SLEEP 1        // 1 = ngủ nhẹ khi rảnh, thức khi TI-89 kéo dây tip/ring; 0 = luôn thức (debug)
const uint32_t IDLE_SLEEP_MS = 600000;   // 10 phút không có câu hỏi / phím bấm thì ngủ
const uint32_t BOOT_AWAKE_MS = 30000;    // 30 giây đầu sau khi có điện luôn thức (để nạp code)

// GPIO4 / GPIO5:  ESP32-S3 Super Mini = chân in số "4" và "5" (hàng trái)
//                 XIAO ESP32-C3       = chân D2 và D3
// Hàn ngược Tip/Ring? Đổi SWAP_TIP_RING thành 1 là xong, khỏi hàn lại.
#define SWAP_TIP_RING 0
#if SWAP_TIP_RING
const int PIN_RED   = 5;   // tip  nằm ở pad 5
const int PIN_WHITE = 4;   // ring nằm ở pad 4
#else
const int PIN_RED   = 4;   // -> tip
const int PIN_WHITE = 5;   // -> ring
#endif
const uint32_t BIT_TIMEOUT_US = 1000000;
const uint32_t WIFI_IDLE_MS   = 60000;   // tắt WiFi sau 60s không dùng (tiết kiệm pin)
const int MAX_Q = 250;
const int MAX_A = 1000;
const int KEY_ROWS = 10;

// ====== LED BÁO TRẠNG THÁI (GPIO48 trên S3 Super Mini) ======
// Xanh lá nháy 3 lần khi khởi động | xanh dương = nhận khung OK
// Đỏ = nhận/gửi lỗi giữa chừng      | vàng sáng liền = có dây bị kéo xuống 0V liên tục
// Tím = TI có gửi nhưng không thấy ESP trả lời. Sau đó tự đo dây:
//   trắng nháy 4 lần = dây pad 4 hở | 5 lần = pad 5 hở | 9 lần = cả hai hở
//   xanh ngọc 2 lần = hai dây đều có điện áp (nghi điện trở / hàn nhầm điểm)
uint32_t ledOffAt = 0;
void led(uint8_t r, uint8_t g, uint8_t b, uint32_t ms = 150) {
#if LED_DEBUG && CONFIG_IDF_TARGET_ESP32S3
  neopixelWrite(48, r, g, b);
  ledOffAt = ms ? millis() + ms : 0;
#endif
}

// ====== NHẬT KÝ LƯU VÀO FLASH ======
// Ghi lại các sự kiện link/WiFi kể cả khi chạy bằng pin (không có Serial).
// Sau đó tháo pin, cắm USB, gõ /log trong Serial Monitor để đọc lại.
String dbg;
void dsave() {
  Preferences p;
  if (p.begin("ti89log", false)) { p.putString("log", dbg); p.end(); }
}
void dlog(const String& s, bool save = true) {
  Serial.println(s);
  dbg += String((int)(millis() / 1000)) + "s " + s + "\n";
  if (dbg.length() > 1500) dbg = dbg.substring(dbg.length() - 1200);
  if (save) dsave();
}
void dlogf(const char* fmt, ...) {
  char b[160];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  dlog(String(b));
}
void dload() {
  Preferences p;
  if (p.begin("ti89log", true)) { dbg = p.getString("log", ""); p.end(); }
}

// ====== LỚP BIT (giao thức 2 dây của TI) ======
// Chân để ở chế độ open-drain + pull-up: ghi LOW = kéo dây, ghi HIGH = thả dây (không bao giờ đẩy lên 3.3V).
// Đổi mức chỉ cần digitalWrite (~1 µs) thay vì pinMode (chậm hàng chục µs) -> trả lời TI kịp thời.
inline void initPin(int p) { digitalWrite(p, HIGH); pinMode(p, OUTPUT_OPEN_DRAIN | PULLUP); digitalWrite(p, HIGH); }
inline void pullLow(int p) { digitalWrite(p, LOW); }
inline void release(int p) { digitalWrite(p, HIGH); }
inline bool isHigh(int p)  { return digitalRead(p) == HIGH; }
void releaseAll() { initPin(PIN_RED); initPin(PIN_WHITE); }
// Khởi động lại mà KHÔNG làm giật dây link (dây giật lúc TI đang tắt sẽ đánh thức TI).
// gpio_hold giữ chân ở trạng thái "nhả + kéo lên" suốt quá trình reset; setup() sẽ bỏ giữ.
void safeRestart() {
  releaseAll();
  delay(5);
  gpio_hold_en((gpio_num_t)PIN_RED);
  gpio_hold_en((gpio_num_t)PIN_WHITE);
  Serial.flush();
  ESP.restart();
}

// ---- Ghi vết (trace) thay đổi trên dây để dò lỗi giao thức ----
// Mỗi mục: thời gian (µs, tính từ đầu khung), trạng thái 2 dây, và việc ESP vừa làm.
struct TraceEv { uint32_t t; uint8_t st; char ev; };
const int TRACE_N = 40;
TraceEv trc[TRACE_N];
int trcN = 0; uint32_t trc0 = 0; uint8_t trcLast = 0xFF;
inline uint8_t lineState() { return (isHigh(PIN_RED) ? 1 : 0) | (isHigh(PIN_WHITE) ? 2 : 0); }
void trace(char ev) {
  uint8_t st = lineState();
  if (ev == '.' && st == trcLast) return;          // chỉ ghi khi dây đổi trạng thái
  if (trcN == 0) trc0 = micros();
  if (trcN < TRACE_N) trc[trcN++] = { micros() - trc0, st, ev };
  trcLast = st;
}
void traceReset() { trcN = 0; trcLast = 0xFF; }
void traceDump() {
  // T = Tip (pad đỏ), R = Ring; chữ hoa = cao (thả), chữ thường = thấp (bị kéo)
  // Sự kiện: '.' dây đổi, 'A' ESP kéo dây trả lời, 'L' ESP thả, 'X' hết giờ
  String out = "Trace:";
  for (int i = 0; i < trcN; i++) {
    out += " " + String((int)trc[i].t) + (trc[i].st & 1 ? "T" : "t") + (trc[i].st & 2 ? "R" : "r");
    if (trc[i].ev != '.') out += trc[i].ev;
  }
  dlog(out);
}

bool waitLevel(int p, bool level, uint32_t to = BIT_TIMEOUT_US) {
  uint32_t t = micros();
  while (isHigh(p) != level) { trace('.'); if (micros() - t > to) { trace('X'); return false; } }
  trace('.');
  return true;
}

int sendFailStage = 0, sendFailBit = 0;   // để ghi nhật ký khi gửi lỗi
bool sendByte(uint8_t b) {
  for (int i = 0; i < 8; i++) {
    int mine  = (b & 1) ? PIN_WHITE : PIN_RED;   // bit 1 kéo trắng, bit 0 kéo đỏ
    int other = (b & 1) ? PIN_RED   : PIN_WHITE;
    // Chờ cả hai dây rảnh trước khi kéo (TI có thể chưa nhả xong bit trước)
    if (!waitLevel(PIN_RED, true) || !waitLevel(PIN_WHITE, true)) { sendFailStage = 1; sendFailBit = i; releaseAll(); return false; }
    pullLow(mine); trace('S');
    if (!waitLevel(other, false)) { sendFailStage = 2; sendFailBit = i; releaseAll(); return false; }  // TI không trả lời
    release(mine); trace('U');
    if (!waitLevel(mine, true) || !waitLevel(other, true)) { sendFailStage = 3; sendFailBit = i; releaseAll(); return false; }
    b >>= 1;
  }
  return true;
}

int lastTheirPin = -1, lastAckPin = -1;
int getFailStage = 0, getFailBit = 0;     // nhận lỗi ở bước nào / bit thứ mấy (cho nhật ký)
// Sau khi lỗi giữa khung, TI vẫn có thể đang gửi nốt -> chờ hai dây rảnh liên tục 30 ms rồi mới nhận khung mới
void resync() {
  releaseAll();
  uint32_t t0 = millis(), quiet = millis();
  while (millis() - t0 < 2000) {
    if (!(isHigh(PIN_RED) && isHigh(PIN_WHITE))) quiet = millis();
    else if (millis() - quiet > 30) return;
  }
}   // dây TI kéo / dây ESP dùng để trả lời (cho nhật ký)

// -1 = không có gì gửi tới; -2 = TI đã bắt đầu gửi nhưng hỏng giữa chừng
int getByte(uint32_t firstTimeoutUs) {
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    uint32_t to = (i == 0) ? firstTimeoutUs : BIT_TIMEOUT_US;
    uint32_t t = micros();
    // Chờ bit bắt đầu: đúng MỘT dây thấp và giữ ổn định. Khi TI mở/reset cổng link, cả hai dây
    // có thể bị kéo thấp chớp nhoáng (~200 µs) -> trước đây ESP tưởng là bit, lệch nhịp cả khung.
    // Bit đầu khung phải ổn định 1 ms (TI giữ dây tới khi được trả lời nên không mất gì), các bit sau 5 µs.
    bool frameStart = (firstTimeoutUs != BIT_TIMEOUT_US && i == 0);
    // Bit đầu khung: chờ ổn định 20 µs (trước là 1 ms — quá lâu: TI kéo Ring mà không được trả lời
    // nhanh thì dây Tip bị giữ cao, ESP không kéo xuống được nữa).
    uint32_t stableUs = frameStart ? FRAME_STABLE_US : 5;
    uint8_t st;
    while (true) {
      st = lineState();
      if (st == 1 || st == 2) {                         // đúng một dây thấp
        uint32_t s0 = micros(); bool ok = true;
        while (micros() - s0 < stableUs) if (lineState() != st) { ok = false; break; }
        if (ok) break;
      }
      if (micros() - t > to) { getFailStage = 1; getFailBit = i; return i == 0 ? -1 : -2; }
    }
    trace('.');
    bool bit1 = (st & 1);                               // Tip (đỏ) còn cao -> TI kéo Ring -> bit 1
    int theirs = bit1 ? PIN_WHITE : PIN_RED;
    int mine   = bit1 ? PIN_RED   : PIN_WHITE;
    lastTheirPin = theirs; lastAckPin = mine;
    pullLow(mine); trace('A');
    if (FALSE_START_FIX && frameStart) {
      // TI thật sự gửi bit thì nhả dây chỉ ~15 µs sau khi ESP trả lời. Nếu 20 ms vẫn chưa nhả thì đó
      // không phải dữ liệu (xung khi TI mở/reset cổng link). Thả dây, chờ yên rồi bắt đầu lại từ đầu,
      // thay vì giữ dây trả lời 1 giây làm TI rối khi nó bắt đầu gửi thật.
      if (!waitLevel(theirs, true, 20000)) {
        release(mine); trace('Z');
        uint32_t w = millis();
        while (!(isHigh(PIN_RED) && isHigh(PIN_WHITE)) && millis() - w < 3000) trace('.');
        return -4;
      }
    } else if (!waitLevel(theirs, true)) { getFailStage = 2; getFailBit = i; releaseAll(); return -2; }   // TI không thấy mình trả lời
    release(mine); trace('L');
    // Không bắt buộc thấy dây mình lên lại: TI có thể bắt đầu bit kế chỉ ~8 µs sau khi ESP thả,
    // lúc đó dây đã bị TI kéo xuống lần nữa (trước đây bị tính nhầm là lỗi "day ESP khong len lai").
    waitLevel(mine, true, 100);
    b >>= 1;
    if (bit1) b |= 0x80;
  }
  return b;
}

// ====== LỚP KHUNG TIN ======
// Trả về số byte đã lưu, -1 nếu không có gì / lỗi. Byte thừa vẫn được đọc hết.
// ====== ĐO TỪNG DÂY ======
// Bật điện trở kéo XUỐNG bên trong ESP (~45kΩ) rồi đọc số. Dây nối tới TI (đang bật) bị điện trở
// kéo lên của TI (mạnh hơn nhiều) giữ ở mức 1; dây hở thì bị kéo về 0.
// (Bản trước dùng analogRead: chế độ analog tắt điện trở kéo xuống nên dây hở vẫn đọc ~3.1V -> sai.)
bool lineConnected(int pin) {
  pinMode(pin, INPUT_PULLDOWN);
  delay(20);                         // đợi điện tích còn sót trên dây xả hết
  int ones = 0;
  for (int i = 0; i < 20; i++) { ones += digitalRead(pin); delayMicroseconds(200); }
  initPin(pin);
  return ones >= 18;
}
// Trả về số pad bị hở (4/5), 45 nếu cả hai, 0 nếu cả hai ổn, -1 nếu không đo được
int lineTest() {
  uint32_t t = millis();
  while (!(isHigh(PIN_RED) && isHigh(PIN_WHITE))) {        // chờ TI nhả dây
    if (millis() - t > 3000) { dlog("Do day: dong bi keo thap, khong do duoc"); return -1; }
    delay(5);
  }
  bool okT = lineConnected(PIN_RED), okR = lineConnected(PIN_WHITE);
  dlogf("Do day: Tip (pad %d) %s, Ring (pad %d) %s", PIN_RED, okT ? "NOI" : "HO", PIN_WHITE, okR ? "NOI" : "HO");
  if (!okT && !okR) return 45;
  if (!okT) return PIN_RED;
  if (!okR) return PIN_WHITE;
  dlog("Do day: ca hai day deu noi toi TI");
  return 0;
}
// Nháy LED trắng N lần = pad N lỗi; 2 lần xanh ngọc = hai dây đều có điện áp
void blinkResult(int r) {
#if LED_DEBUG && CONFIG_IDF_TARGET_ESP32S3
  delay(600);
  if (r == 4 || r == 5 || r == 45) {
    int n = (r == 45) ? 9 : r;
    for (int i = 0; i < n; i++) { neopixelWrite(48, 30, 30, 30); delay(250); neopixelWrite(48, 0, 0, 0); delay(250); }
  } else if (r == 0) {
    for (int i = 0; i < 2; i++) { neopixelWrite(48, 0, 30, 30); delay(500); neopixelWrite(48, 0, 0, 0); delay(400); }
  }
#endif
}

bool traceDumpPending = false;
// -1 = không có gì, -2 = lỗi giữa chừng, -3 = hỏng ngay bit đầu (TI không thấy ESP trả lời)
int readFrame(uint8_t* buf, int maxLen) {
  traceReset();
  int lo = getByte(2000);
  if (lo == -4) { dlog("Link: bo qua xung gia o dau khung"); traceDump(); return -1; }
  if (lo != -1) traceDumpPending = true;
  if (lo == -2) {
    dlogf("Link: TI keo pad %d, ESP tra loi qua pad %d nhung TI khong thay -> nghi day pad %d",
          lastTheirPin, lastAckPin, lastAckPin);
    return -3;
  }
  if (lo < 0) return -1;
  static const char* WHY[] = {"", "cho bit tiep theo qua lau", "TI khong nha day sau khi ESP tra loi", "day ESP khong len lai"};
  traceReset();
  int hi = getByte(BIT_TIMEOUT_US);
  if (hi < 0) { dlogf("Link: loi o byte 2 (byte 1 = 0x%02X), bit %d: %s", lo, getFailBit, WHY[getFailStage]); traceDump(); resync(); return -2; }
  // Khung đúng luôn có byte cao = 0 (dài < 256). Ngược dây thì mọi bit bị đảo -> byte cao = 0xFF
  if (hi == 0xFF) dlogf("Link: byte cao = 0xFF -> co the nguoc Tip/Ring (SWAP_TIP_RING = %d)", SWAP_TIP_RING);
  int n = lo | (hi << 8), stored = 0;
  if (n == 0 || n > MAX_Q + 8) {                   // khung TI gửi không bao giờ dài thế -> đang lệch nhịp
    dlogf("Link: do dai khung vo ly (%d) -> bo, dong bo lai", n);
    resync();
    return -2;
  }
  for (int i = 0; i < n; i++) {
    traceReset();                                  // chỉ giữ vết của byte đang nhận
    int c = getByte(BIT_TIMEOUT_US);
    if (c < 0) { dlogf("Link: loi o byte %d/%d, bit %d: %s", i + 3, n + 2, getFailBit, WHY[getFailStage]); traceDump(); resync(); return -2; }
    if (stored < maxLen) buf[stored++] = (uint8_t)c;
  }
  return stored;
}

bool sendFrame(const String& s) {
  uint16_t n = s.length();
  traceReset();
  int idx = -2;
  bool ok = sendByte(n & 0xFF) && (idx = -1, sendByte(n >> 8));
  if (ok) {
    for (uint16_t i = 0; i < n; i++) {
      traceReset();                             // chỉ giữ vết của byte đang gửi (byte lỗi)
      if (!sendByte((uint8_t)s[i])) { ok = false; idx = i; break; }
    }
  }
  if (!ok) {
    const char* why[] = {"", "day chua ranh", "TI khong tra loi", "TI khong nha day"};
    dlogf("Gui: loi o byte %d/%d (bit %d): %s", idx + 2, n + 2, sendFailBit, why[sendFailStage]);
    traceDump();
  }
  return ok;
}

// ====== CHẾ ĐỘ AI ======
uint32_t lastWifiUse = 0;
const char* SYSTEM_PROMPT = "Reply in plain ASCII only. No markdown, no LaTeX. Max 600 characters.";

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // board Super Mini hay lỗi khi phát mạnh nhất
  WiFi.setSleep(true);
  dlogf("WiFi: dang ket noi '%s'...", WIFI_SSID);
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t > 15000) {
      int st = WiFi.status();
      dlogf("WiFi LOI, status = %d (%s)", st,
        st == WL_NO_SSID_AVAIL ? "khong thay ten WiFi" :
        st == WL_CONNECT_FAILED ? "sai mat khau / bi tu choi" : "het gio");
      return false;
    }
    delay(200);
  }
  // Độ mạnh sóng: -30..-67 tốt, -67..-80 dùng được, dưới -80 yếu
  dlogf("WiFi OK, RSSI = %d dBm", WiFi.RSSI());
  return true;
}

String sanitize(const String& in) {
  String out;
  for (size_t i = 0; i < in.length() && out.length() < MAX_A; i++) {
    char c = in[i];
    if (c == '\n' || (c >= 32 && c <= 126)) out += c;
  }
  return out;
}

// /wifi: quét mạng xung quanh rồi thử kết nối, để tìm lỗi WiFi
String wifiTest() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  delay(100);
  int n = WiFi.scanNetworks();
  String out = "Thay " + String(n) + " mang:\n";
  bool found = false;
  for (int i = 0; i < n && i < 10; i++) {
    out += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm, kenh " + String(WiFi.channel(i)) + ")\n";
    if (WiFi.SSID(i) == WIFI_SSID) found = true;
  }
  WiFi.scanDelete();
  if (!found) return out + "KHONG thay '" + String(WIFI_SSID) + "'. Kiem tra ten, bang tan 2.4GHz.";
  bool ok = ensureWiFi();
  return out + (ok ? "Ket noi OK, IP " + WiFi.localIP().toString()
                   : String("Thay mang nhung khong vao duoc: sai mat khau?"));
}

// ====== QUẢN LÝ MODEL (lưu trong flash, giữ qua tắt/mở) ======
Preferences prefs;
String currentModel;

void loadModel() {
  prefs.begin("ti89", true);
  currentModel = prefs.getString("model", CLAUDE_MODEL);
  prefs.end();
}

void saveModel(const String& m) {
  currentModel = m;
  prefs.begin("ti89", false);
  prefs.putString("model", m);
  prefs.end();
}

// Lấy danh sách model từ Anthropic (mới nhất đứng đầu). Không tốn token.
bool fetchModels(std::vector<String>& ids) {
  ids.clear();
  if (!ensureWiFi()) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(15000);
  http.begin(client, "https://api.anthropic.com/v1/models?limit=50");
  http.addHeader("x-api-key", CLAUDE_KEY);
  http.addHeader("anthropic-version", "2023-06-01");
  int code = http.GET();
  String resp = http.getString();
  http.end();
  lastWifiUse = millis();
  if (code != 200) { Serial.println("GET models: " + String(code) + " " + resp.substring(0, 300)); return false; }
  JsonDocument filter;
  filter["data"][0]["id"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, resp, DeserializationOption::Filter(filter))) return false;
  for (JsonObject m : doc["data"].as<JsonArray>()) ids.push_back(m["id"].as<String>());
  return !ids.empty();
}

// Model mới nhất có chứa "family" (vd "sonnet"), "" nếu không có
String newestOf(const std::vector<String>& ids, const String& family) {
  for (const String& id : ids) if (id.indexOf(family) >= 0) return id;
  return "";
}

String familyOf(const String& model) {
  const char* fams[] = {"fable", "opus", "sonnet", "haiku"};
  for (const char* f : fams) if (model.indexOf(f) >= 0) return f;
  return "";
}

bool bleUsed = false;        // đã bật Bluetooth từ lúc khởi động (dùng cho ngủ nhẹ)
String runPadDemo();         // khai báo trước, định nghĩa ở phần tay cầm
String unpairAll();
String wifiTest();
bool pendingRestart = false; // đặt true -> khởi động lại sau khi đã gửi trả lời

String helpText() {
  return
    "=== HUONG DAN ===\n"
    "Go cau hoi, ENTER de gui.\n"
    "/model        model dang dung\n"
    "/models       danh sach model\n"
    "/model sonnet doi sang Sonnet moi\n"
    "/model haiku  doi sang Haiku (re)\n"
    "/model opus   doi sang Opus (dat)\n"
    "/model <ten>  doi dung ten model\n"
    "/model reset  ve mac dinh\n"
    "/paddemo      tu bam thu cac nut\n"
    "/unpair       xoa may da ghep BT\n"
    "/wifi         kiem tra WiFi\n"
    "/log          xem nhat ky\n"
    "/help         huong dan nay\n"
    "Model bi ngung -> tu doi sang\n"
    "ban moi nhat cung dong.";
}

// Xử lý lệnh bắt đầu bằng '/'
String handleCommand(const String& raw) {
  String cmd = raw;
  cmd.trim();
  cmd.toLowerCase();
  if (cmd == "/help" || cmd == "/?") return helpText();
  if (cmd == "/model") return "Model: " + currentModel;
  if (cmd == "/paddemo") { bleUsed = USE_BLE; return runPadDemo(); }
  if (cmd == "/unpair") return unpairAll();
  if (cmd == "/wifi") return wifiTest();
  if (cmd == "/log") {   // TI-89 chỉ hiện được ~1000 ký tự: lấy phần cuối
    if (!dbg.length()) return "(nhat ky trong)";
    Serial.println(dbg);
    return dbg.length() > 900 ? dbg.substring(dbg.length() - 900) : dbg;
  }
  if (cmd == "/linetest") { int r = lineTest(); blinkResult(r); return "Ket qua do day: " + String(r) + " (xem /log)"; }
  if (cmd == "/logclear") { dbg = ""; dsave(); return "Da xoa nhat ky"; }
  if (cmd == "/models") {
    std::vector<String> ids;
    if (!fetchModels(ids)) return "Khong lay duoc danh sach (WiFi/key?)";
    String out = "Model hien co (moi nhat truoc):\n";
    for (size_t i = 0; i < ids.size() && i < 15; i++) {
      out += (ids[i] == currentModel ? "* " : "  ");
      out += ids[i] + "\n";
    }
    out += "(* = dang dung)";
    return out;
  }
  if (cmd.startsWith("/model ")) {
    String arg = cmd.substring(7);
    arg.trim();
    if (arg == "reset") { saveModel(CLAUDE_MODEL); return "Ve mac dinh: " + currentModel; }
    std::vector<String> ids;
    if (!fetchModels(ids)) return "Khong lay duoc danh sach (WiFi/key?)";
    String pick;
    for (const String& id : ids) if (id == arg) pick = id;      // khớp đúng tên
    if (pick == "") pick = newestOf(ids, arg);                  // hoặc mới nhất chứa từ khóa
    if (pick == "") return "Khong tim thay model: " + arg + "\nGo /models de xem danh sach";
    saveModel(pick);
    return "Da doi model: " + pick;
  }
  return "Lenh khong hop le. Go /help";
}

String askAIOnce(const String& q, int& code) {
  code = 0;
  if (!ensureWiFi()) return "No WiFi";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(30000);
  JsonDocument req;
  http.begin(client, "https://api.anthropic.com/v1/messages");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", CLAUDE_KEY);
  http.addHeader("anthropic-version", "2023-06-01");
  req["model"] = currentModel;
  req["max_tokens"] = 1024;   // đủ chỗ cho câu trả lời; độ dài thật do system prompt và MAX_A giới hạn
  req["system"] = SYSTEM_PROMPT;
  JsonArray msgs = req["messages"].to<JsonArray>();
  JsonObject usr = msgs.add<JsonObject>();
  usr["role"] = "user";
  usr["content"] = q;
  String body;
  serializeJson(req, body);
  code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (code != 200) { Serial.println(resp); dlogf("Claude: HTTP %d", code); return "HTTP error " + String(code); }
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return "JSON error";
  // Tìm khối "text" trong mọi khối nội dung (có thể có khối khác đứng trước)
  String a;
  for (JsonObject block : doc["content"].as<JsonArray>()) {
    if (block["type"] == "text") a += block["text"].as<const char*>();
  }
  if (a.length() == 0) {
    String reason = doc["stop_reason"] | "?";
    Serial.println("Khong co text. stop_reason = " + reason);
    Serial.println(resp.substring(0, 600));      // in một phần phản hồi để debug
    return "(empty: " + reason + ")";
  }
  return sanitize(a);
}

// Hỏi Claude; nếu model bị ngừng (404) thì tự đổi sang bản mới nhất cùng dòng và hỏi lại
String askAI(const String& q) {
  int code;
  String a = askAIOnce(q, code);
  if (code != 404) return a;
  String fam = familyOf(currentModel);
  std::vector<String> ids;
  if (fam == "" || !fetchModels(ids)) return a;
  String pick = newestOf(ids, fam);
  if (pick == "" || pick == currentModel) return a;
  String old = currentModel;
  saveModel(pick);
  Serial.println("Model " + old + " khong con, doi sang " + pick);
  a = askAIOnce(q, code);
  return "(Tu doi model: " + pick + ")\n" + a;
}

// Một chỗ xử lý chung cho câu hỏi từ TI-89 và từ Serial
String processQuestion(const String& q) {
  String t = q;
  t.trim();
  if (t.startsWith("/")) return handleCommand(t);   // lệnh chạy cả khi ECHO_TEST = 1
#if ECHO_TEST
  return "ECHO: " + q;
#else
  String a = askAI(q);
  lastWifiUse = millis();
  return a;
#endif
}

// ====== CHẾ ĐỘ TAY CẦM ======
uint8_t lastRows[KEY_ROWS];
#if USE_BLE
BleGamepad pad("TI-89 Gamepad", "DIY", 100);
bool padStarted = false;
#endif

// Bảng gán phím kiểu tay cầm Xbox: {hàng, bit, số nút}.
// D-pad = 4 phím mũi tên (hàng 0, bit 0..3), xử lý riêng trong handleKeys().
// Số nút theo thứ tự tay cầm Xbox chuẩn (1=A, 2=B, 3=X, 4=Y, 5=LB, 6=RB, 7=Back, 8=Start, 9=L3, 10=R3).
// Vị trí phím lấy theo tài liệu TIGCC — kiểm tra bằng keytest hoặc KEY_DEBUG; sai thì sửa số hàng/bit ở đây.
struct KeyMap { uint8_t row, bit, button; };
const KeyMap MAP[] = {
  {1, 0,  1},  // ENTER   -> A
  {6, 0,  2},  // ESC     -> B
  {0, 4,  3},  // 2nd     -> X
  {0, 6,  4},  // ◆       -> Y
  {5, 7,  5},  // F1      -> LB
  {1, 7,  6},  // F5      -> RB
  {4, 6,  7},  // MODE    -> Back / View
  {5, 6,  8},  // HOME    -> Start / Menu
  {0, 5,  9},  // shift   -> L3
  {0, 7, 10},  // alpha   -> R3
  {4, 7, 11},  // F2      -> LT (cò trái, bấm/nhả)
  {2, 7, 12},  // F4      -> RT (cò phải, bấm/nhả)
  {3, 7, 13},  // F3      -> Xbox / Guide
};
const int MAP_N = sizeof(MAP) / sizeof(MAP[0]);

const char* BTN_NAMES[] = {"", "A", "B", "X", "Y", "LB", "RB", "Back", "Start",
                           "L3", "R3", "LT", "RT", "Guide"};

// Mô tả bằng chữ những nút đang bấm (dùng để in ra Serial / giả lập khi không có Bluetooth)
String describeRows(const uint8_t* rows) {
  String s;
  const char* dir[] = {"UP", "LEFT", "DOWN", "RIGHT"};
  for (int b = 0; b < 4; b++) if (rows[0] & (1 << b)) { s += "DPAD_"; s += dir[b]; s += " "; }
  for (int i = 0; i < MAP_N; i++)
    if (rows[MAP[i].row] & (1 << MAP[i].bit)) { s += BTN_NAMES[MAP[i].button]; s += " "; }
  return s.length() ? s : String("(nha het)");
}

void printKeyDiff(const uint8_t* rows) {
#if KEY_DEBUG
  for (int r = 0; r < KEY_ROWS; r++) {
    uint8_t diff = rows[r] ^ lastRows[r];
    for (int b = 0; b < 8; b++)
      if (diff & (1 << b))
        Serial.printf("row %d bit %d %s\n", r, b, (rows[r] & (1 << b)) ? "BAM" : "nha");
  }
#endif
  memcpy(lastRows, rows, KEY_ROWS);
}

#if USE_BLE
void startPad() {
  if (padStarted) return;
  BleGamepadConfiguration cfg;
  cfg.setAutoReport(false);
  cfg.setButtonCount(16);
  cfg.setHatSwitchCount(1);
  // Không dùng cần analog: tắt hết trục, tránh PC thấy cần bị lệch
  cfg.setWhichAxes(false, false, false, false, false, false, false, false);
  pad.begin(&cfg);
  padStarted = true;
  Serial.println("Bluetooth gamepad: dang quang ba, hay ket noi tu PC/dien thoai");
}

void handleKeys(const uint8_t* rows) {
  printKeyDiff(rows);
#if KEY_DEBUG
  Serial.println("[PAD] " + describeRows(rows));
#endif
  if (!padStarted || !pad.isConnected()) return;

  bool up = rows[0] & 1, left = rows[0] & 2, down = rows[0] & 4, right = rows[0] & 8;
  uint8_t hat = HAT_CENTERED;
  if (up && right) hat = HAT_UP_RIGHT;
  else if (up && left) hat = HAT_UP_LEFT;
  else if (down && right) hat = HAT_DOWN_RIGHT;
  else if (down && left) hat = HAT_DOWN_LEFT;
  else if (up) hat = HAT_UP;
  else if (down) hat = HAT_DOWN;
  else if (left) hat = HAT_LEFT;
  else if (right) hat = HAT_RIGHT;
  pad.setHat1(hat);

  for (int i = 0; i < MAP_N; i++) {
    if (rows[MAP[i].row] & (1 << MAP[i].bit)) pad.press(MAP[i].button);
    else pad.release(MAP[i].button);
  }
  pad.sendReport();
}

void releasePad() {
  memset(lastRows, 0, sizeof(lastRows));
  if (!padStarted || !pad.isConnected()) return;
  pad.setHat1(HAT_CENTERED);
  for (int i = 0; i < MAP_N; i++) pad.release(MAP[i].button);
  pad.sendReport();
}
#else   // bản không Bluetooth: giả lập — in ra Serial những gì lẽ ra gửi qua Bluetooth
void startPad()   { Serial.println("USE_BLE = 0: gia lap tay cam, in nut ra Serial"); }
void handleKeys(const uint8_t* rows) {
  printKeyDiff(rows);
  Serial.println("[PAD gia lap] " + describeRows(rows));
}
void releasePad() { memset(lastRows, 0, sizeof(lastRows)); Serial.println("[PAD gia lap] (nha het)"); }
#endif

// /paddemo: tự bấm lần lượt từng nút qua đúng đường code handleKeys(), không cần TI-89.
// USE_BLE 1: gửi thật qua Bluetooth (xem bằng joy.cpl). USE_BLE 0: chỉ in ra Serial.
String runPadDemo() {
  startPad();
#if USE_BLE
  Serial.println("Cho PC ket noi 'TI-89 Gamepad' (toi da 60 giay)...");
  uint32_t t = millis();
  while (!pad.isConnected()) {
    if (millis() - t > 60000) return "Het gio: PC chua ket noi Bluetooth";
    delay(200);
  }
  delay(1000);
#endif
  uint8_t rows[KEY_ROWS];
  for (int i = 0; i < MAP_N + 4; i++) {
    memset(rows, 0, sizeof(rows));
    String name;
    if (i < MAP_N) { rows[MAP[i].row] |= (1 << MAP[i].bit); name = BTN_NAMES[MAP[i].button]; }
    else           { static const char* D[] = {"D-pad LEN","D-pad TRAI","D-pad XUONG","D-pad PHAI"};
                     rows[0] |= (1 << (i - MAP_N)); name = D[i - MAP_N]; }
    Serial.println("Demo: bam " + name);
    handleKeys(rows);
    delay(800);
    memset(rows, 0, sizeof(rows));
    handleKeys(rows);
    delay(300);
  }
  releasePad();
  return "Demo xong: " + String(MAP_N) + " nut + 4 huong D-pad";
}

// /unpair: xoá mọi máy đã ghép Bluetooth (lưu trong flash, rút pin không mất).
// Xoá xong ESP tự khởi động lại để Bluetooth sạch hẳn.
String unpairAll() {
#if USE_BLE
  if (padStarted) NimBLEDevice::deleteAllBonds();   // BLE đang chạy: xoá qua thư viện
  Preferences bt;
  if (bt.begin("nimble_bond", false)) { bt.clear(); bt.end(); }  // xoá thẳng vùng lưu
  pendingRestart = true;
  return "Da xoa may da ghep. ESP khoi dong lai.\n"
         "Nho Forget 'TI-89 Gamepad' tren may cu.";
#else
  return "USE_BLE = 0: khong co Bluetooth";
#endif
}

// ====== NGỦ NHẸ ======
uint32_t lastActivity = 0;   // lần cuối có câu hỏi / phím bấm

void goLightSleep() {
  // Bluetooth đã bật thì khởi động lại để tắt sạch (an toàn hơn tắt NimBLE giữa chừng).
  // Sau khi khởi động lại, ESP thức 30 giây rồi mới ngủ.
  if (bleUsed) {
    Serial.println("Ranh 10 phut: khoi dong lai de tat Bluetooth");
    Serial.flush();
    delay(50);
    safeRestart();
  }
  if (WiFi.getMode() != WIFI_OFF) { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
  releaseAll();

  // Dây đang ở mức thấp thì không ngủ kiểu chờ dây (sẽ bị đánh thức liên tục).
  // Thấp quá 2 giây mà không có dữ liệu → ngủ hẹn giờ 60 giây rồi kiểm tra lại.
  static uint32_t lowSince = 0;
  if (!isHigh(PIN_RED) || !isHigh(PIN_WHITE)) {
    if (!lowSince) lowSince = millis();
    if (millis() - lowSince < 2000) return;
    Serial.println("Day link bi giu thap: ngu hen gio 60s");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    return;
  }
  lowSince = 0;

  Serial.println("Ngu nhe...");
  Serial.flush();
  gpio_sleep_sel_dis((gpio_num_t)PIN_RED);      // giữ nguyên pull-up khi ngủ
  gpio_sleep_sel_dis((gpio_num_t)PIN_WHITE);
  gpio_wakeup_enable((gpio_num_t)PIN_RED, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)PIN_WHITE, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  esp_light_sleep_start();                      // CPU dừng ở đây, thức thì chạy tiếp

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  gpio_wakeup_disable((gpio_num_t)PIN_RED);
  gpio_wakeup_disable((gpio_num_t)PIN_WHITE);
  // TI-89 vẫn đang giữ dây chờ xác nhận → vòng loop() kế tiếp đọc được ngay byte đó.
  // Nếu bị đánh thức nhầm, loop() không nhận được gì và sẽ ngủ lại.
}

// ====== MAIN ======
void setup() {
  Serial.begin(115200);
  releaseAll();                                  // cấu hình chân trước...
  gpio_hold_dis((gpio_num_t)PIN_RED);            // ...rồi mới bỏ giữ -> dây không bị giật
  gpio_hold_dis((gpio_num_t)PIN_WHITE);
#if CONFIG_IDF_TARGET_ESP32S3
  neopixelWrite(48, 0, 0, 0);   // Super Mini: tắt LED RGB (GPIO48) cho đỡ tốn pin
#endif
#if LED_DEBUG
  for (int i = 0; i < 3; i++) { led(0, 40, 0, 0); delay(200); neopixelWrite(48, 0, 0, 0); delay(200); }  // còn sống
#endif
  setCpuFrequencyMhz(80);   // đủ cho link + WiFi/BLE, đỡ tốn pin (bỏ dòng này nếu link lỗi)
  WiFi.mode(WIFI_OFF);      // WiFi chỉ bật khi có câu hỏi AI
  memset(lastRows, 0, sizeof(lastRows));
  lastActivity = millis();
  loadModel();
  dload();
  dlogf("--- Khoi dong (ly do reset %d), ECHO_TEST=%d, SWAP=%d ---", (int)esp_reset_reason(), ECHO_TEST, SWAP_TIP_RING);
  Serial.println("TI-89 combo ready. Model: " + currentModel);
}

void loop() {
  static uint8_t buf[MAX_Q + 2];
  int n = readFrame(buf, sizeof(buf));
  if (traceDumpPending) {             // ghi vết khung lỗi (và khung OK đầu tiên để so sánh)
    static bool dumpedOk = false;
    if (n == -3 || (n > 0 && !dumpedOk)) { traceDump(); if (n > 0) dumpedOk = true; }
    traceDumpPending = false;
  }
  if (n > 0) led(0, 0, 40);          // xanh dương: nhận khung OK
  else if (n == -2) led(40, 0, 0, 400);  // đỏ: lỗi giữa chừng
  else if (n == -3) {                    // tím: TI không thấy ESP trả lời -> đo từng dây
    led(40, 0, 40, 0); delay(400); neopixelWrite(48, 0, 0, 0); ledOffAt = 0;
    blinkResult(lineTest());
  }
#if LED_DEBUG
  {  // dây bị kéo xuống liên tục > 1 giây -> vàng
    static uint32_t lowSince = 0;
    if (!isHigh(PIN_RED) || !isHigh(PIN_WHITE)) {
      if (!lowSince) lowSince = millis();
      else if (millis() - lowSince > 1000) {
        static uint32_t lastMsg = 0;
        led(40, 30, 0, 0);
        if (millis() - lastMsg > 2000) {
          lastMsg = millis();
          if (!lastMsg) dlogf("Link: day bi keo xuong lien tuc (do=%d trang=%d)", isHigh(PIN_RED), isHigh(PIN_WHITE));
          else Serial.printf("Link: day bi keo xuong lien tuc (do=%d trang=%d)\n", isHigh(PIN_RED), isHigh(PIN_WHITE));
        }
      }
    } else if (lowSince) { lowSince = 0; led(0, 0, 0, 1); }
  }
  if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) { neopixelWrite(48, 0, 0, 0); ledOffAt = 0; }
#endif
  if (n > 0) {
    lastActivity = millis();
    switch (buf[0]) {
      case 'Q': {
        String q;
        for (int i = 1; i < n; i++) q += (char)buf[i];
        dlog("Nhan cau hoi: " + q.substring(0, 60));
        String a = processQuestion(q);
        dlog("Tra loi (" + String((int)a.length()) + " ky tu): " + a.substring(0, 60));
        if (WiFi.getMode() != WIFI_OFF) {        // tắt WiFi trước khi gửi để sóng không chen vào giao thức link
          WiFi.disconnect(true); WiFi.mode(WIFI_OFF); delay(50);
        }
        delay(20);
        if (!sendFrame(a)) { dlog("Gui tra loi cho TI: LOI"); led(40, 0, 0, 400); }
        else dlog("Gui tra loi cho TI: OK");
        break;
      }
      case 'P': dlog("Nhan lenh pad"); startPad(); bleUsed = USE_BLE; memset(lastRows, 0, sizeof(lastRows)); break;
      case 'K': if (n >= 1 + KEY_ROWS) handleKeys(buf + 1); break;
      case 'X': releasePad(); break;
    }
  }
#if SERIAL_TEST
  if (Serial.available()) {
    String q = Serial.readStringUntil('\n');
    q.trim();
    if (q.length()) {
      lastActivity = millis();
      Serial.println("[Serial] Q: " + q);
      String a = processQuestion(q);
      Serial.println("[Serial] A: " + a);
    }
  }
#endif
  if (WiFi.getMode() != WIFI_OFF && millis() - lastWifiUse > WIFI_IDLE_MS) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  if (pendingRestart) {       // chờ TI-89 nhận xong câu trả lời rồi mới khởi động lại
    Serial.println("Khoi dong lai...");
    delay(500);
    safeRestart();
  }
#if USE_SLEEP
  if (millis() > BOOT_AWAKE_MS && millis() - lastActivity > IDLE_SLEEP_MS) goLightSleep();
#endif
}
