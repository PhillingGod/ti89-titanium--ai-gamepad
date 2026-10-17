// render.c - ve bieu thuc dang tu nhien (phan so chong, can, mu, tich phan, ...)
#include "casio.h"

short dry, curFont = -1, showCur, cx, cax, csm;
short bigFont = 1;                 // 1 = chu lon (8x10 / 6x8), 0 = chu nho (6x8 / 4x6)
short CA(short sm) { return bigFont ? (sm ? 3 : 4) : (sm ? 2 : 3); }   // hang truc tu dinh ky tu
short CH(short sm) { return bigFont ? (sm ? 8 : 10) : (sm ? 6 : 8); }  // chieu cao ky tu
SCR_RECT clip;
extern const char *const CONV_NAME[];

void setFont(short sm) {
  short f = bigFont ? (sm ? F_6x8 : F_8x10) : (sm ? F_4x6 : F_6x8);
  if (f != curFont) { FontSetSys(f); curFont = f; }
}
void setClip(short x0, short y0, short x1, short y1) {
  clip.xy.x0 = x0; clip.xy.y0 = y0; clip.xy.x1 = x1; clip.xy.y1 = y1;
}
static short chW(unsigned char c, short sm) {
  if (bigFont) {
    if (c == '*') return sm ? 6 : 8;
    if (c == ' ') return sm ? 3 : 5;
    if (c == CH_DMS) return sm ? 4 : 5;
    return sm ? 6 : 8;
  }
  if (c == '*') return sm ? 4 : 6;
  if (c == ' ') return sm ? 2 : 4;
  if (c == CH_DMS) return sm ? 3 : 4;
  if (!sm) return 6;
  setFont(1);
  return FontCharWidth(c);
}
short strW(const char *s, short sm) {
  short w = 0;
  while (*s) w += chW((unsigned char)*s++, sm);
  return w;
}
static void ppix(short x, short y) {
  if (dry) return;
  if (x < clip.xy.x0 || x > clip.xy.x1 || y < clip.xy.y0 || y > clip.xy.y1) return;
  DrawPix(x, y, A_NORMAL);
}
static void pline(short x0, short y0, short x1, short y1) {
  WIN_RECT l;
  if (dry) return;
  l.x0 = x0; l.y0 = y0; l.x1 = x1; l.y1 = y1;
  DrawClipLine(&l, &clip, A_NORMAL);
}
static void pchar(short x, short ax, unsigned char c, short sm) {
  if (dry || c == ' ') return;
  if (c == '*') {                                   // dau nhan ×
    if (sm) { ppix(x, ax - 1); ppix(x + 2, ax - 1); ppix(x + 1, ax); ppix(x, ax + 1); ppix(x + 2, ax + 1); }
    else if (bigFont) { pline(x + 2, ax - 2, x + 5, ax + 1); pline(x + 5, ax - 2, x + 2, ax + 1); }
    else    { pline(x + 1, ax - 2, x + 3, ax); pline(x + 3, ax - 2, x + 1, ax); }
    return;
  }
  if (c == CH_DMS) {                                // dau ° (vong tron nho)
    short t = ax - CA(sm);
    ppix(x + 1, t); ppix(x, t + 1); ppix(x + 2, t + 1); ppix(x + 1, t + 2);
    return;
  }
  if (x > 159 || x < -8) return;
  setFont(sm);
  DrawClipChar(x, ax - CA(sm), c, &clip, A_NORMAL);
}
void drawStrS(short x, short y, const char *s, short sm) {
  setFont(sm);
  while (*s) {
    unsigned char c = *s++;
    if (x > 159) break;
    if (x >= 0) DrawChar(x, y, c, A_NORMAL);
    x += chW(c, sm);
  }
}
void fillRect(short x0, short y0, short x1, short y1, short attr) {
  SCR_RECT r;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 239) x1 = 239;
  if (y1 > 127) y1 = 127;
  if (x0 > x1 || y0 > y1) return;
  r.xy.x0 = x0; r.xy.y0 = y0; r.xy.x1 = x1; r.xy.y1 = y1;
  ScrRectFill(&r, &r, attr);
}

