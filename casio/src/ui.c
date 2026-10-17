// ui.c - menu, o nhap, man hinh ket qua nhieu trang
#include "casio.h"

short scroll;

/* ---------------- menu chon ---------------- */
static char keyLabel(short i) { return i < 9 ? '1' + i : i == 9 ? '0' : 'A' + (i - 10); }

short menuSel(const char *title, const char *const *items, short n, short start) {
  short sel = start, top = 0, i, k;
  const short VIS = 10;
  for (;;) {
    if (sel < top) top = sel;
    if (sel >= top + VIS) top = sel - VIS + 1;
    beginDraw();
    setClip(0, 0, 159, 99);
    fillRect(0, 0, 159, 7, A_NORMAL);
    setFont(1);
    DrawStr(2, 1, title, A_REVERSE);
    FontSetSys(F_6x8); curFont = F_6x8;
    for (i = 0; i < VIS && top + i < n; i++) {
      char line[40];
      short y = 9 + i * 9;
      sprintf(line, "%c:%s", keyLabel(top + i), items[top + i]);
      line[26] = 0;
      DrawStr(2, y, line, A_NORMAL);
      if (top + i == sel) fillRect(0, y - 1, 159, y + 7, A_XOR);
    }
    if (top > 0) { setFont(1); DrawStr(153, 9, "\x18", A_NORMAL); }
    if (top + VIS < n) { setFont(1); DrawStr(153, 92, "\x19", A_NORMAL); }
    endDraw();
    k = ngetchx();
    if (k == KEY_ESC || k == KEY_CLEAR) return -1;
    if (k == KEY_UP) sel = (sel + n - 1) % n;
    else if (k == KEY_DOWN) sel = (sel + 1) % n;
    else if (k == KEY_LEFT) sel = sel >= VIS ? sel - VIS : 0;
    else if (k == KEY_RIGHT) sel = sel + VIS < n ? sel + VIS : n - 1;
    else if (k == KEY_ENTER) return sel;
    else if (k >= '1' && k <= '9' && k - '1' < n) return k - '1';
    else if (k == '0' && n > 9) return 9;
    else if (k >= 'a' && k <= 'z' && 10 + k - 'a' < n) return 10 + k - 'a';
    else if (k >= 'A' && k <= 'Z' && 10 + k - 'A' < n) return 10 + k - 'A';
  }
}

void notice(const char *l1, const char *l2) {
  short w = 150;
  setClip(0, 0, 159, 99);
  fillRect(4, 34, 4 + w, 64, A_REVERSE);
  DrawLine(4, 34, 4 + w, 34, A_NORMAL); DrawLine(4, 64, 4 + w, 64, A_NORMAL);
  DrawLine(4, 34, 4, 64, A_NORMAL); DrawLine(4 + w, 34, 4 + w, 64, A_NORMAL);
  FontSetSys(F_6x8); curFont = F_6x8;
  DrawStr(8, 38, l1, A_NORMAL);
  setFont(1);
  if (l2) DrawStr(8, 50, l2, A_NORMAL);
}
void message(const char *l1, const char *l2) {
  notice(l1, l2);
  setFont(1);
  DrawStr(8, 57, "(bam phim bat ky)", A_NORMAL);
  ngetchx();
}

/* ---------------- cac menu chen ---------------- */
static const char *const M_TPL[] = { "a/b phan so", "hon so", "can bac 2", "can bac 3", "can bac n", "x^2", "x^3",
                                     "x^-1", "x^n", "10^x", "e^x", "|x| (Abs)", "log_a(b)" };
static const char *const M_FUN[] = { "sin", "cos", "tan", "sin-1", "cos-1", "tan-1", "sinh", "cosh", "tanh",
                                     "sinh-1", "cosh-1", "tanh-1", "ln", "log" };
static const char *const M_CALC[] = { "d/dx dao ham", "tich phan", "Sigma tong", "Pi tich", "x! giai thua", "nPr", "nCr",
                                      "%", "chia lay du (R)", "Pol(", "Rec(", "GCD( UCLN", "LCM( BCNN", "Int( phan nguyen",
                                      "Intg( lam tron xuong", "Ran# so ngau nhien", "RanInt#(", "do phut giay",
                                      "solve( giai (CAS)", "factor( phan tich", "expand( khai trien" };
