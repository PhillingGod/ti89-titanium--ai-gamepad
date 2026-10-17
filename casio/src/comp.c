// comp.c - che do tinh toan (COMP), so phuc (CMPLX), kiem tra (VERIFY); bien nho, CALC, SOLVE, cai dat
#include "casio.h"

short setAngle = 0, setFmt = 0, setDigits = 4, setMixed = 0, setDecOut = 0, setComplex = 0;
short nextMode = 0;

enum { ST_EDIT, ST_SHOW };
enum { D_EXACT, D_DEC, D_REC, D_MIXED, D_ENG, D_DMS, D_FACT };
static short state, dmode, engShift, resErr, variant, divrRes, fnRes;
static char exactText[300], decText[300], lastExpr[560], info[40];
static char scratch[620];          // bo dem dung chung (SOLVE, M+, FACT)

/* ---------------- tien ich ---------------- */
static short isDigitStr(const char *s) {
  if (!*s) return 0;
  for (; *s; s++) if (*s < '0' || *s > '9') return 0;
  return 1;
}
// "p/q", "-p/q", "p" (so nguyen <= 9 chu so) -> phan so
static short parseFrac(const char *s, short *neg, long *p, long *q) {
  char a[16], b[16];
  const char *sl;
  short n;
  *neg = 0;
  if ((unsigned char)*s == CH_NEG || *s == '-') { *neg = 1; s++; }
  sl = strchr(s, '/');
  if (!sl) {
    if (strlen(s) > 9 || !isDigitStr(s)) return 0;
    *p = atol(s); *q = 1; return 1;
  }
  n = sl - s;
  if (n < 1 || n > 9 || strlen(sl + 1) > 9) return 0;
  memcpy(a, s, n); a[n] = 0; strcpy(b, sl + 1);
  if (!isDigitStr(a) || !isDigitStr(b)) return 0;
  *p = atol(a); *q = atol(b);
  return *q > 0;
}
static void lputl(char *buf, long v) { sprintf(buf + strlen(buf), "%ld", v); }

