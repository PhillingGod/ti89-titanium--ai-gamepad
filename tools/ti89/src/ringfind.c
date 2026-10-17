// ringfind.c — TI-89: liên tục "nháy" dây Tip hoặc Ring của cổng link, để ESP dò chân hàn.
// Phím 1 = nháy Tip, 2 = nháy Ring, ESC = thoát. Rút cáp USB trước khi chạy.

#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 100
#include <tigcclib.h>

static void waitTicks(short t) {          // 20 tick = 1 giây
  OSFreeTimer(USER_TIMER);
  OSRegisterTimer(USER_TIMER, t);
  while (!OSTimerExpired(USER_TIMER)) ;
  OSFreeTimer(USER_TIMER);
}

static void drawScreen(short mode) {
  clrscr();
  FontSetSys(F_6x8);
  DrawStr(0, 0,  "DO CHAN LINK (ringfind)", A_NORMAL);
  DrawStr(0, 14, mode == 1 ? "Dang nhay: TIP  (dau nhon)" : "Dang nhay: RING (khoanh giua)", A_NORMAL);
  DrawStr(0, 30, "1 = nhay Tip", A_NORMAL);
  DrawStr(0, 40, "2 = nhay Ring", A_NORMAL);
  DrawStr(0, 50, "ESC = thoat", A_NORMAL);
  DrawStr(0, 66, "Xem LED tren ESP", A_NORMAL);
  DrawStr(0, 76, "(nhay ESP do_chan.ino)", A_NORMAL);
}

void _main(void) {
  short mode = 2, k;
  unsigned char b;

  drawScreen(mode);
  OSLinkOpen();
  while (1) {
    if (kbhit()) {
      k = ngetchx();
      if (k == KEY_ESC) break;
      if (k == '1') { mode = 1; drawScreen(mode); }
      if (k == '2') { mode = 2; drawScreen(mode); }
    }
    // Byte 0x00: bit đầu = 0 -> kéo Tip. Byte 0xFF: bit đầu = 1 -> kéo Ring.
    // ESP ở chế độ dò KHÔNG trả lời, nên TI giữ dây thấp cho đến khi reset.
    b = (mode == 1) ? 0x00 : 0xFF;
    OSLinkReset();
    OSWriteLinkBlock((const char *)&b, 1);
    waitTicks(6);          // giữ thấp ~0.3 s
    OSLinkReset();         // nhả dây
    waitTicks(4);          // thả ~0.2 s
  }
  OSLinkReset();
  OSLinkClose();
  GKeyFlush();
}