static const char *const M_CONST[] = { "pi", "e (co so ln)", "i (so phuc)", "Hang so vat ly...", "Doi don vi...",
                                       "Ans", "PreAns", "Conjg( lien hop", "Arg( acgumen", "ReP( phan thuc", "ImP( phan ao" };
extern const char *const CONST_NAME[];
extern const char *const CONV_NAME[];
static const char *const CONST_DESC[NCONST] = {
  "mp k.luong proton", "mn k.luong notron", "me k.luong electron", "mmu k.luong muon", "a0 ban kinh Bohr",
  "h hang so Planck", "uN magneton hat nhan", "uB magneton Bohr", "hbar Planck rut gon", "alpha cau truc tinh te",
  "re ban kinh electron", "lc buoc song Compton", "gp ti so tu hoi proton", "lcp Compton proton", "lcn Compton notron",
  "Rinf hang so Rydberg", "u don vi k.luong ng.tu", "up mo men tu proton", "ue mo men tu electron", "un mo men tu notron",
  "umu mo men tu muon", "F hang so Faraday", "e dien tich nguyen to", "NA so Avogadro", "k hang so Boltzmann",
  "Vm the tich mol khi", "R hang so khi", "c0 toc do anh sang", "c1 hang so buc xa 1", "c2 hang so buc xa 2",
  "sigma Stefan-Boltzmann", "e0 hang so dien", "u0 hang so tu", "phi0 luong tu tu thong", "g gia toc trong truong",
  "G0 luong tu dan dien", "Z0 tro khang chan khong", "t nhiet do Celsius", "G hang so hap dan", "atm ap suat chuan" };

void constMenu(void) {
  short i = menuSel("HANG SO VAT LY", CONST_DESC, NCONST, 0);
  if (i >= 0) insTok(TK_CONST0 + i);
}
void convMenu(void) {
  short i = menuSel("DOI DON VI (sau so)", CONV_NAME, NCONV, 0);
  if (i >= 0) insCapture(IT_CONV, i);
}