// so thap phan tuan hoan: tra ve 1 neu tao duoc (ghi vao hang RESR)
static short buildRecurring(void) {
  short neg, i, j, nd = 0, start = -1;
  long p, q, ip, rem, rems[24];
  char dig[24], buf[40];
  Item f;
  if (!parseFrac(exactText, &neg, &p, &q) || q <= 1) return 0;
  ip = p / q; rem = p % q;
  while (rem && nd < 22) {
    for (j = 0; j < nd; j++) if (rems[j] == rem) { start = j; break; }
    if (start >= 0) break;
    rems[nd] = rem;
    rem *= 10;
    dig[nd++] = '0' + (char)(rem / q);
    rem %= q;
  }
  if (start < 0) return 0;                        // huu han hoac chu ky qua dai
  clearRow(RESR);
  buf[0] = 0;
  if (neg) strcat(buf, "-");
  lputl(buf, ip);
  strcat(buf, ".");
  for (i = 0; i < start; i++) { short l = strlen(buf); buf[l] = dig[i]; buf[l + 1] = 0; }
  appendRowChars(RESR, buf);
  // phan lap lai co gach tren
  memset(&f, 0, sizeof(f));
  {
    short nr = allocRow(RESR);
    if (nr < 0) return 0;
    f.t = IT_OVER; f.a = nr; f.b = f.d = NONE;
  }
  {
    char rep[24];
    short l = 0;
    for (i = start; i < nd; i++) rep[l++] = dig[i];
    rep[l] = 0;
    appendRowChars(f.a, rep);
  }
  insItem(RESR, R[RESR].len, &f);
  return 1;
}
static short buildMixed(void) {
  short neg;
  long p, q;
  Item f;
  char b[16];
  if (!parseFrac(exactText, &neg, &p, &q) || q <= 1 || p < q) return 0;
  clearRow(RESR);
  if (neg) appendRowChars(RESR, "-");
  memset(&f, 0, sizeof(f));
  f.t = IT_MIXED;
  f.a = allocRow(RESR); f.b = allocRow(RESR); f.d = allocRow(RESR);
  b[0] = 0; lputl(b, p / q); appendRowChars(f.a, b);
  b[0] = 0; lputl(b, p % q); appendRowChars(f.b, b);
  b[0] = 0; lputl(b, q); appendRowChars(f.d, b);
  insItem(RESR, R[RESR].len, &f);
  return 1;
}
static short getDec(void) {
  if (!decText[0]) {
    if (evalText(lastExpr, 1, decText, sizeof(decText))) { decText[0] = 0; return 0; }
  }
  return 1;
}
static void buildEng(void) {
  float v, m;
  short e, e3, neg = 0;
  char buf[60], t[40];
  if (!getDec()) return;
  v = tiAtof(decText);
  if (v < 0) { neg = 1; v = -v; }
  if (v == 0) { clearRow(RESR); appendRowChars(RESR, "0"); return; }
  e = (short)floor(log10(v));
  e3 = (short)(floor(e / 3.0) * 3) - 3 * engShift;
  m = v / pow(10, e3);
  sprintf(t, "%.10g", m);
  sprintf(buf, "%s%s*10^%d", neg ? "-" : "", t, e3);
  clearRow(RESR);
  buildRowFromText(RESR, buf);
}
static void buildDMS(void) {
  float v, mf, s;
  long d, mn;
  short neg = 0;
  char buf[60], t[30];
  unsigned char deg[2];
  if (!getDec()) return;
  v = tiAtof(decText);
  if (v < 0) { neg = 1; v = -v; }
  d = (long)floor(v);
  mf = (v - d) * 60;
  mn = (long)floor(mf);
  s = (mf - mn) * 60;
  if (s >= 59.995) { s = 0; mn++; if (mn >= 60) { mn = 0; d++; } }
  clearRow(RESR);
  deg[0] = CH_DMS; deg[1] = 0;
  sprintf(buf, "%s%ld", neg ? "-" : "", d);
  appendRowChars(RESR, buf); appendRowChars(RESR, (char *)deg);
  sprintf(buf, "%ld", mn);
  appendRowChars(RESR, buf); appendRowChars(RESR, (char *)deg);
  sprintf(t, "%.2f", s);
  { short l = strlen(t); while (l > 1 && t[l - 1] == '0') t[--l] = 0; if (t[l - 1] == '.') t[--l] = 0; }
  appendRowChars(RESR, t); appendRowChars(RESR, (char *)deg);
}
// tach "{a,b}" thanh a va b (dau phay o muc ngoai cung)
static short split2(const char *s, char *a, char *b) {
  short d = 0, i, n = strlen(s);
  if (n < 3 || s[0] != '{' || s[n - 1] != '}') return 0;
  for (i = 1; i < n - 1; i++) {
    char c = s[i];
    if (c == '(' || c == '{' || c == '[') d++;
    else if (c == ')' || c == '}' || c == ']') d--;
    else if (c == ',' && d == 0) {
      memcpy(a, s + 1, i - 1); a[i - 1] = 0;
      memcpy(b, s + i + 1, n - i - 2); b[n - i - 2] = 0;
      return 1;
    }
  }
  return 0;
}
static void buildPair(const char *src, const char *l1, const char *l2) {
  static char a[150], b[150];
  clearRow(RESR);
  if (strlen(src) < 290 && split2(src, a, b)) {
    appendRowChars(RESR, l1); buildRowFromText(RESR, a);
    appendRowChars(RESR, l2); buildRowFromText(RESR, b);
  } else buildRowFromText(RESR, src);
}
static void buildDisplay(void) {
  const char *src = exactText;
  clearRow(RESR);
  if (resErr) return;
  if (dmode == D_DEC && getDec()) src = decText;
  if (dmode == D_REC && buildRecurring()) return;
  if (dmode == D_MIXED && buildMixed()) return;
  if (dmode == D_ENG) { buildEng(); return; }
  if (dmode == D_DMS) { buildDMS(); return; }
  if (variant == 1) {                                   // VERIFY
    if (!strcmp(src, "true"))  { appendRowChars(RESR, "DUNG (TRUE)"); return; }
    if (!strcmp(src, "false")) { appendRowChars(RESR, "SAI (FALSE)"); return; }
  }
  if (divrRes) { buildPair(src, "Q=", ",R="); return; }
  if (fnRes == 1) { buildPair(src, "r=", ",\x88="); return; }
  if (fnRes == 2) { buildPair(src, "x=", ",y="); return; }
  if (!strcmp(src, "false")) { appendRowChars(RESR, "Vo nghiem"); return; }
  if (dmode == D_EXACT && setMixed && buildMixed()) return;
  buildRowFromText(RESR, src);
}

