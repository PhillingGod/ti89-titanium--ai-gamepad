// do_chan.ino — Biến ESP32-S3 thành máy dò chân Tip/Ring (không cần đồng hồ đo).
// Dùng cùng chương trình ringfind trên TI-89. Không cần WiFi, không cần thư viện.
//
// LED (theo dây ở pad 5 = dây XANH, dây đang dò):
//   XANH DƯƠNG nháy theo nhịp = đang chạm đúng dây TI đang nháy
//   ĐỎ sáng liền              = đang chạm GND (sai chỗ)
//   tắt                        = không có tín hiệu
// Pad 4 (dây vàng) cũng được theo dõi: nháy XANH LÁ = dây vàng đang nhận nhịp.

const int PAD4 = 4, PAD5 = 5;

struct Watch { int pin; int falls; int lowMs; bool last; };
Watch w4 = {PAD4, 0, 0, true}, w5 = {PAD5, 0, 0, true};

void sample(Watch& w) {
  bool h = digitalRead(w.pin);
  if (!h) w.lowMs += 5;
  if (w.last && !h) w.falls++;
  w.last = h;
}

// 0 = không có gì, 1 = có nhịp (đúng dây), 2 = thấp liên tục (GND)
int classify(const Watch& w) {
  if (w.lowMs >= 1450) return 2;
  if (w.falls >= 1 && w.lowMs > 0) return 1;
  return 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(PAD4, INPUT_PULLUP);
  pinMode(PAD5, INPUT_PULLUP);
  for (int i = 0; i < 3; i++) { neopixelWrite(48, 30, 0, 30); delay(150); neopixelWrite(48, 0, 0, 0); delay(150); }
  Serial.println("do_chan: san sang. Chay ringfind tren TI-89.");
}

void loop() {
  w4.falls = w5.falls = 0; w4.lowMs = w5.lowMs = 0;
  uint32_t t = millis();
  while (millis() - t < 1500) {           // quan sát 1.5 giây (≈3 nhịp)
    sample(w4); sample(w5);
    bool l4 = !w4.last, l5 = !w5.last;
    // LED phản ánh trực tiếp trạng thái dây để thấy được nhịp
    if (l5)      neopixelWrite(48, 0, 0, 40);   // dây xanh đang bị kéo thấp
    else if (l4) neopixelWrite(48, 0, 40, 0);   // dây vàng đang bị kéo thấp
    else         neopixelWrite(48, 0, 0, 0);
    delay(5);
  }
  int c4 = classify(w4), c5 = classify(w5);
  if (c5 == 2) neopixelWrite(48, 40, 0, 0);     // dây xanh chạm GND -> đỏ
  const char* txt[] = {"khong co tin hieu", "CO NHIP (dung day TI dang nhay)", "THAP LIEN TUC (GND?)"};
  Serial.printf("pad4 (vang): %s | pad5 (xanh): %s\n", txt[c4], txt[c5]);
}