static void doTpl(short i) {
  switch (i) {
    case 0: insCapture(IT_FRAC, 0); break;
    case 1: insTpl(IT_MIXED, 0); break;
    case 2: insTpl(IT_SQRT, 0); break;
    case 3: insRootN("3"); break;
    case 4: insTpl(IT_NROOT, 0); break;
    case 5: insPowText("2"); break;
    case 6: insPowText("3"); break;
    case 7: { char m[3]; m[0] = (char)CH_NEG; m[1] = '1'; m[2] = 0; insPowText(m); break; }
    case 8: insTpl(IT_POW, 0); break;
    case 9: insTen(); break;
    case 10: insExp(); break;
    case 11: insTpl(IT_ABS, 0); break;
    case 12: insTpl(IT_LOGB, 0); break;
  }
}
static void doFun(short i) {
  static const unsigned char T[14] = { TK_SIN, TK_COS, TK_TAN, TK_ASIN, TK_ACOS, TK_ATAN, TK_SINH, TK_COSH, TK_TANH,
                                       TK_ASINH, TK_ACOSH, TK_ATANH, TK_LN, TK_LOG };
  if (i >= 0 && i < 14) insTok(T[i]);
}
static void doCalc(short i) {
  switch (i) {
    case 0: insTpl(IT_DERIV, 0); break;
    case 1: insTpl(IT_INTEG, 0); break;
    case 2: insTpl(IT_SUM, 0); break;
    case 3: insTpl(IT_PROD, 0); break;
    case 4: insChar('!'); break;
    case 5: insCapture(IT_PERM, 0); break;
    case 6: insCapture(IT_COMB, 0); break;
    case 7: insChar('%'); break;
    case 8: insCapture(IT_DIVR, 0); break;
    case 9: insTpl(IT_FN, FN_POL); break;
    case 10: insTpl(IT_FN, FN_REC); break;
    case 11: insTok(TK_GCD); break;
    case 12: insTok(TK_LCM); break;
    case 13: insTok(TK_INT); break;
    case 14: insTok(TK_INTG); break;
    case 15: insTok(TK_RAN); break;
    case 16: insTpl(IT_FN, FN_RANINT); break;
    case 17: insChar(CH_DMS); break;
    case 18: insTok(TK_SOLVE); break;
    case 19: insTok(TK_FACTOR); break;
    case 20: insTok(TK_EXPAND); break;
  }
}
static void doConst(short i) {
  switch (i) {
    case 0: insChar(chPi); break;
    case 1: insChar(chE); break;
    case 2: insChar(chI); break;
    case 3: constMenu(); break;
    case 4: convMenu(); break;
    case 5: insTok(TK_ANS); break;
    case 6: insTok(TK_PREANS); break;
    case 7: insTok(TK_CONJ); break;
    case 8: insTok(TK_ARG); break;
    case 9: insTok(TK_REP); break;
    case 10: insTok(TK_IMP); break;
  }
}
// phim chen dung chung cho moi o nhap. Tra ve 1 neu da xu ly.
short handleInsertKey(short k) {
  switch (k) {
    case KEY_LEFT:  moveLeft(); return 1;
    case KEY_RIGHT: moveRight(); return 1;
    case KEY_UP:    moveVert(1); return 1;
    case KEY_DOWN:  moveVert(0); return 1;
    case KEY_BACKSPACE: backspace(); return 1;
    case KEY_F1: doTpl(menuSel("F1 MAU", M_TPL, 13, 0)); return 1;
    case KEY_F2: doFun(menuSel("F2 HAM", M_FUN, 14, 0)); return 1;
    case KEY_F3: doCalc(menuSel("F3 TINH TOAN", M_CALC, 21, 0)); return 1;
    case KEY_F4: doConst(menuSel("F4 HANG SO", M_CONST, 11, 0)); return 1;
    case '/': insCapture(IT_FRAC, 0); return 1;
    case '^': insTpl(IT_POW, 0); return 1;
    case 4138: insTpl(IT_SQRT, 0); return 1;                 // 2nd + ×
    case 4190: insChar(chPi); return 1;                      // 2nd + ^
    case 259: case 4185: case 4217: insTok(TK_SIN); return 1;   // 2nd + Y
    case 260: case 4186: case 4218: insTok(TK_COS); return 1;   // 2nd + Z
    case 261: case 4180: case 4212: insTok(TK_TAN); return 1;   // 2nd + T
    case 262: case 4184: case 4216: insTok(TK_LN); return 1;    // 2nd + X (ma 4184 do ban bao)
    case 16472: case 8280: case 8312: insExp(); return 1;       // ♦ + X (ma 16472)
    case 16473: case 8281: case 8313: insTok(TK_ASIN); return 1;            // ♦ + Y
    case 16474: case 8282: case 8314: insTok(TK_ACOS); return 1;            // ♦ + Z
    case 16468: case 8276: case 8308: insTok(TK_ATAN); return 1;            // ♦ + T
    case 4269: case 4141: insTok(TK_ANS); return 1;          // 2nd + (-) hoac 2nd + -
  }
  if (k >= 32 && k < 256) {
    if (k >= 'A' && k <= 'Z') k += 32;
    insChar((unsigned char)k);
    return 1;
  }
  return 0;
}

/* ---------------- ve o nhap (dung chung) ---------------- */
// ve ROOT trong vung y0..y1; cap nhat cuon ngang de con tro luon thay
void drawInput(short y0, short y1, short withCursor) {
  Box b;
  short ax;
  setClip(0, y0, 159, y1);
  mRow(ROOT, 0, &b);
  ax = y0 + 1 + b.a;
  showCur = withCursor;
  if (withCursor) {
    dry = 1; dRow(ROOT, 1, ax, 0); dry = 0;
    if (cx - scroll > 154) scroll = cx - 150;
    if (cx - scroll < 2) scroll = cx > 20 ? cx - 20 : 0;
  } else scroll = 0;
  dRow(ROOT, 1 - scroll, ax, 0);
  if (scroll > 0) { setFont(1); DrawStr(0, y0, "<", A_REPLACE); }
}