/* ---------------- tinh ---------------- */
static void doEvaluate(short approx) {
  short e;
  Item *only = R[ROOT].len == 1 ? &R[ROOT].it[0] : 0;
  divrRes = only && only->t == IT_DIVR;
  fnRes = (only && only->t == IT_FN && only->c != FN_RANINT) ? (only->c == FN_POL ? 1 : 2) : 0;
  linearize(ROOT, lastExpr, sizeof(lastExpr));
  decText[0] = 0;
  e = evalRow(ROOT, approx, exactText, sizeof(exactText));
  resErr = e;
  if (!e) {
    strcpy(preAnsText, ansText);
    strcpy(ansText, exactText);
    if (approx) strcpy(decText, exactText);
  }
  dmode = approx ? D_DEC : D_EXACT;
  engShift = 0;
  info[0] = 0;
  state = ST_SHOW;
  buildDisplay();
}
static short isFracResult(void) { short n; long p, q; return parseFrac(exactText, &n, &p, &q) && q > 1; }
static void cycleSD(void) {
  if (resErr) return;
  engShift = 0;
  if (dmode == D_ENG || dmode == D_DMS || dmode == D_FACT) { dmode = D_EXACT; buildDisplay(); return; }
  if (isFracResult()) {
    if (dmode == D_EXACT || dmode == D_MIXED) { dmode = D_REC; if (!buildRecurring()) dmode = D_DEC; }
    else if (dmode == D_REC) dmode = D_DEC;
    else dmode = D_EXACT;
  } else dmode = (dmode == D_DEC) ? D_EXACT : D_DEC;
  buildDisplay();
}

