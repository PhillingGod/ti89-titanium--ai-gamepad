// pad.c — TI-89 làm tay cầm Bluetooth (qua module ESP32). Biên dịch bằng GCC4TI/TIGCC.
// Cách dùng: chạy pad() trên Home. Thoát: ON hoặc ESC+CLEAR.
// Gửi khung ['K'][10 byte hàng phím] mỗi khi trạng thái phím thay đổi.

#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 100
#include <tigcclib.h>

#define ROWS 10

// Chờ t tick (20 tick = 1 giây). Dùng sau OSLinkReset: reset có thể làm dây link chớp xuống thấp,
// đợi một chút để ESP không lẫn tín hiệu reset với dữ liệu.
static void waitTicks(short t) {
  OSFreeTimer(USER_TIMER);
  OSRegisterTimer(USER_TIMER, t);
  while (!OSTimerExpired(USER_TIMER)) ;
  OSFreeTimer(USER_TIMER);
}
#define MAX_TRIES 3000   // số lần thử gửi trước khi bỏ (không có ESP thì khỏi treo)

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

static short writeAll(const unsigned char *p, unsigned short n) {
  while (n) {
    unsigned short c = n > 32 ? 32 : n, tries = 0;
    while (OSWriteLinkBlock((const char *)p, c)) {   // khác 0 = hàng đợi đầy, thử lại
      if (exitPressed() || ++tries > MAX_TRIES) return 0;
    }
    p += c; n -= c;
  }
  return 1;
}

static short sendFrame(const unsigned char *p, unsigned short n) {
  unsigned char hdr[2];
  hdr[0] = n & 0xFF; hdr[1] = n >> 8;
  return writeAll(hdr, 2) && writeAll(p, n);
}

void _main(void) {
  unsigned char cur[ROWS + 1], prev[ROWS + 1], cmd;
  short r;
  INT_HANDLER save1 = GetIntVec(AUTO_INT_1);
  INT_HANDLER save5 = GetIntVec(AUTO_INT_5);

  clrscr();
  printf("TI-89 Gamepad (Bluetooth)\n");
  printf("Mui ten: D-pad\n");
  printf("ENTER=A ESC=B 2nd=X <>=Y\n");
  printf("F1=LB F5=RB F2=LT F4=RT\n");
  printf("HOME=Start MODE=Back\n");
  printf("F3=Guide shift=L3 alpha=R3\n");
  printf("ON hoac ESC+CLEAR = thoat\n");

  // Thứ tự quan trọng: OSLinkReset() còn ĐÓNG link (OSLinkClose) -> phải reset trước rồi mới mở.
  // (Bản cũ mở rồi reset => link bị đóng, TI vẫn gửi được nhưng KHÔNG nhận được gì.)
  OSLinkReset();
  waitTicks(2);                              // ~0.1 s cho dây ổn định sau reset
  OSLinkOpen();
  cmd = 'P';
  sendFrame(&cmd, 1);

  // Tắt xử lý bàn phím của hệ điều hành để đọc ma trận phím trực tiếp
  SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
  SetIntVec(AUTO_INT_5, DUMMY_HANDLER);

  memset(prev, 0, sizeof(prev));
  cur[0] = 'K';
  while (!exitPressed()) {
    for (r = 0; r < ROWS; r++)
      cur[r + 1] = _rowread(~((unsigned short)(1 << r)));
    if (memcmp(cur + 1, prev + 1, ROWS)) {
      if (!sendFrame(cur, ROWS + 1)) {
        if (exitPressed()) break;              // người dùng thoát giữa lúc gửi
        DrawStr(0, 90, "Khong gui duoc (ESP?)", A_REPLACE);
      }
      memcpy(prev, cur, ROWS + 1);
    }
  }

  waitRelease();                               // nhả phím rồi mới gửi 'X' và trả bàn phím cho OS
  cmd = 'X';
  sendFrame(&cmd, 1);

  SetIntVec(AUTO_INT_1, save1);
  SetIntVec(AUTO_INT_5, save5);
  OSLinkClose();
  GKeyFlush();
}