short keyBlinkAt(void) {            // cho phim, con tro nhap nhay (vung clip hien tai)
  short on = 1;
  cursorXor();
  for (;;) {
    OSFreeTimer(USER_TIMER);
    OSRegisterTimer(USER_TIMER, 10);
    while (!OSTimerExpired(USER_TIMER)) {
      if (kbhit()) {
        OSFreeTimer(USER_TIMER);
        if (on) cursorXor();
        return ngetchx();
      }
      idle();
    }
    cursorXor(); on = !on;
  }
}

// o nhap rieng (dung cho CALC, he so phuong trinh, ...). Tra ve 1 neu ENTER.
short editLineKey(const char *title, char *tiOut, short outsz, short evalIt, short firstKey) {
  short saveRoot = ROOT, saveRow = curRow, saveIdx = curIdx, saveScroll = scroll, r, k, ok = 0;
  const char *err = 0;
  r = allocRow(NONE);
  if (r < 0) return 0;
  ROOT = r; curRow = r; curIdx = 0; scroll = 0;
  if (firstKey) handleInsertKey(firstKey);
  for (;;) {
    beginDraw();
    setClip(0, 0, 159, 99);
    fillRect(0, 0, 159, 7, A_NORMAL);
    setFont(1); DrawStr(2, 1, title, A_REVERSE);
    drawStrS(1, HINTY, err ? err : (bigFont ? "ENTER=xong ESC=huy" : "ENTER: xong   ESC: huy   F1-F4: chen"), 1);
    drawInput(bigFont ? 11 : 10, 58, 1);
    endDraw();
    setClip(0, 10, 159, 58);
    k = keyBlinkAt();
    err = 0;
    if (k == KEY_ESC) break;
    if (k == KEY_CLEAR) { clearRow(ROOT); curRow = ROOT; curIdx = 0; continue; }
    if (k == KEY_ENTER) {
      if (!R[ROOT].len) continue;
      if (evalIt) {
        short e = evalRow(ROOT, 0, tiOut, outsz);
        if (e) { err = find_error_message(e); continue; }
      } else linearize(ROOT, tiOut, outsz);
      ok = 1;
      break;
    }
    handleInsertKey(k);
  }
  freeRow(r);
  ROOT = saveRoot; curRow = saveRow; curIdx = saveIdx; scroll = saveScroll;
  return ok;
}

short editLine(const char *title, char *tiOut, short outsz, short evalIt) {
  return editLineKey(title, tiOut, outsz, evalIt, 0);
}

/* ---------------- ket qua nhieu trang (phuong trinh, ...) ---------------- */
void showPages(const char *title, const char *const *labels, const char *const *texts, short n) {
  short pg = 0, k, approx = 0;
  static char shown[300];
  for (;;) {
    short r = allocRow(NONE);
    const char *src = texts[pg];
    if (approx && evalText(texts[pg], 1, shown, sizeof(shown)) == 0) src = shown;
    beginDraw();
    setClip(0, 0, 159, 99);
    fillRect(0, 0, 159, 7, A_NORMAL);
    setFont(1); DrawStr(2, 1, title, A_REVERSE);
    setFont(0); DrawStr(2, 12, labels[pg], A_NORMAL);
    if (r >= 0) {
      buildRowFromText(r, src);
      setClip(0, 22, 159, 88);
      drawRowAt(r, 1, 30, 157, 1);
      freeRow(r);
    }
    setClip(0, 0, 159, 99);
    {
      char f[50];
      sprintf(f, bigFont ? "%d/%d \x18\x19 2ndF1=S<>D" : "%d/%d  \x18\x19:trang  F6:S<>D  ESC:quay lai", pg + 1, n);
      drawStrS(1, HINTY, f, 1);
    }
    endDraw();
    k = ngetchx();
    if (k == KEY_ESC || k == KEY_CLEAR || (k == KEY_ENTER && pg == n - 1)) return;
    if (k == KEY_DOWN || k == KEY_ENTER) { if (pg < n - 1) pg++; approx = 0; }
    if (k == KEY_UP) { if (pg > 0) pg--; approx = 0; }
    if (k == KEY_F6 || k == KEY_DIAMOND + KEY_ENTER) approx = !approx;
  }
}