/* ---------------- bien nho ---------------- */
static short askVar(const char *title) {
  short k;
  notice(title, "Chon bien: a b c d e f m x y");
  k = ngetchx();
  if (k >= 'A' && k <= 'Z') k += 32;
  return varIndex((unsigned char)k);
}
static void storeTo(short vi, const char *val) {
  if (vi < 0) return;
  strncpy(varVal[vi], val, 79); varVal[vi][79] = 0;
}
static void doSto(void) {
  short vi;
  if (state == ST_EDIT && R[ROOT].len) doEvaluate(0);
  if (resErr || !exactText[0]) return;
  vi = askVar("STO: luu ket qua vao bien");
  if (vi < 0) return;
  storeTo(vi, exactText);
  sprintf(info, "-> %c", VARCH[vi] - 32);
}
static void doRcl(void) {
  const char *items[NVARS + 1];
  static char lines[NVARS][40];
  short i;
  for (i = 0; i < NVARS; i++) {
    sprintf(lines[i], "%c = %s", VARCH[i] - 32 * (VARCH[i] < 'x'), varVal[i][0] ? varVal[i] : "(trong)");
    lines[i][36] = 0;
    { char *c; for (c = lines[i]; *c; c++) if ((unsigned char)*c == CH_NEG) *c = '-'; }
    items[i] = lines[i];
  }
  i = menuSel("RCL: chen bien vao bieu thuc", items, NVARS, 0);
  if (i >= 0) { if (state == ST_SHOW) { state = ST_EDIT; clearRow(ROOT); curRow = ROOT; curIdx = 0; } insChar(VARCH[i]); }
}
static void doMplus(short sign) {
  char *t = scratch;
  static char v[300];
  if (state == ST_EDIT && R[ROOT].len) doEvaluate(0);
  if (resErr || !exactText[0]) return;
  sprintf(t, "(%s)%c(%s)", varVal[6][0] ? varVal[6] : "0", sign > 0 ? '+' : '-', exactText);
  if (!evalText(t, 0, v, sizeof(v))) { storeTo(6, v); strcpy(info, sign > 0 ? "M+" : "M-"); }
}
// cac bien xuat hien trong bieu thuc
static void collectVars(short r, char *used) {
  short i, k;
  Row *w = &R[r];
  for (i = 0; i < w->len; i++) {
    Item *it = &w->it[i];
    if (it->t == IT_CHAR) {
      unsigned char c = it->c;
      short vi;
      if (c >= 'A' && c <= 'Z') c += 32;
      // bo qua chuoi chu dung truoc '(' (ten ham)
      if (c >= 'a' && c <= 'z') {
        short j = i;
        while (j < w->len && w->it[j].t == IT_CHAR && ((w->it[j].c | 32) >= 'a' && (w->it[j].c | 32) <= 'z')) j++;
        if (j - i >= 2 && j < w->len && w->it[j].t == IT_CHAR && w->it[j].c == '(') { i = j - 1; continue; }
      }
      vi = varIndex(c);
      if (vi >= 0) used[vi] = 1;
    }
    if (it->t == IT_DERIV || it->t == IT_INTEG || it->t == IT_SUM || it->t == IT_PROD) {
      char sub[NVARS];
      memset(sub, 0, sizeof(sub));
      for (k = 0; k < nKids(it->t); k++) collectVars(kidAt(it, k), sub);
      sub[7] = 0;                                       // x la bien chay
      for (k = 0; k < NVARS; k++) if (sub[k]) used[k] = 1;
      continue;
    }
    for (k = 0; k < nKids(it->t); k++) collectVars(kidAt(it, k), used);
  }
}
static short promptVars(char *used, short skip) {
  short i;
  char title[60], v[80];
  for (i = 0; i < NVARS; i++) {
    if (!used[i] || i == skip) continue;
    sprintf(title, "%c? (hien tai: %s)", VARCH[i] - 32 * (VARCH[i] < 'x'), varVal[i][0] ? varVal[i] : "0");
    { char *c; for (c = title; *c; c++) if ((unsigned char)*c == CH_NEG) *c = '-'; }
    title[38] = 0;
    if (!editLine(title, v, sizeof(v), 1)) return 0;
    storeTo(i, v);
  }
  return 1;
}
static void doCalc(void) {
  char used[NVARS];
  if (!R[ROOT].len) return;
  memset(used, 0, sizeof(used));
  collectVars(ROOT, used);
  if (!promptVars(used, -1)) return;
  doEvaluate(setDecOut);
}
static void doSolve(void) {
  char used[NVARS], *t = scratch;
  static char buf[560];
  short sv = 7, i, e, hasEq = 0;
  if (!R[ROOT].len) return;
  memset(used, 0, sizeof(used));
  collectVars(ROOT, used);
  if (!used[7]) for (i = 0; i < NVARS; i++) if (used[i]) { sv = i; break; }
  if (!used[sv]) { message("SOLVE", "Bieu thuc can co bien (vd x)"); return; }
  if (!promptVars(used, sv)) return;
  for (i = 0; i < R[ROOT].len; i++) if (R[ROOT].it[i].t == IT_CHAR && R[ROOT].it[i].c == '=') hasEq = 1;
  freeVar = VARCH[sv];
  linearize(ROOT, buf, sizeof(buf));
  freeVar = 0;
  sprintf(t, hasEq ? "solve(%s,zz%c)" : "solve((%s)=0,zz%c)", buf, VARCH[sv]);
  strncpy(lastExpr, t, sizeof(lastExpr) - 1); lastExpr[sizeof(lastExpr) - 1] = 0;
  decText[0] = 0; divrRes = 0; fnRes = 0;
  e = evalText(t, 0, exactText, sizeof(exactText));
  resErr = e;
  if (!e && (strlen(exactText) > 120 || strstr(exactText, "solve"))) {   // ket qua chinh xac qua dai -> gan dung
    e = evalText(t, 1, exactText, sizeof(exactText));
    resErr = e;
  }
  dmode = D_EXACT; engShift = 0; info[0] = 0; state = ST_SHOW;
  buildDisplay();
}
static void doFact(void) {
  char *t = scratch;
  if (resErr || !isDigitStr(exactText)) { strcpy(info, "FACT: can so nguyen duong"); return; }
  sprintf(t, "factor(%s)", exactText);
  if (!evalText(t, 0, decText, sizeof(decText))) {
    clearRow(RESR); buildRowFromText(RESR, decText); decText[0] = 0; dmode = D_FACT;
  }
}

