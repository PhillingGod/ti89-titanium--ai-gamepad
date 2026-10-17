// keytest.c — Hiển thị ma trận phím TI-89 (_rowread) lên màn hình, KHÔNG cần ESP.
// Dùng để kiểm tra bảng gán phím trước khi làm tay cầm. Thoát: ON hoặc ESC+CLEAR.
// Mỗi dòng: "R<hàng>: <8 bit, bit7 ... bit0>", '1' = đang bấm.

#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 100
#include <tigcclib.h>

#define ROWS 10

// Thoát: ON (đọc thẳng cổng phần cứng 0x60001A bit 1, 0 = đang nhấn),
// hoặc ESC + CLEAR cùng lúc (dự phòng chắc chắn chạy).
// Không dùng OSCheckBreak vì AMS tắt phím Break khi chạy chương trình ASM.
#define ON_PRESSED() (!(*(volatile unsigned char *)0x60001A & 0x02))
static short exitPressed(void) {
  return ON_PRESSED() ||
         ((_rowread(~(1 << 6)) & 0x01) && (_rowread(~(1 << 1)) & 0x40));   // ESC (R6b0) + CLEAR (R1b6)
}
static void waitRelease(void) {
  while (exitPressed()) ;
}

void _main(void) {
  unsigned char cur[ROWS], prev[ROWS];
  short r, b;
  char line[20];
  INT_HANDLER save1 = GetIntVec(AUTO_INT_1);
  INT_HANDLER save5 = GetIntVec(AUTO_INT_5);

  ClrScr();
  FontSetSys(F_6x8);
  DrawStr(0, 0, "KEYTEST ON/ESC+CLEAR=thoat", A_NORMAL);
  memset(prev, 0xFF, sizeof(prev));        // ép vẽ lần đầu

  SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
  SetIntVec(AUTO_INT_5, DUMMY_HANDLER);
  while (!exitPressed()) {
    for (r = 0; r < ROWS; r++) {
      cur[r] = _rowread(~((unsigned short)(1 << r)));
      if (cur[r] != prev[r]) {
        sprintf(line, "R%d: ", r);
        for (b = 7; b >= 0; b--) {
          char s[2] = { (cur[r] & (1 << b)) ? '1' : '0', 0 };
          strcat(line, s);
        }
        DrawStr(0, 12 + r * 8, line, A_REPLACE);
        prev[r] = cur[r];
      }
    }
  }

  waitRelease();
  SetIntVec(AUTO_INT_1, save1);
  SetIntVec(AUTO_INT_5, save5);
  GKeyFlush();
}