/* ---- do kich thuoc token (co the co chu nho phia tren: sin^-1) ---- */
static void mTok(const char *s, short sm, Box *b) {
  short sup = 0, hasSup = 0;
  b->w = 0;
  for (; *s; s++) {
    if (*s == 1) { sup = !sup; hasSup = 1; continue; }
    b->w += chW((unsigned char)*s, sup ? 1 : sm);
  }
  b->a = CA(sm); b->h = CH(sm);
  if (hasSup) { short u = bigFont ? 4 : 3; b->a += u; b->h += u; }
}
static void dTok(const char *s, short x, short ax, short sm) {
  short sup = 0;
  for (; *s; s++) {
    unsigned char c = *s;
    if (c == 1) { sup = !sup; continue; }
    if (sup) { pchar(x, ax - (bigFont ? 5 : 4), c, 1); x += chW(c, 1); }
    else { pchar(x, ax, c, sm); x += chW(c, sm); }
  }
}

static const char *fnName(unsigned char c) { return c == FN_POL ? "Pol(" : c == FN_REC ? "Rec(" : "RanInt#("; }

void mItem(Item *it, short sm, Box *b);
static void combine(Box *b, Box *k) {       // ghep ngang
  short D = max(b->h - b->a, k->h - k->a);
  b->w += k->w;
  b->a = max(b->a, k->a);
  b->h = b->a + D;
}
void mItem(Item *it, short sm, Box *b) {
  Box k1, k2, k3, t;
  short ca = CA(sm), ch = CH(sm);
  switch (it->t) {
    case IT_CHAR: b->w = chW(it->c, sm); b->h = ch; b->a = ca; break;
    case IT_TOK:  mTok(tokDisp(it->c), sm, b); break;
    case IT_FRAC:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      b->w = max(k1.w, k2.w) + 4; b->a = k1.h + 1; b->h = k1.h + 3 + k2.h; break;
    case IT_MIXED:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2); mRow(it->d, sm, &k3);
      b->w = k1.w + 1; b->a = k1.a; b->h = k1.h;
      t.w = max(k2.w, k3.w) + 4; t.a = k2.h + 1; t.h = k2.h + 3 + k3.h;
      combine(b, &t); break;
    case IT_SQRT:
      mRow(it->a, sm, &k1);
      b->w = k1.w + 7; b->h = k1.h + 2; b->a = k1.a + 2; break;
    case IT_POW:
      mRow(it->a, 1, &k1);
      b->w = k1.w + 1; b->a = k1.h + (sm ? 0 : 1); b->h = b->a + 1; break;
    case IT_NROOT: {
      short sh, ex, iw;
      mRow(it->a, 1, &k1); mRow(it->b, sm, &k2);
      sh = k2.h + 2; ex = k1.h - sh / 2; if (ex < 0) ex = 0;
      iw = k1.w > 2 ? k1.w - 2 : 0;
      b->w = iw + k2.w + 7; b->h = sh + ex; b->a = k2.a + 2 + ex; break;
    }
    case IT_ABS: case IT_OVER:
      mRow(it->a, sm, &k1);
      b->w = k1.w + (it->t == IT_ABS ? 5 : 0); b->h = k1.h + 2; b->a = k1.a + (it->t == IT_ABS ? 1 : 2); break;
    case IT_LOGB: {
      short d;
      mRow(it->a, 1, &k1); mRow(it->b, sm, &k2);
      b->w = strW("log", sm) + k1.w + strW("()", sm) + k2.w;
      b->a = max(ca, k2.a);
      d = max(ch - ca, k2.h - k2.a); d = max(d, k1.h + 2);
      b->h = b->a + d; break;
    }
    case IT_PERM: case IT_COMB: case IT_DIVR:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      *b = k1;
      t.w = it->t == IT_DIVR ? (sm ? 9 : 13) : chW('P', sm); t.a = ca; t.h = ch;
      combine(b, &t); combine(b, &k2); break;
    case IT_FN:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      b->w = strW(fnName(it->c), sm); b->a = ca; b->h = ch;
      combine(b, &k1); t.w = chW(',', sm); t.a = ca; t.h = ch; combine(b, &t);
      combine(b, &k2); t.w = chW(')', sm); combine(b, &t); break;
    case IT_DERIV:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      b->w = strW("d/dx(", sm); b->a = ca; b->h = ch;
      combine(b, &k1); t.w = strW(")|x=", sm); t.a = ca; t.h = ch; combine(b, &t);
      combine(b, &k2); break;
    case IT_INTEG: {
      short hc, lw;
      mRow(it->a, 1, &k1); mRow(it->b, 1, &k2); mRow(it->d, sm, &k3);
      hc = max(k3.h, 9);
      lw = max(k1.w, k2.w);
      b->w = 5 + lw + 1 + k3.w + 1 + strW("dx", sm);
      b->h = k2.h + hc + k1.h - 2;
      b->a = k2.h - 1 + k3.a + (hc - k3.h) / 2;
      break;
    }
    case IT_SUM: case IT_PROD: {
      short cw, D;
      mRow(it->a, 1, &k1); mRow(it->b, 1, &k2); mRow(it->d, sm, &k3);
      cw = max(7, k2.w); cw = max(cw, strW("x=", 1) + k1.w);
      b->w = cw + 1 + strW("()", sm) + k3.w;
      b->a = max(4 + k2.h, k3.a);
      D = max(5 + k1.h, k3.h - k3.a);
      b->h = b->a + D; break;
    }
    case IT_CONV:
      mRow(it->a, sm, &k1);
      *b = k1;
      t.w = strW(CONV_NAME[it->c], 1) + 2; t.a = 2; t.h = 6;
      combine(b, &t); break;
  }
}
void mRow(short r, short sm, Box *b) {
  Row *w = &R[r];
  short i;
  Box k;
  if (w->len == 0) {
    b->w = (r == ROOT || r == RESR) ? 1 : chW('0', sm) + 1;
    b->h = CH(sm); b->a = CA(sm);
    return;
  }
  b->w = 0; b->a = 0; b->h = 0;
  for (i = 0; i < w->len; i++) { mItem(&w->it[i], sm, &k); combine(b, &k); }
}
static void recordCursor(short r, short i, short x, short ax, short sm) {
  if (showCur && r == curRow && i == curIdx) { cx = x; cax = ax; csm = sm; }
}
static void sqrtShape(short x, short T, short B, short right) {
  pline(x, B - 2, x + 1, B - 2);
  pline(x + 1, B - 2, x + 2, B);
  pline(x + 2, B, x + 4, T);
  pline(x + 4, T, right, T);
}
static short dStr(short x, short ax, const char *s, short sm) {
  while (*s) { pchar(x, ax, *s, sm); x += chW(*s, sm); s++; }
  return x;
}
static void dItem(Item *it, short x, short ax, short sm, Box *b) {
  Box k1, k2, k3;
  short T = ax - b->a, B = T + b->h - 1;
  switch (it->t) {
    case IT_CHAR: pchar(x, ax, it->c, sm); break;
    case IT_TOK:  dTok(tokDisp(it->c), x, ax, sm); break;
    case IT_FRAC:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      pline(x + 1, ax, x + b->w - 2, ax);
      dRow(it->a, x + (b->w - k1.w) / 2, ax - 1 - k1.h + k1.a, sm);
      dRow(it->b, x + (b->w - k2.w) / 2, ax + 2 + k2.a, sm);
      break;
    case IT_MIXED: {
      short fw;
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2); mRow(it->d, sm, &k3);
      dRow(it->a, x, ax, sm);
      x += k1.w + 1;
      fw = max(k2.w, k3.w) + 4;
      pline(x + 1, ax, x + fw - 2, ax);
      dRow(it->b, x + (fw - k2.w) / 2, ax - 1 - k2.h + k2.a, sm);
      dRow(it->d, x + (fw - k3.w) / 2, ax + 2 + k3.a, sm);
      break;
    }
    case IT_SQRT:
      sqrtShape(x, T, B, x + b->w - 1);
      dRow(it->a, x + 6, ax, sm);
      break;
    case IT_POW:
      mRow(it->a, 1, &k1);
      dRow(it->a, x, T + k1.a, 1);
      break;
    case IT_NROOT: {
      short sh, ex, iw, sx, st;
      mRow(it->a, 1, &k1); mRow(it->b, sm, &k2);
      sh = k2.h + 2; ex = k1.h - sh / 2; if (ex < 0) ex = 0;
      iw = k1.w > 2 ? k1.w - 2 : 0;
      sx = x + iw; st = T + ex;
      dRow(it->a, x, st + sh / 2 - k1.h + k1.a, 1);
      sqrtShape(sx, st, B, x + b->w - 1);
      dRow(it->b, sx + 6, ax, sm);
      break;
    }
    case IT_ABS:
      pline(x + 1, T, x + 1, B);
      pline(x + b->w - 2, T, x + b->w - 2, B);
      dRow(it->a, x + 3, ax, sm);
      break;
    case IT_OVER:
      pline(x, T, x + b->w - 1, T);
      dRow(it->a, x, ax, sm);
      break;
    case IT_LOGB:
      mRow(it->a, 1, &k1); mRow(it->b, sm, &k2);
      x = dStr(x, ax, "log", sm);
      dRow(it->a, x, ax + 2 + k1.a, 1); x += k1.w;
      x = dStr(x, ax, "(", sm);
      dRow(it->b, x, ax, sm); x += k2.w;
      dStr(x, ax, ")", sm);
      break;
    case IT_PERM: case IT_COMB: case IT_DIVR:
      mRow(it->a, sm, &k1);
      dRow(it->a, x, ax, sm); x += k1.w;
      if (it->t == IT_DIVR) {                       // ÷R
        short dx = x + 1;
        pline(dx, ax, dx + 4, ax); ppix(dx + 2, ax - 2); ppix(dx + 2, ax + 2);
        pchar(x + (sm ? 5 : 7), ax, 'R', sm);
        x += sm ? 9 : 13;
      } else { pchar(x, ax, it->t == IT_PERM ? 'P' : 'C', sm); x += chW('P', sm); }
      dRow(it->b, x, ax, sm);
      break;
    case IT_FN:
      mRow(it->a, sm, &k1); mRow(it->b, sm, &k2);
      x = dStr(x, ax, fnName(it->c), sm);
      dRow(it->a, x, ax, sm); x += k1.w;
      x = dStr(x, ax, ",", sm);
      dRow(it->b, x, ax, sm); x += k2.w;
      dStr(x, ax, ")", sm);
      break;
    case IT_DERIV:
      mRow(it->a, sm, &k1);
      x = dStr(x, ax, "d/dx(", sm);
      dRow(it->a, x, ax, sm); x += k1.w;
      x = dStr(x, ax, ")|x=", sm);
      dRow(it->b, x, ax, sm);
      break;
    case IT_INTEG: {
      short lw, fx;
      mRow(it->a, 1, &k1); mRow(it->b, 1, &k2); mRow(it->d, sm, &k3);
      // dau tich phan
      pline(x + 2, T + 1, x + 2, B - 1);
      ppix(x + 3, T); ppix(x + 4, T + 1);
      ppix(x + 1, B); ppix(x, B - 1);
      lw = max(k1.w, k2.w);
      dRow(it->b, x + 5, T + k2.a, 1);                         // can tren
      dRow(it->a, x + 5, B - k1.h + 1 + k1.a, 1);              // can duoi
      fx = x + 5 + lw + 1;
      dRow(it->d, fx, ax, sm);
      dStr(fx + k3.w + 1, ax, "dx", sm);
      break;
    }
    case IT_SUM: case IT_PROD: {
      short cw, sx, fx;
      mRow(it->a, 1, &k1); mRow(it->b, 1, &k2); mRow(it->d, sm, &k3);
      cw = max(7, k2.w); cw = max(cw, strW("x=", 1) + k1.w);
      sx = x + (cw - 7) / 2;
      if (it->t == IT_SUM) {
        pline(sx, ax - 4, sx + 6, ax - 4); pline(sx, ax + 4, sx + 6, ax + 4);
        pline(sx, ax - 4, sx + 3, ax); pline(sx + 3, ax, sx, ax + 4);
      } else {
        pline(sx, ax - 4, sx + 6, ax - 4);
        pline(sx + 1, ax - 4, sx + 1, ax + 4); pline(sx + 5, ax - 4, sx + 5, ax + 4);
      }
      dRow(it->b, x + (cw - k2.w) / 2, ax - 5 - k2.h + k2.a, 1);
      {
        short lx = x + (cw - strW("x=", 1) - k1.w) / 2, ly = ax + 5 + k1.a;
        lx = dStr(lx, ly, "x=", 1);
        dRow(it->a, lx, ly, 1);
      }
      fx = dStr(x + cw + 1, ax, "(", sm);
      dRow(it->d, fx, ax, sm);
      dStr(fx + k3.w, ax, ")", sm);
      break;
    }
    case IT_CONV:
      mRow(it->a, sm, &k1);
      dRow(it->a, x, ax, sm);
      dStr(x + k1.w + 1, ax, CONV_NAME[it->c], 1);
      break;
  }
}
void dRow(short r, short x, short ax, short sm) {
  Row *w = &R[r];
  short i;
  Box b;
  if (w->len == 0) {
    recordCursor(r, 0, x, ax, sm);
    if (r != ROOT && r != RESR) {
      short bw = chW('0', sm), bh = CH(sm) - 1, top = ax - CA(sm), j;
      for (j = 0; j < bw; j += 2) { ppix(x + j, top); ppix(x + j, top + bh - 1); }
      for (j = 0; j < bh; j += 2) { ppix(x, top + j); ppix(x + bw - 1, top + j); }
    }
    return;
  }
  for (i = 0; i < w->len; i++) {
    recordCursor(r, i, x, ax, sm);
    mItem(&w->it[i], sm, &b);
    dItem(&w->it[i], x, ax, sm, &b);
    x += b.w;
  }
  recordCursor(r, w->len, x, ax, sm);
}
// ve hang r bat dau tu toa do (x, ytop); can phai neu rightAlign
void drawRowAt(short r, short x, short ytop, short maxw, short rightAlign) {
  Box b;
  mRow(r, 0, &b);
  if (rightAlign && b.w < maxw) x += maxw - b.w;
  dRow(r, x, ytop + b.a, 0);
}
void cursorXor(void) {
  WIN_RECT l;
  l.x0 = l.x1 = cx > 0 ? cx - 1 : 0;
  l.y0 = cax - CA(csm); l.y1 = cax - CA(csm) + CH(csm) - 1;
  DrawClipLine(&l, &clip, A_XOR);
}

static void *vbuf;
void beginDraw(void) {
  if (!vbuf) vbuf = malloc(LCD_SIZE);
  if (vbuf) PortSet(vbuf, 239, 127);
  ClrScr();
  curFont = -1;
}
void endDraw(void) {
  if (!vbuf) return;
  PortRestore();
  memcpy(LCD_MEM, vbuf, LCD_SIZE);
}