/* ---------------- cai dat ---------------- */
// ---- Doi MODE cua TI ----
// Cach 1: chay lenh setMode(...) nhu tren man hinh Home. Sau do KIEM TRA lai bang phep tinh thu;
// neu chua dung thi thu ghi thang vao bang tuy chon MODE (MO_OPTIONS) voi cac gia tri co the.
static void execText(const char *s) {
  volatile HANDLE h = H_NULL;
  ESI save = top_estack;
  TRY
    push_parse_text(s);
    h = HS_popEStack();
    NG_execute(h, FALSE);
  ONERR
  ENDTRY
  if (h) HeapFree(h);
  top_estack = save;
}
static short probeAngle(void) {           // 0 Deg, 1 Rad, 2 Gra, -1 khong ro
  char t[40], o[40];
  float v;
  sprintf(t, "%s1)", tokTi(TK_ASIN));
  if (evalText(t, 1, o, sizeof(o))) return -1;
  v = tiAtof(o);
  if (v > 89 && v < 91) return 0;
  if (v > 1.5 && v < 1.6) return 1;
  if (v > 99 && v < 101) return 2;
  return -1;
}
static short probeDigits(void) {          // khop voi cai dat so chu so?
  char o[60];
  short i, dec = 0, dot = 0, e = 0;
  if (evalText("1/3", 1, o, sizeof(o))) return 0;
  for (i = 0; o[i]; i++) {
    if ((unsigned char)o[i] == CH_EE) e = 1;
    else if (o[i] == '.') dot = 1;
    else if (dot && !e && o[i] >= '0' && o[i] <= '9') dec++;
  }
  if (setFmt == 3) return e;                                  // Sci: phai co E
  if (e) return 0;
  if (setFmt == 2) return dec == setDigits;                   // Fix n: dung n so le
  return dec >= 9;                                            // Norm: day du chu so
}
static short getMode(short which) {
  MO_currentOptions();
  return which == 0 ? ModeSettings->Angle : which == 1 ? ModeSettings->Fix : which == 2 ? ModeSettings->Exp : ModeSettings->Complex;
}
static void tryModeSet(short which, short v) {
  MO_currentOptions();
  if (which == 0) ModeSettings->Angle = v;
  else if (which == 1) ModeSettings->Fix = v;
  else if (which == 2) ModeSettings->Exp = v;
  else ModeSettings->Complex = v;
  MO_digestOptions(0);
}
static void setAngleMode(void) {
  static const char *const N[3] = { "DEGREE", "RADIAN", "GRADIAN" };
  char t[60];
  short v;
  sprintf(t, "setMode(\"Angle\",\"%s\")", N[setAngle]);
  execText(t);
  if (probeAngle() == setAngle) return;
  {
    short orig = getMode(0);
    for (v = 0; v <= 3; v++) { tryModeSet(0, v); if (probeAngle() == setAngle) return; }
    tryModeSet(0, orig);                       // khong khop gia tri nao: tra lai nhu cu
    setAngle = probeAngle() < 0 ? setAngle : probeAngle();
  }
}
static void setNumberMode(void) {
  char t[70];
  short base, k;
  static const signed char D[5] = { 0, -1, 1, -2, 2 };
  if (setFmt == 2) sprintf(t, "setMode(\"Display Digits\",\"FIX %d\")", setDigits);
  else sprintf(t, "setMode(\"Display Digits\",\"FLOAT %d\")", setFmt == 0 ? 10 : setFmt == 1 ? 12 : (setDigits ? setDigits : 10));
  execText(t);
  sprintf(t, "setMode(\"Exponential Format\",\"%s\")", setFmt == 3 ? "SCIENTIFIC" : "NORMAL");
  execText(t);
  if (probeDigits()) return;
  // du phong: ghi thang vao MO_OPTIONS
  {
    short oFix = getMode(1), oExp = getMode(2);
  tryModeSet(2, setFmt == 3 ? 2 : 1);
  if (probeDigits()) return;
  base = setFmt == 2 ? setDigits + 1 : 14 + (setFmt == 0 ? 10 : setFmt == 1 ? 12 : (setDigits ? setDigits : 10));
  for (k = 0; k < 5; k++) { tryModeSet(1, base + D[k]); if (probeDigits()) return; }
    tryModeSet(1, oFix); tryModeSet(2, oExp);  // khong khop: tra lai nhu cu
  }
}
void applySetup(void) {
  setAngleMode();
  setNumberMode();
}
// trang thai so phuc hien tai: 0 = REAL (bao loi), 1 = a+bi, 2 = r<goc
static short probeComplex(void) {
  char o[40], t[8];
  sprintf(t, "%c(%c1)", chSqrt, CH_NEG);          // can(-1)
  if (evalText(t, 1, o, sizeof(o))) return 0;
  if ((unsigned char)o[0] == chI && !o[1]) return 1;
  return 2;
}
void setComplexMode(short on) {
  short want = on ? (setComplex ? 2 : 1) : 0, v;
  static const char *const N[3] = { "REAL", "RECTANGULAR", "POLAR" };
  char t[60];
  sprintf(t, "setMode(\"Complex Format\",\"%s\")", N[want]);
  execText(t);
  if (probeComplex() == want) return;
  {
    short orig = getMode(3);
    for (v = 0; v <= 4; v++) { tryModeSet(3, v); if (probeComplex() == want) return; }
    tryModeSet(3, orig);
  }
}
void readAngleMode(void) {
  short a = probeAngle();
  setAngle = a < 0 ? 0 : a;
}
static short askDigits(const char *title, short lo, short hi) {
  char v[40];
  short n;
  if (!editLine(title, v, sizeof(v), 1)) return -1;
  n = atoi(v);
  if (n < lo || n > hi) return -1;
  return n;
}
void setupMenu(void) {
  static char lines[15][34];
  const char *items[15];
  static const char *const TXT[15] = { "Goc: Do (Deg)", "Goc: Radian (Rad)", "Goc: Grad (Gra)", "So: Norm 1", "So: Norm 2",
                                       "So: Fix (so le thap phan)", "So: Sci (so chu so)", "Phan so: d/c", "Phan so: hon so ab/c",
                                       "KQ mac dinh: chinh xac", "KQ mac dinh: thap phan", "So phuc: a+bi", "So phuc: r<goc",
                                       "Co chu: LON", "Co chu: nho" };
  short i, sel = 0;
  for (;;) {
    for (i = 0; i < 15; i++) {
      short on = (i < 3 && setAngle == i) || (i >= 3 && i <= 6 && setFmt == i - 3) || (i == 7 && !setMixed) ||
                 (i == 8 && setMixed) || (i == 9 && !setDecOut) || (i == 10 && setDecOut) ||
                 (i == 11 && !setComplex) || (i == 12 && setComplex) ||
                 (i == 13 && bigFont) || (i == 14 && !bigFont);
      sprintf(lines[i], "%s%s", on ? "* " : "  ", TXT[i]);
      items[i] = lines[i];
    }
    sel = menuSel("CAI DAT (SETUP)  * = dang chon", items, 15, sel);
    if (sel < 0) break;
    if (sel < 3) setAngle = sel;
    else if (sel == 3 || sel == 4) setFmt = sel - 3;
    else if (sel == 5) { short n = askDigits("Fix: so chu so thap phan (0-9)", 0, 9); if (n >= 0) { setFmt = 2; setDigits = n; } }
    else if (sel == 6) { short n = askDigits("Sci: so chu so co nghia (1-10)", 1, 10); if (n >= 0) { setFmt = 3; setDigits = n; } }
    else if (sel == 7 || sel == 8) setMixed = sel == 8;
    else if (sel == 9 || sel == 10) setDecOut = sel == 10;
    else if (sel >= 13) { bigFont = sel == 13; continue; }
    else { setComplex = sel == 12; if (variant == 2) setComplexMode(1); }
    applySetup();
  }
}

