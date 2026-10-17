// gpt.c — Chương trình cho TI-89, biên dịch bằng TIGCC / GCC4TI
// Cách dùng trên máy: gpt("cau hoi cua ban")
// Gửi khung [lenLo][lenHi]['Q'][câu hỏi]; ESP32 trả lời [lenLo][lenHi][text].

#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 100
#include <tigcclib.h>

#ifndef OFFLINE_TEST
#define OFFLINE_TEST 0   // 1 = không dùng link, hiện câu trả lời giả (để test trên TiEmu)
#endif

#define MAXQ 250

// Chờ t tick (20 tick = 1 giây). Dùng sau OSLinkReset: reset có thể làm dây link chớp xuống thấp,
// đợi một chút để ESP không lẫn tín hiệu reset với dữ liệu.
static void waitTicks(short t) {
  OSFreeTimer(USER_TIMER);
  OSRegisterTimer(USER_TIMER, t);
  while (!OSTimerExpired(USER_TIMER)) ;
  OSFreeTimer(USER_TIMER);
}
#define MAXA 1000
#define COLS 40     // font 4x6: 40 cột
#define ROWS 16     //           16 dòng

static short escPressed(void) {
  return kbhit() && ngetchx() == KEY_ESC;
}

static short writeAll(const char *p, unsigned short n) {
  while (n) {
    unsigned short c = n > 32 ? 32 : n;
    while (OSWriteLinkBlock(p, c)) {        // khác 0 = hàng đợi đầy, thử lại
      if (escPressed()) return 0;
    }
    p += c; n -= c;
  }
  return 1;
}

static short readExact(char *buf, unsigned short n) {
  unsigned short got = 0;
  while (got < n) {
    got += OSReadLinkBlock(buf + got, n - got);
    if (escPressed()) return 0;
  }
  return 1;
}

static void showPaged(const char *s) {
  short col = 0, row = 0;
  clrscr();
  FontSetSys(F_4x6);
  for (; *s; s++) {
    if (*s == '\n' || col == COLS) { putchar('\n'); col = 0; row++; if (*s == '\n') continue; }
    if (row == ROWS) {
      ST_helpMsg("Nhan phim de xem tiep");
      ngetchx();
      clrscr(); row = 0; col = 0;
    }
    putchar(*s); col++;
  }
  ST_helpMsg("Het. Nhan phim de thoat");
  ngetchx();
}

// Màn hình hướng dẫn gõ phím (F1) — phần này không đổi theo firmware
static void showKeyHelp(void) {
  clrscr();
  FontSetSys(F_6x8);
  DrawStr(0, 0,  "HUONG DAN GO PHIM", A_NORMAL);
  DrawStr(0, 12, "2nd+alpha: khoa chu", A_NORMAL);
  DrawStr(0, 22, "alpha    : tat khoa chu", A_NORMAL);
  DrawStr(0, 32, "<-       : xoa 1 ky tu", A_NORMAL);
  DrawStr(0, 42, "ENTER    : gui", A_NORMAL);
  DrawStr(0, 52, "ESC      : huy", A_NORMAL);
  DrawStr(0, 62, "/ (phim chia): lenh", A_NORMAL);
  DrawStr(0, 72, "/help: danh sach lenh", A_NORMAL);
  DrawStr(0, 86, "Nhan phim bat ky...", A_NORMAL);
  ngetchx();
}

static void drawInputHeader(void) {
  clrscr();
  FontSetSys(F_6x8);
  DrawStr(0, 0,  "Nhap cau hoi:", A_NORMAL);
  DrawStr(0, 10, "ENTER = gui   ESC = huy", A_NORMAL);
  DrawStr(0, 20, "(2nd+alpha = khoa chu)", A_NORMAL);
  DrawStr(0, 30, "/help=lenh  F1=go phim", A_NORMAL);
}

