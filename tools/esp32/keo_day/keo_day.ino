// keo_day.ino — Test chiều ESP -> TI-89: ESP lần lượt kéo từng dây xuống 0V.
// Dùng cùng chương trình linestat trên TI-89 (hiện trạng thái 2 dây).
//   LED XANH LÁ  = ESP đang kéo pad 4 (Tip)  xuống
//   LED XANH DƯƠNG = ESP đang kéo pad 5 (Ring) xuống
//   LED tắt      = ESP thả cả hai
const int PAD4 = 4, PAD5 = 5;

void releaseBoth() { pinMode(PAD4, INPUT_PULLUP); pinMode(PAD5, INPUT_PULLUP); }
void pull(int p)   { pinMode(p, OUTPUT); digitalWrite(p, LOW); }

void setup() {
  Serial.begin(115200);
  releaseBoth();
  for (int i = 0; i < 3; i++) { neopixelWrite(48, 30, 30, 0); delay(150); neopixelWrite(48, 0, 0, 0); delay(150); }
}

void loop() {
  pull(PAD4);   neopixelWrite(48, 0, 40, 0); Serial.println("Keo TIP (pad 4)");  delay(1500);
  releaseBoth(); neopixelWrite(48, 0, 0, 0);  Serial.println("Tha");              delay(1500);
  pull(PAD5);   neopixelWrite(48, 0, 0, 40); Serial.println("Keo RING (pad 5)"); delay(1500);
  releaseBoth(); neopixelWrite(48, 0, 0, 0);  Serial.println("Tha");              delay(1500);
}