/* ---------------- huong dan ---------------- */
static void help(void) {
  static const char *const H[] = {
    "Nhap: go nhu Casio, ENTER = tinh",
    "chia = phan so (lay so truoc lam tu)",
    "^ = mu, 2nd+nhan = can, 2nd+^ = pi",
    "F1 Mau: phan so, hon so, can, mu, log",
    "F2 Ham: sin cos tan (va -1), sinh, ln",
    "F3 Tinh: dao ham, tich phan, Sigma,",
    "  nPr nCr, %, du, Pol Rec, GCD LCM ...",
    "F4 Hang so: pi e i, 40 hang so, doi don vi",
    "F5 Bien: STO RCL M+ M- CALC SOLVE",
    "F6 hoac DIAMOND+ENTER: S<>D",
    "F7 Ket qua: hon so, ENG, do phut giay,",
    "  FACT (thua so nguyen to)",
    "F8 Menu: che do, cai dat, huong dan",
    "mui ten: di chuyen; len/xuong tu<->mau",
    "<- xoa, CLEAR xoa het, ESC thoat",
    "STO: luu, 2nd+STO (RCL): goi bien",
  };
  short top = 0, k, i, n = sizeof(H) / sizeof(H[0]);
  for (;;) {
    beginDraw();
    setClip(0, 0, 159, 99);
    fillRect(0, 0, 159, 7, A_NORMAL);
    setFont(1); DrawStr(2, 1, "HUONG DAN  (len/xuong de cuon, ESC)", A_REVERSE);
    for (i = 0; i < (bigFont ? 11 : 13) && top + i < n; i++) drawStrS(1, 9 + i * (bigFont ? 8 : 7), H[top + i], 1);
    endDraw();
    k = ngetchx();
    if (k == KEY_DOWN && top + (bigFont ? 11 : 13) < n) top++;
    else if (k == KEY_UP && top > 0) top--;
    else if (k != KEY_DOWN && k != KEY_UP) return;
  }
}