// Nhập câu hỏi trực tiếp trên máy (không cần dấu "). Trả về độ dài, -1 nếu nhấn ESC.
static short readLine(char *buf, short max) {
  short n = 0, key, start;
  char view[27];
  drawInputHeader();
  buf[0] = 0;
  while (1) {
    start = n > 25 ? n - 25 : 0;              // chỉ hiện 25 ký tự cuối
    sprintf(view, "%-26s", buf + start);
    DrawStr(0, 44, view, A_REPLACE);
    key = ngetchx();
    if (key == KEY_ENTER) break;
    if (key == KEY_ESC) return -1;
    if (key == KEY_F1) { showKeyHelp(); drawInputHeader(); continue; }
    if (key == KEY_BACKSPACE) { if (n) buf[--n] = 0; continue; }
    if (key >= 32 && key <= 126 && n < max) { buf[n++] = (char)key; buf[n] = 0; }
  }
  return n;
}

void _main(void) {
  static char ans[MAXA + 1];
  unsigned char hdr[2];
  unsigned short len, alen;
  const char *q;
  ESI ap;

  static char typed[MAXQ + 1];

  InitArgPtr(ap);
  if (GetArgType(ap) == STR_TAG) {
    q = GetStrnArg(ap);                      // gpt("cau hoi") vẫn dùng được
  } else {
    if (readLine(typed, MAXQ) <= 0) { ST_helpMsg("Da huy"); return; }
    q = typed;                               // gpt() -> nhập trực tiếp
  }
  len = strlen(q);
  if (len > MAXQ) len = MAXQ;

#if OFFLINE_TEST
  // Không dùng link: tạo câu trả lời giả dài để test hiển thị và chia trang
  {
    short i;
    ans[0] = 0;
    strcat(ans, "OFFLINE TEST. Q: ");
    strncat(ans, q, 100);
    strcat(ans, "\n");
    for (i = 0; i < 12; i++) {
      char line[60];
      sprintf(line, "Dong %d: the quick brown fox jumps over the lazy dog.\n", i + 1);
      strcat(ans, line);
    }
  }
  showPaged(ans);
  return;
#endif

  clrscr();
  printf("Dang gui...\n");
  // Thứ tự quan trọng: OSLinkReset() còn ĐÓNG link (OSLinkClose) -> phải reset trước rồi mới mở.
  // (Bản cũ mở rồi reset => link bị đóng, TI vẫn gửi được nhưng KHÔNG nhận được gì.)
  OSLinkReset();
  waitTicks(2);                              // ~0.1 s cho dây ổn định sau reset
  OSLinkOpen();

  hdr[0] = (len + 1) & 0xFF; hdr[1] = (len + 1) >> 8;
  if (!writeAll((char *)hdr, 2) || !writeAll("Q", 1) || !writeAll(q, len)) goto done;

  printf("Dang cho tra loi (ESC=huy)\n");
  if (!readExact((char *)hdr, 2)) goto done;
  alen = hdr[0] | (hdr[1] << 8);
  if (alen > MAXA) alen = MAXA;
  {
    // Nhận nội dung, hiện tiến độ để biết dữ liệu có về tới máy không
    char st[40];
    unsigned short got = 0, shown = 0xFFFF;
    sprintf(st, "Do dai: %u ky tu", alen);
    DrawStr(0, 40, st, A_REPLACE);
    while (got < alen) {
      got += OSReadLinkBlock(ans + got, alen - got);
      if (got != shown) {
        sprintf(st, "Da nhan: %u/%u   ", got, alen);
        DrawStr(0, 50, st, A_REPLACE);
        shown = got;
      }
      if (escPressed()) {
        OSLinkClose();
        sprintf(st, "Huy: nhan %u/%u", got, alen);
        ST_helpMsg(st);
        return;
      }
    }
  }
  ans[alen] = 0;

  OSLinkClose();
  showPaged(ans);
  return;

done:
  OSLinkClose();
  ST_helpMsg("Da huy / loi link");
}
