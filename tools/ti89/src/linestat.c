// linestat.c — TI-89: hiện trạng thái 2 dây link đọc trực tiếp từ phần cứng (cổng 0x60000E).
// Dùng để kiểm tra ESP có kéo được dây xuống không. ESC = thoát. Rút cáp USB trước khi chạy.
#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 100
#include <tigcclib.h>

void _main(void) {
  volatile unsigned char *port = (volatile unsigned char *)0x60000E;
  unsigned char v, last = 0xFF;
  char s[40];
  clrscr();
  FontSetSys(F_6x8);
  DrawStr(0, 0,  "LINESTAT  (ESC = thoat)", A_NORMAL);
  DrawStr(0, 10, "Doc truc tiep cong link", A_NORMAL);
  while (1) {
    if (kbhit() && ngetchx() == KEY_ESC) break;
    v = *port;
    if (v != last) {
      sprintf(s, "0x60000E = %02X       ", v);
      DrawStr(0, 30, s, A_REPLACE);
      sprintf(s, "bit2 = %d   bit3 = %d   ", (v >> 2) & 1, (v >> 3) & 1);
      DrawStr(0, 44, s, A_REPLACE);
      sprintf(s, "Tip  : %s      ", (v & 0x04) ? "CAO (tha)" : "THAP (keo)");
      DrawStr(0, 60, s, A_REPLACE);
      sprintf(s, "Ring : %s      ", (v & 0x08) ? "CAO (tha)" : "THAP (keo)");
      DrawStr(0, 72, s, A_REPLACE);
      last = v;
    }
  }
  GKeyFlush();
}