/* ---------------- menu chinh ---------------- */
static const char *const MODES[] = { "Tinh toan (COMP)", "So phuc (CMPLX)", "Co so N (BASE-N)", "Ma tran (MATRIX)",
                                     "Vecto (VECTOR)", "Thong ke (STAT)", "Phan phoi (DIST)", "Bang tinh (SPREADSHEET)",
                                     "Bang gia tri (TABLE)", "Phuong trinh / He PT", "Bat phuong trinh", "Kiem tra (VERIFY)",
                                     "Ti le (RATIO)" };
static short f8Menu(void) {   // tra ve 1 neu can thoat che do hien tai
  static const char *const M[] = { "Chon che do (MENU)", "Cai dat (SETUP)", "Huong dan", "Xoa tat ca bien", "Thoat chuong trinh" };
  short i = menuSel("F8 MENU", M, 5, 0), m;
  switch (i) {
    case 0:
      m = menuSel("CHE DO", MODES, 13, 0);
      if (m < 0) return 0;
      nextMode = m;
      return 1;
    case 1: setupMenu(); break;
    case 2: help(); break;
    case 3: memset(varVal, 0, sizeof(varVal)); strcpy(info, "Da xoa bien"); break;
    case 4: nextMode = -1; return 1;
  }
  return 0;
}
static void f5Menu(void) {
  static const char *const M[] = { "STO luu vao bien", "RCL goi bien", "M+", "M-", "CALC (nhap gia tri bien)",
                                   "SOLVE (giai theo bien)" };
  switch (menuSel("F5 BIEN", M, 6, 0)) {
    case 0: doSto(); break;
    case 1: doRcl(); break;
    case 2: doMplus(1); break;
    case 3: doMplus(-1); break;
    case 4: doCalc(); break;
    case 5: doSolve(); break;
  }
}
static void f7Menu(void) {
  static const char *const M[] = { "S<>D phan so <> thap phan", "ab/c <> d/c hon so", "ENG", "<- ENG",
                                   "do phut giay", "FACT thua so nguyen to", "Dang thap phan" };
  if (state != ST_SHOW || resErr) { strcpy(info, "Chua co ket qua"); return; }
  switch (menuSel("F7 KET QUA", M, 7, 0)) {
    case 0: cycleSD(); break;
    case 1:
      if (dmode == D_MIXED) { dmode = D_EXACT; buildDisplay(); }
      else { dmode = D_MIXED; if (!buildMixed()) { dmode = D_EXACT; buildDisplay(); } }
      break;
    case 2: if (dmode == D_ENG) engShift++; dmode = D_ENG; buildDisplay(); break;
    case 3: if (dmode == D_ENG) engShift--; else engShift = 0; dmode = D_ENG; buildDisplay(); break;
    case 4: dmode = dmode == D_DMS ? D_EXACT : D_DMS; buildDisplay(); break;
    case 5: doFact(); break;
    case 6: dmode = D_DEC; buildDisplay(); break;
  }
}

/* ---------------- man hinh ---------------- */
static void render(void) {
  static const char *const BAR[8] = { "Mau", "Ham", "Tinh", "Hang", "Bien", "S<>D", "KQ", "Menu" };
  static const char *const VNAME[3] = { "COMP", "VERIFY", "CMPLX" };
  static const char *const ANG[3] = { "D", "R", "G" };
  short i, x = 0;
  beginDraw();
  setClip(0, 0, 159, 99);
  setFont(1);
  for (i = 0; i < (bigFont ? 5 : 8); i++) {           // chu lon: chi hien F1-F5 cho vua
    short w = DrawStrWidth(BAR[i], bigFont ? F_6x8 : F_4x6);
    DrawStr(x + 1, bigFont ? 0 : 1, BAR[i], A_NORMAL);
    x += w + 3;
    DrawLine(x - 1, 0, x - 1, 7, A_NORMAL);
  }
  DrawLine(0, 8, 159, 8, A_NORMAL);
  {
    char st[24];
    sprintf(st, "%s %s%s", VNAME[variant], ANG[setAngle], setFmt == 2 ? " FIX" : setFmt == 3 ? " SCI" : "");
    drawStrS(159 - strW(st, 1), 10, st, 1);
  }
  drawInput(bigFont ? 19 : 16, 58, state == ST_EDIT);
  setClip(0, 60, 159, 91);
  if (state == ST_SHOW) {
    if (resErr) {
      drawStrS(1, 64, "Math ERROR", 0);
      drawStrS(1, 75, find_error_message(resErr), 1);
      drawStrS(1, 83, "<- -> de sua, CLEAR xoa", 1);
      if (!info[0]) { setClip(0, 0, 159, 99); drawStrS(1, HINTY, lastTried, 1); setClip(0, 60, 159, 91); }
    } else {
      Box b;
      short xx, ax;
      mRow(RESR, 0, &b);
      xx = b.w < 158 ? 158 - b.w : 1;
      ax = 90 - (b.h - b.a - 1);
      if (ax - b.a < 60) ax = 60 + b.a;
      showCur = 0;
      dRow(RESR, xx, ax, 0);
    }
  }
  setClip(0, 0, 159, 99);
  if (info[0]) drawStrS(1, HINTY, info, 1);
  else if (state == ST_EDIT && !R[ROOT].len) drawStrS(1, HINTY, bigFont ? "ENTER=tinh  2nd F3=menu" : "Go phep tinh, ENTER. F1-F5 chen, F8 menu", 1);
  endDraw();
  setClip(0, 16, 159, 58);
}

static short isOpKey(short k) {
  return k == '+' || k == '-' || k == '*' || k == '/' || k == '^' || k == '!' || k == '%';
}
void compMode(short v) {
  short k;
  variant = v;
  if (variant == 2) setComplexMode(1);
  clearRow(ROOT); clearRow(RESR);
  curRow = ROOT; curIdx = 0; state = ST_EDIT; scroll = 0; info[0] = 0; resErr = 0;
  for (;;) {
    render();
    k = (state == ST_EDIT) ? keyBlinkAt() : ngetchx();
    if (k != KEY_F5 && k != KEY_F7 && k != KEY_STO) info[0] = 0;
    if (k == KEY_ESC || k == KEY_QUIT) { nextMode = -1; break; }
    if (k == KEY_F8) { if (f8Menu()) break; continue; }
    if (k == KEY_F5) { f5Menu(); continue; }
    if (k == KEY_F7) { f7Menu(); continue; }
    if (k == KEY_STO) { doSto(); continue; }
    if (k == KEY_RCL) { doRcl(); continue; }
    if (k == KEY_F6 || k == KEY_DIAMOND + KEY_ENTER) {
      if (state == ST_SHOW) cycleSD();
      else if (R[ROOT].len) doEvaluate(1);
      continue;
    }
    if (state == ST_SHOW) {
      if (k == KEY_ENTER) { if (R[ROOT].len) doEvaluate(setDecOut); continue; }
      if (k == KEY_UP || k == KEY_DOWN) continue;
      if (k == KEY_LEFT || k == KEY_BACKSPACE) { state = ST_EDIT; curRow = ROOT; curIdx = R[ROOT].len; continue; }
      if (k == KEY_RIGHT) { state = ST_EDIT; curRow = ROOT; curIdx = 0; continue; }
      state = ST_EDIT;
      clearRow(ROOT); curRow = ROOT; curIdx = 0; scroll = 0;
      if (k == KEY_CLEAR) continue;
      if (!resErr && isOpKey(k)) insTok(TK_ANS);            // giong Casio: bam + sau ket qua => Ans+
    }
    if (k == KEY_CLEAR) { clearRow(ROOT); curRow = ROOT; curIdx = 0; scroll = 0; continue; }
    if (k == KEY_ENTER) { if (R[ROOT].len) doEvaluate(setDecOut); continue; }
    if (!handleInsertKey(k)) sprintf(info, "Phim chua ho tro (ma %d)", k);
  }
  if (variant == 2) setComplexMode(0);
}
