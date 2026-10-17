// core.c - cau truc bieu thuc, soan thao, chuyen sang chuoi TI, tinh toan, doc ket qua
#include "casio.h"

Row *R;
short ROOT, RESR, curRow, curIdx;
char tokTI[12][16];
unsigned char chSqrt = 168, chPi = 140, chE = 150, chI = 151;
static char chD[8] = "", chInt[8] = "";     // "d(" va "∫(" (tim luc khoi dong)

const char VARCH[NVARS] = { 'a', 'b', 'c', 'd', 'e', 'f', 'm', 'x', 'y' };
char varVal[NVARS][80];
char ansText[300], preAnsText[300];
char freeVar = 0;

/* ---------------- bang token ---------------- */
// \1 = bat/tat chu nho phia tren (vd: sin^-1)
static const TokDef TOKS[] = {
  { "sin(", "sin(" }, { "cos(", "cos(" }, { "tan(", "tan(" },
  { "sin\1-1\1(", 0 }, { "cos\1-1\1(", 0 }, { "tan\1-1\1(", 0 },
  { "sinh(", "sinh(" }, { "cosh(", "cosh(" }, { "tanh(", "tanh(" },
  { "sinh\1-1\1(", 0 }, { "cosh\1-1\1(", 0 }, { "tanh\1-1\1(", 0 },
  { "ln(", "ln(" }, { "log(", "log(" }, { "Ans", 0 },
  { "GCD(", "gcd(" }, { "LCM(", "lcm(" }, { "Int(", "iPart(" }, { "Intg(", "floor(" },
  { "Ran#", "round(rand(),3)" },
  { "solve(", "solve(" }, { "factor(", "factor(" }, { "expand(", "expand(" }, { "PreAns", 0 },
  { "Conjg(", "conj(" }, { "Arg(", "angle(" }, { "ReP(", "real(" }, { "ImP(", "imag(" },
};
#define NTOKS (sizeof(TOKS) / sizeof(TOKS[0]))

// 40 hang so vat ly (CODATA 2018) - ten hien thi va gia tri
const char *const CONST_NAME[NCONST] = {
  "mp", "mn", "me", "m\xB5", "a0", "h", "\xB5N", "\xB5" "B", "hbar", "alpha",
  "re", "lc", "gp", "lcp", "lcn", "Rinf", "u", "\xB5p", "\xB5" "e", "\xB5n",
  "\xB5\xB5", "F", "e", "NA", "k", "Vm", "R", "c0", "c1", "c2",
  "sigma", "e0", "\xB5" "0", "phi0", "g", "G0", "Z0", "t", "G", "atm" };
const char *const CONST_VAL[NCONST] = {
  "1.67262192369E-27", "1.67492749804E-27", "9.1093837015E-31", "1.883531627E-28", "5.29177210903E-11",
  "6.62607015E-34", "5.0507837461E-27", "9.2740100783E-24", "1.054571817E-34", "7.2973525693E-3",
  "2.8179403262E-15", "2.42631023867E-12", "2.6752218744E8", "1.32140985539E-15", "1.31959090581E-15",
  "10973731.56816", "1.6605390666E-27", "1.41060679736E-26", "-9.2847647043E-24", "-9.6623651E-27",
  "-4.4904483E-26", "96485.33212", "1.602176634E-19", "6.02214076E23", "1.380649E-23",
  "22.71095464E-3", "8.314462618", "299792458", "3.741771852E-16", "1.438776877E-2",
  "5.670374419E-8", "8.8541878128E-12", "1.25663706212E-6", "2.067833848E-15", "9.80665",
  "7.748091729E-5", "376.730313668", "273.15", "6.6743E-11", "101325" };

// 40 phep doi don vi: nhan voi he so (CONV_OP 0 = nhan, 1 = chia, 2 = F->C, 3 = C->F)
const char *const CONV_NAME[NCONV] = {
  "in>cm", "cm>in", "ft>m", "m>ft", "yd>m", "m>yd", "mile>km", "km>mile", "nmile>m", "m>nmile",
  "acre>m2", "m2>acre", "galUS>l", "l>galUS", "galUK>l", "l>galUK", "pc>km", "km>pc", "km/h>m/s", "m/s>km/h",
  "oz>g", "g>oz", "lb>kg", "kg>lb", "atm>Pa", "Pa>atm", "mmHg>Pa", "Pa>mmHg", "hp>kW", "kW>hp",
  "kgf/cm2>Pa", "Pa>kgf/cm2", "kgf.m>J", "J>kgf.m", "lbf/in2>kPa", "kPa>lbf/in2", "F>C", "C>F", "J>cal", "cal>J" };
static const char *const CONV_K[NCONV / 2] = {
  "2.54", "0.3048", "0.9144", "1.609344", "1852", "4046.856422", "3.785411784", "4.54609", "3.085677581E13", "(1/3.6)",
  "28.34952313", "0.45359237", "101325", "133.3223684", "0.7457", "98066.5", "9.80665", "6.894757293", "0", "4.184" };

const char *tokDisp(unsigned char k) {
  if (k >= TK_CONST0) return CONST_NAME[k - TK_CONST0];
  return k < NTOKS ? TOKS[k].disp : "?";
}
const char *tokTi(unsigned char k) {
  if (k >= TK_ASIN && k <= TK_ATAN) return tokTI[k - TK_ASIN];
  if (k >= TK_ASINH && k <= TK_ATANH) return tokTI[3 + k - TK_ASINH];
  return TOKS[k].ti;
}

/* ---------------- bo nho hang ---------------- */
short allocRow(short parent) {
  short i;
  for (i = 0; i < NROWS; i++)
    if (!R[i].used) { R[i].used = 1; R[i].len = 0; R[i].parent = parent; return i; }
  return -1;
}
short nKids(unsigned char t) {
  static const unsigned char K[IT_NTYPES] = { 0, 0, 2, 1, 1, 2, 1, 2, 3, 2, 2, 2, 2, 2, 3, 3, 3, 1, 1 };
  return t < IT_NTYPES ? K[t] : 0;
}
unsigned char kidAt(Item *it, short k) { return k == 0 ? it->a : k == 1 ? it->b : it->d; }
static short kidIndex(Item *it, short r) {
  short k, n = nKids(it->t);
  for (k = 0; k < n; k++) if (kidAt(it, k) == r) return k;
  return -1;
}
static void freeKids(Item *it) {
  short k, n = nKids(it->t);
  for (k = 0; k < n; k++) freeRow(kidAt(it, k));
}
void freeRow(short r) {
  short i;
  Row *w = &R[r];
  for (i = 0; i < w->len; i++) freeKids(&w->it[i]);
  w->used = 0; w->len = 0;
}
void clearRow(short r) {
  short i;
  Row *w = &R[r];
  for (i = 0; i < w->len; i++) freeKids(&w->it[i]);
  w->len = 0;
}
static void setKidsParent(Item *it, short r) {
  short k, n = nKids(it->t);
  for (k = 0; k < n; k++) R[kidAt(it, k)].parent = r;
}
short insItem(short r, short idx, const Item *it) {
  Row *w = &R[r];
  if (w->len >= RCAP) return 0;
  memmove(&w->it[idx + 1], &w->it[idx], (w->len - idx) * sizeof(Item));
  w->it[idx] = *it;
  w->len++;
  setKidsParent(&w->it[idx], r);
  return 1;
}
void delItem(short r, short idx) {
  Row *w = &R[r];
  memmove(&w->it[idx], &w->it[idx + 1], (w->len - idx - 1) * sizeof(Item));
  w->len--;
}
static short newTemplate(unsigned char t, unsigned char c, Item *out) {
  short k, n = nKids(t);
  unsigned char got[3];
  out->t = t; out->c = c; out->a = out->b = out->d = NONE; out->pad = 0;
  for (k = 0; k < n; k++) {
    short r = allocRow(NONE);
    if (r < 0) { while (k-- > 0) R[got[k]].used = 0; return 0; }
    got[k] = r;
  }
  if (n > 0) out->a = got[0];
  if (n > 1) out->b = got[1];
  if (n > 2) out->d = got[2];
  return 1;
}
static short findInParent(short r, short *p, short *i) {
  short j, pr = R[r].parent;
  if (pr == NONE) return 0;
  for (j = 0; j < R[pr].len; j++)
    if (kidIndex(&R[pr].it[j], r) >= 0) { *p = pr; *i = j; return 1; }
  return 0;
}
short rowHasType(short r, unsigned char t) {
  short i, k;
  Row *w = &R[r];
  for (i = 0; i < w->len; i++) {
    Item *it = &w->it[i];
    if (it->t == t) return 1;
    for (k = 0; k < nKids(it->t); k++) if (rowHasType(kidAt(it, k), t)) return 1;
  }
  return 0;
}

/* ---------------- di chuyen con tro ---------------- */
static short startKid(unsigned char t) { return (t == IT_INTEG || t == IT_SUM || t == IT_PROD) ? 2 : 0; }
void moveRight(void) {
  Row *w = &R[curRow];
  short p, i, k;
  if (curIdx < w->len) {
    Item *it = &w->it[curIdx];
    if (nKids(it->t)) { curRow = it->a; curIdx = 0; } else curIdx++;
    return;
  }
  if (!findInParent(curRow, &p, &i)) { curIdx = 0; return; }
  {
    Item *it = &R[p].it[i];
    k = kidIndex(it, curRow);
    if (k + 1 < nKids(it->t)) { curRow = kidAt(it, k + 1); curIdx = 0; }
    else { curRow = p; curIdx = i + 1; }
  }
}
void moveLeft(void) {
  Row *w = &R[curRow];
  short p, i, k;
  if (curIdx > 0) {
    Item *it = &w->it[curIdx - 1];
    short n = nKids(it->t);
    if (n) { curRow = kidAt(it, n - 1); curIdx = R[curRow].len; } else curIdx--;
    return;
  }
  if (!findInParent(curRow, &p, &i)) { curIdx = w->len; return; }
  {
    Item *it = &R[p].it[i];
    k = kidIndex(it, curRow);
    if (k > 0) { curRow = kidAt(it, k - 1); curIdx = R[curRow].len; }
    else { curRow = p; curIdx = i; }
  }
}
void moveVert(short up) {
  short r = curRow, p, i, k, to;
  while (findInParent(r, &p, &i)) {
    Item *it = &R[p].it[i];
    k = kidIndex(it, r);
    to = -1;
    switch (it->t) {
      case IT_FRAC:  if (up && k == 1) to = 0; if (!up && k == 0) to = 1; break;
      case IT_MIXED: if (up && k == 2) to = 1; if (!up && k == 1) to = 2; break;
      case IT_INTEG: case IT_SUM: case IT_PROD:
        if (up && k == 0) to = 1; if (!up && k == 1) to = 0; break;
    }
    if (to >= 0) { curRow = kidAt(it, to); curIdx = R[curRow].len; return; }
    r = p;
  }
}

/* ---------------- chen / xoa ---------------- */
void insChar(unsigned char c) {
  Item it;
  memset(&it, 0, sizeof(it));
  it.t = IT_CHAR; it.c = c; it.a = it.b = it.d = NONE;
  if (insItem(curRow, curIdx, &it)) curIdx++;
}
void insTok(unsigned char k) {
  Item it;
  memset(&it, 0, sizeof(it));
  it.t = IT_TOK; it.c = k; it.a = it.b = it.d = NONE;
  if (insItem(curRow, curIdx, &it)) curIdx++;
}
short insTpl(unsigned char t, unsigned char c) {
  Item f;
  if (!newTemplate(t, c, &f)) return 0;
  if (!insItem(curRow, curIdx, &f)) { freeKids(&f); return 0; }
  curRow = kidAt(&f, startKid(t)); curIdx = 0;
  return 1;
}
static void fillRow(short r, const char *s) {
  Item it;
  memset(&it, 0, sizeof(it));
  it.t = IT_CHAR; it.a = it.b = it.d = NONE;
  while (*s) { it.c = (unsigned char)*s++; insItem(r, R[r].len, &it); }
}
void insPowText(const char *s) {      // x^2, x^3, x^-1: mu co san, con tro dung sau
  Item f;
  if (!newTemplate(IT_POW, 0, &f)) return;
  if (!insItem(curRow, curIdx, &f)) { freeKids(&f); return; }
  fillRow(f.a, s);
  curIdx++;
}
void insRootN(const char *idx) {      // can bac 3: chi so 3 co san, con tro vao trong
  Item f;
  if (!newTemplate(IT_NROOT, 0, &f)) return;
  if (!insItem(curRow, curIdx, &f)) { freeKids(&f); return; }
  fillRow(f.a, idx);
  curRow = f.b; curIdx = 0;
}
void insTen(void) { insChar('1'); insChar('0'); insTpl(IT_POW, 0); }
void insExp(void) { insChar(chE); insTpl(IT_POW, 0); }

static short isLetterC(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static short isDigitC(unsigned char c)  { return c >= '0' && c <= '9'; }
static short isOpener(Item *it) {
  if (it->t == IT_CHAR) return it->c == '(';
  if (it->t == IT_TOK) { const char *s = tokDisp(it->c); return s[strlen(s) - 1] == '('; }
  return 0;
}
static short isAtomItem(Item *it) {
  if (it->t == IT_CHAR) {
    unsigned char c = it->c;
    return isDigitC(c) || c == '.' || isLetterC(c) || c == chPi || c == chE || c == chI || c == CH_EE || c == '!' || c == '%' || c == CH_DMS;
  }
  if (it->t == IT_TOK) { const char *s = tokDisp(it->c); return s[strlen(s) - 1] != '('; }
  return 1;
}
static short operandStart(short r, short idx) {
  short i = idx;
  while (i > 0) {
    Item *it = &R[r].it[i - 1];
    if (isAtomItem(it)) { i--; continue; }
    if (it->t == IT_CHAR && it->c == ')') {
      short d = 0, j;
      for (j = i - 1; j >= 0; j--) {
        Item *q = &R[r].it[j];
        if (q->t == IT_CHAR && q->c == ')') d++;
        else if (isOpener(q)) { d--; if (d == 0) break; }
      }
      if (j < 0) break;
      i = j;
      continue;
    }
    break;
  }
  return i;
}
// khung lay toan hang ben trai lam o dau tien: a/b, nPr, nCr, ÷R, doi don vi
void insCapture(unsigned char t, unsigned char c) {
  Item f;
  short s, n, j, nk;
  if (!newTemplate(t, c, &f)) return;
  s = operandStart(curRow, curIdx);
  n = curIdx - s;
  for (j = 0; j < n; j++) {
    R[f.a].it[j] = R[curRow].it[s + j];
    setKidsParent(&R[f.a].it[j], f.a);
  }
  R[f.a].len = n;
  for (j = 0; j < n; j++) delItem(curRow, s);
  curIdx = s;
  if (!insItem(curRow, curIdx, &f)) { freeKids(&f); return; }
  nk = nKids(t);
  if (n && nk > 1) { curRow = f.b; curIdx = 0; }
  else if (n) { curIdx++; }                 // doi don vi: xong, con tro dung sau
  else { curRow = f.a; curIdx = 0; }
}
void backspace(void) {
  Row *w = &R[curRow];
  short p, i;
  if (curIdx > 0) {
    Item *it = &w->it[curIdx - 1];
    short k, n = nKids(it->t), empty = 1;
    if (!n) { delItem(curRow, curIdx - 1); curIdx--; return; }
    for (k = 0; k < n; k++) if (R[kidAt(it, k)].len) empty = 0;
    if (empty) { freeKids(it); delItem(curRow, curIdx - 1); curIdx--; return; }
    curRow = kidAt(it, n - 1); curIdx = R[curRow].len;
    return;
  }
  if (!findInParent(curRow, &p, &i)) return;
  {   // dau o con -> go bo khung, giu noi dung
    Item tmp = R[p].it[i];
    short q, j, pos = i, cpos = i, n = nKids(tmp.t);
    delItem(p, i);
    for (q = 0; q < n; q++) {
      short kr = kidAt(&tmp, q);
      Row *kw = &R[kr];
      if (kr == curRow) cpos = pos;
      for (j = 0; j < kw->len; j++) {
        if (insItem(p, pos, &kw->it[j])) pos++;
        else freeKids(&kw->it[j]);
      }
      kw->used = 0; kw->len = 0;
    }
    curRow = p; curIdx = cpos;
  }
}

/* ======================= CHUYEN SANG CHUOI TI-89 ======================= */
static char *lb;
static short ll, lmax, prevEnd, prevNum, boundX;
static short calcVariant;           // cach viet dao ham/tich phan (thu lan luot neu TI bao loi)

static void lput(char c) { if (ll < lmax - 1) lb[ll++] = c; }
static void lputs(const char *s) { while (*s) lput(*s++); }
static void lnum(const char *s) {          // so dang "1.6E-27" -> ky tu TI
  short afterE = 1;
  for (; *s; s++) {
    if (*s == 'E') { lput((char)CH_EE); afterE = 1; continue; }
    if (*s == '-' && afterE) { lput((char)CH_NEG); afterE = 0; continue; }
    lput(*s); afterE = 0;
  }
}
static void lstart(void) { if (prevEnd) lput('*'); }
static void linRow(short r);
static void lkid(const char *pre, short r, const char *post) {
  short pe = prevEnd, pn = prevNum;
  lputs(pre); linRow(r); lputs(post);
  prevEnd = pe; prevNum = pn;
}
short varIndex(unsigned char c) {
  short i;
  if (c >= 'A' && c <= 'Z') c += 32;
  for (i = 0; i < NVARS; i++) if (VARCH[i] == c) return i;
  return -1;
}
static void lvar(unsigned char c) {
  short vi;
  if (c >= 'A' && c <= 'Z') c += 32;
  vi = varIndex(c);
  if (vi >= 0 && varVal[vi][0] && !(c == 'x' && boundX) && c != freeVar) { lput('('); lputs(varVal[vi]); lput(')'); }
  else { lput('z'); lput('z'); lput(c); }
}
// nhom do-phut-giay: 1°30°15° -> (1+30/60+15/3600)
static short linDMS(Row *w, short *pi) {
  short i = *pi, part = 0, j;
  static const char *const DIV[3] = { "", "/60", "/3600" };
  // kiem tra: so roi den °
  j = i;
  while (j < w->len && w->it[j].t == IT_CHAR && (isDigitC(w->it[j].c) || w->it[j].c == '.')) j++;
  if (j == i || j >= w->len || w->it[j].t != IT_CHAR || w->it[j].c != CH_DMS) return 0;
  lstart();
  lput('(');
  while (part < 3) {
    j = i;
    while (j < w->len && w->it[j].t == IT_CHAR && (isDigitC(w->it[j].c) || w->it[j].c == '.')) j++;
    if (j == i) break;
    if (part) lput('+');
    lput('(');
    for (; i < j; i++) lput(w->it[i].c);
    lput(')');
    lputs(DIV[part]);
    part++;
    if (i < w->len && w->it[i].t == IT_CHAR && w->it[i].c == CH_DMS) i++;
    else break;
  }
  lput(')');
  *pi = i - 1;
  prevEnd = 1; prevNum = 0;
  return 1;
}
static void lconv(Item *it) {
  short k = it->c, base = k / 2;
  lstart();
  if (base == 18) {                        // nhiet do
    if (k == 36) lkid("(((", it->a, ")-32)/1.8)");
    else lkid("((", it->a, ")*1.8+32)");
  } else {
    lkid("((", it->a, ")");
    lput((k & 1) ? '/' : '*');
    lnum(CONV_K[base]);
    lput(')');
  }
  prevEnd = 1;
}
static void linItem(Row *w, short *pi) {
  Item *it = &w->it[*pi];
  unsigned char c = it->c;
  switch (it->t) {
    case IT_CHAR:
      if (isLetterC(c)) {
        short j = *pi, k;
        while (j < w->len && w->it[j].t == IT_CHAR && isLetterC(w->it[j].c)) j++;
        if (j - *pi >= 2 && j < w->len && w->it[j].t == IT_CHAR && w->it[j].c == '(') {
          lstart();                                   // ten ham TI go bang chu
          for (k = *pi; k < j; k++) lput(w->it[k].c);
          *pi = j - 1; prevEnd = 0; prevNum = 0;
          return;
        }
        lstart(); lvar(c); prevEnd = 1; prevNum = 0;
        return;
      }
      if ((isDigitC(c) || c == '.') && !prevNum && linDMS(w, pi)) return;
      if (isDigitC(c) || c == '.') {
        if (prevEnd && !prevNum) lput('*');
        lput(c); prevEnd = 1; prevNum = 1;
        return;
      }
      prevNum = 0;
      if (c == chPi || c == chE || c == chI) { lstart(); lput(c); prevEnd = 1; return; }
      if (c == '(') { lstart(); lput(c); prevEnd = 0; return; }
      if (c == ')' || c == '!') { lput(c); prevEnd = 1; return; }
      if (c == '%') { lputs("/100"); prevEnd = 1; return; }
      if (c == '-') { lput(prevEnd ? '-' : (char)CH_NEG); prevEnd = 0; return; }
      if (c == CH_DMS) { prevEnd = 1; return; }
      lput(c); prevEnd = 0;
      return;
    case IT_TOK:
      lstart();
      if (c == TK_ANS || c == TK_PREANS) {
        const char *s = c == TK_ANS ? ansText : preAnsText;
        lput('('); lputs(s[0] ? s : "0"); lput(')'); prevEnd = 1;
      } else if (c >= TK_CONST0) {
        lput('('); lnum(CONST_VAL[c - TK_CONST0]); lput(')'); prevEnd = 1;
      } else if (c == TK_RAN) {
        lputs(tokTi(c)); prevEnd = 1;
      } else { lputs(tokTi(c)); prevEnd = 0; }
      break;
    case IT_FRAC:  lstart(); lkid("((", it->a, ")/("); lkid("", it->b, "))"); prevEnd = 1; break;
    case IT_MIXED: lstart(); lkid("((", it->a, ")+("); lkid("", it->b, ")/("); lkid("", it->d, "))"); prevEnd = 1; break;
    case IT_SQRT:  lstart(); lput(chSqrt); lkid("(", it->a, ")"); prevEnd = 1; break;
    case IT_POW:   lkid("^(", it->a, ")"); prevEnd = 1; break;
    case IT_NROOT: lstart(); lkid("((", it->b, ")^(1/("); lkid("", it->a, ")))"); prevEnd = 1; break;
    case IT_ABS:   lstart(); lkid("abs(", it->a, ")"); prevEnd = 1; break;
    case IT_LOGB:  lstart(); lkid("(ln(", it->b, ")/ln("); lkid("", it->a, "))"); prevEnd = 1; break;
    case IT_PERM:  lstart(); lkid("nPr((", it->a, "),("); lkid("", it->b, "))"); prevEnd = 1; break;
    case IT_COMB:  lstart(); lkid("nCr((", it->a, "),("); lkid("", it->b, "))"); prevEnd = 1; break;
    case IT_DIVR:
      lstart();
      lkid("{intDiv((", it->a, "),("); lkid("", it->b, ")),remain(("); lkid("", it->a, "),(");
      lkid("", it->b, "))}"); prevEnd = 1; break;
    case IT_FN:
      lstart();
      if (c == FN_POL) {
        char sq[2]; sq[0] = chSqrt; sq[1] = 0;
        lputs("{"); lputs(sq);
        lkid("((", it->a, ")^2+("); lkid("", it->b, ")^2),angle(("); lkid("", it->a, ")+(");
        lkid("", it->b, ")*"); lput(chI); lputs(")}");
      } else if (c == FN_REC) {
        lkid("{(", it->a, ")*cos(("); lkid("", it->b, ")),("); lkid("", it->a, ")*sin((");
        lkid("", it->b, "))}");
      } else {
        lkid("((", it->a, ")-1+rand(("); lkid("", it->b, ")-("); lkid("", it->a, ")+1))");
      }
      prevEnd = 1; break;
    case IT_DERIV:
      lstart(); boundX++;
      if (calcVariant == 0 && chD[0]) {                     // dao ham chinh xac d(f,x)|x=a
        lputs("("); lputs(chD); lkid("(", it->a, "),zzx)|zzx=(");
        boundX--; lkid("", it->b, "))");
      } else if (calcVariant <= 1) {                         // nDeriv(f,x,h)|x=a
        lkid("(nDeriv((", it->a, "),zzx,1/100000)|zzx=(");
        boundX--; lkid("", it->b, "))");
      } else {                                               // sai phan trung tam tu tinh
        lkid("((((", it->a, ")|zzx=(");
        boundX--; lkid("", it->b, ")+1/100000)-((");
        boundX++; lkid("", it->a, ")|zzx=(");
        boundX--; lkid("", it->b, ")-1/100000))*50000)");
      }
      prevEnd = 1; break;
    case IT_INTEG:
      lstart(); boundX++;
      if (calcVariant == 0 && chInt[0]) { lputs(chInt); lkid("(", it->d, "),zzx,("); }
      else lkid("nInt((", it->d, "),zzx,(");
      boundX--;
      lkid("", it->a, "),("); lkid("", it->b, "))");
      prevEnd = 1; break;
    case IT_SUM: case IT_PROD:
      lstart(); boundX++;
      lkid(it->t == IT_SUM ? "sum(seq((" : "product(seq((", it->d, "),zzx,(");
      boundX--;
      lkid("", it->a, "),("); lkid("", it->b, ")))");
      prevEnd = 1; break;
    case IT_CONV: lconv(it); break;
    case IT_OVER: lkid("", it->a, ""); prevEnd = 1; break;
  }
  prevNum = 0;
}
static void linRow(short r) {
  short i;
  Row *w = &R[r];
  prevEnd = 0; prevNum = 0;
  for (i = 0; i < w->len; i++) linItem(w, &i);
}
short linearize(short r, char *buf, short size) {
  short d = 0, i;
  lb = buf; lmax = size; ll = 0; boundX = 0;
  linRow(r);
  for (i = 0; i < ll; i++) { if (buf[i] == '(') d++; else if (buf[i] == ')') d--; }
  while (d-- > 0) lput(')');
  buf[ll] = 0;
  return ll;
}

/* ======================= TINH TOAN ======================= */
// doc so thuc tu chuoi TI (dau tru 0xAD, so mu 0x95)
float tiAtof(const char *s) {
  float v = 0, f = 0.1;
  short neg = 0, e = 0, eneg = 0, dot = 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p == ' ' || *p == '(') p++;
  if (*p == CH_NEG || *p == '-') { neg = 1; p++; }
  for (; *p; p++) {
    if (*p >= '0' && *p <= '9') {
      if (dot) { v += (*p - '0') * f; f /= 10; } else v = v * 10 + (*p - '0');
    } else if (*p == '.') dot = 1;
    else break;
  }
  if (*p == CH_EE || *p == 'E' || *p == 'e') {
    p++;
    if (*p == CH_NEG || *p == '-') { eneg = 1; p++; }
    while (*p >= '0' && *p <= '9') { e = e * 10 + (*p - '0'); p++; }
    v *= pow(10, eneg ? -e : e);
  }
  return neg ? -v : v;
}
char lastTried[64];                 // chuoi cuoi cung gui cho TI (de xem khi bao loi)
short evalText(const char *in, short approx, char *out, short outsz) {
  volatile HANDLE h = H_NULL;
  {
    short i;
    for (i = 0; in[i] && i < 63; i++) {
      unsigned char c = in[i];
      lastTried[i] = c == CH_NEG ? '-' : c == CH_EE ? 'E' : c;
    }
    lastTried[i] = 0;
  }
  volatile short err = 0;
  ESI save = top_estack;
  out[0] = 0;
  TRY
    push_parse_text(in);
    if (approx) NG_approxESI(top_estack); else NG_rationalESI(top_estack);
    h = Parse1DExpr(top_estack, FALSE, 0);
  ONERR
    err = errCode;
  ENDTRY
  top_estack = save;
  if (!err && h) {
    strncpy(out, (const char *)HeapDeref(h), outsz - 1);
    out[outsz - 1] = 0;
  }
  if (h) HeapFree(h);
  if (!err && !out[0]) err = 1;
  return err;
}
short evalRow(short r, short approx, char *out, short outsz) {
  static char buf[560];
  short e;
  calcVariant = 0;
  if (!chD[0] && rowHasType(r, IT_DERIV)) approx = 1;        // dao ham so: luon tinh gan dung
  linearize(r, buf, sizeof(buf));
  if (!buf[0]) { out[0] = 0; return 1; }
  e = evalText(buf, approx, out, outsz);
  // dao ham / tich phan: neu cach viet nay TI khong nhan thi thu cach khac
  while (e && calcVariant < 2 && (rowHasType(r, IT_DERIV) || rowHasType(r, IT_INTEG))) {
    calcVariant++;
    linearize(r, buf, sizeof(buf));
    e = evalText(buf, 1, out, outsz);
  }
  calcVariant = 0;
  // tich phan / dao ham chua tinh duoc chinh xac -> tinh gan dung
  if (!e && !approx && ((chInt[0] && strchr(out, chInt[0])) || (chD[0] && strstr(out, chD)))) e = evalText(buf, 1, out, outsz);
  return e;
}

// tim ky tu TI cua cac ham dac biet bang cach in thu bieu thuc tren estack
static short discover1(unsigned char tag, short args, char *out, short outsz) {
  volatile HANDLE h = H_NULL;
  volatile short err = 0;
  ESI save = top_estack;
  out[0] = 0;
  TRY
    while (args-- > 0) push_quantum(VAR_X_TAG);
    push_quantum(tag);
    h = Parse1DExpr(top_estack, FALSE, 0);
  ONERR
    err = errCode;
  ENDTRY
  top_estack = save;
  if (!err && h) { strncpy(out, (const char *)HeapDeref(h), outsz - 1); out[outsz - 1] = 0; }
  if (h) HeapFree(h);
  return out[0] != 0;
}
static void prefixParen(const char *s, char *out, short outsz) {   // "sin^-1(x)" -> "sin^-1("
  const char *p = strchr(s, '(');
  short n;
  out[0] = 0;
  if (!p) return;
  n = p - s + 1;
  if (n >= outsz) return;
  memcpy(out, s, n); out[n] = 0;
}
void discoverChars(void) {
  static const unsigned char TAGS[6] = { ASIN_TAG, ACOS_TAG, ATAN_TAG, ASINH_TAG, ACOSH_TAG, ATANH_TAG };
  static const char *const FALLBACK[6] = { "arcsin(", "arccos(", "arctan(", "arcsinh(", "arccosh(", "arctanh(" };
  char t[40];
  short i;
  for (i = 0; i < 6; i++) {
    strcpy(tokTI[i], FALLBACK[i]);
    if (discover1(TAGS[i], 1, t, sizeof(t))) prefixParen(t, tokTI[i], 16);
    if (!tokTI[i][0]) strcpy(tokTI[i], FALLBACK[i]);
  }
  if (discover1(SQRT_TAG, 1, t, sizeof(t)) && t[1] == '(') chSqrt = t[0];
  if (discover1(PI_TAG, 0, t, sizeof(t)) && !t[1]) chPi = t[0];
  if (discover1(EXP_TAG, 0, t, sizeof(t)) && !t[1]) chE = t[0];
  if (discover1(IM_TAG, 0, t, sizeof(t)) && !t[1]) chI = t[0];
  if (discover1(DIFFERENTIATE_TAG, 2, t, sizeof(t))) {
    prefixParen(t, chD, sizeof(chD));
    if (strlen(chD) == 2 && (unsigned char)chD[0] >= 128) chD[1] = 0; else chD[0] = 0;
  }
  if (discover1(INTEGRATE_TAG, 2, t, sizeof(t))) {
    prefixParen(t, chInt, sizeof(chInt));
    if (strlen(chInt) == 2 && (unsigned char)chInt[0] >= 128) chInt[1] = 0; else chInt[0] = 0;
  }
}

/* ======================= DOC KET QUA (chuoi -> khung dep) ======================= */
enum { N_NUM, N_NAME, N_CALL, N_NEG, N_ADD, N_SUB, N_MUL, N_DIV, N_POW, N_SQRT, N_REL, N_WORD, N_LIST, N_FACT, N_MAT };
typedef struct { unsigned char k, op; short a, b, nx, s, n; } Node;
#define NMAX 200
static Node *N;
static short nn, pp, perr;
static const unsigned char *ps;

static short mk(unsigned char k, short a, short b) {
  if (nn >= NMAX) { perr = 1; return 0; }
  N[nn].k = k; N[nn].op = 0; N[nn].a = a; N[nn].b = b; N[nn].nx = -1; N[nn].s = 0; N[nn].n = 0;
  return nn++;
}
static void skipSp(void) { while (ps[pp] == ' ') pp++; }
static unsigned char pk(void) { skipSp(); return ps[pp]; }
static short isNameCh(unsigned char c) {
  if (isLetterC(c) || isDigitC(c) || c == '_') return 1;
  if (c >= 128 && c != chSqrt && c != CH_NEG && c != CH_EE && c != 156 && c != 157 && c != 158) return 1;
  return 0;
}
static short pLogic(void);
static short pNeg(void);
static short pArgs(short n, unsigned char close) {     // danh sach doi so -> N[n].a
  short e, last = -1;
  if (pk() != close) {
    for (;;) {
      e = pLogic(); if (perr) return 0;
      if (last < 0) N[n].a = e; else N[last].nx = e;
      last = e;
      if (pk() == ',') { pp++; continue; }
      break;
    }
  }
  if (pk() != close) { perr = 1; return 0; }
  pp++;
  return 1;
}
static short pAtom(void) {
  unsigned char c = pk();
  short n, e;
  if (perr) return 0;
  if (c == '(') {
    pp++; e = pLogic();
    if (pk() != ')') { perr = 1; return 0; }
    pp++; return e;
  }
  if (c == chSqrt) {
    pp++;
    if (pk() == '(') { pp++; e = pLogic(); if (pk() != ')') { perr = 1; return 0; } pp++; }
    else e = pAtom();
    return mk(N_SQRT, e, -1);
  }
  if (c == '{') { pp++; n = mk(N_LIST, -1, -1); pArgs(n, '}'); return n; }
  if (c == '[') {                                      // ma tran [[1,2][3,4]] / [1,2]
    short last = -1, row;
    pp++; n = mk(N_MAT, -1, -1);
    if (pk() == '[') {
      while (pk() == '[') {
        pp++; row = mk(N_LIST, -1, -1);
        if (!pArgs(row, ']')) return 0;
        if (last < 0) N[n].a = row; else N[last].nx = row;
        last = row;
        if (pk() == ',') pp++;
      }
      if (pk() != ']') { perr = 1; return 0; }
      pp++;
    } else {
      row = mk(N_LIST, -1, -1); N[n].a = row;
      if (!pArgs(row, ']')) return 0;
    }
    return n;
  }
  if (isDigitC(c) || c == '.') {
    n = mk(N_NUM, -1, -1); N[n].s = pp;
    while (isDigitC(ps[pp]) || ps[pp] == '.') pp++;
    if (ps[pp] == CH_EE) {
      pp++;
      if (ps[pp] == CH_NEG || ps[pp] == '-') pp++;
      while (isDigitC(ps[pp])) pp++;
    }
    N[n].n = pp - N[n].s;
    return n;
  }
  if (c == '"') {                                      // chuoi
    n = mk(N_NAME, -1, -1); N[n].s = pp; pp++;
    while (ps[pp] && ps[pp] != '"') pp++;
    if (ps[pp]) pp++;
    N[n].n = pp - N[n].s;
    return n;
  }
  if (isNameCh(c)) {
    n = mk(N_NAME, -1, -1); N[n].s = pp;
    if (c >= 128) pp++;
    else while (isNameCh(ps[pp]) && ps[pp] < 128) pp++;
    N[n].n = pp - N[n].s;
    if (ps[pp] == '(') { pp++; N[n].k = N_CALL; pArgs(n, ')'); }
    return n;
  }
  perr = 1;
  return 0;
}
static short pPost(void) {
  short a = pAtom();
  while (!perr && pk() == '!') { pp++; a = mk(N_FACT, a, -1); }
  return a;
}
static short pPow(void) {
  short b = pPost(), e;
  if (!perr && pk() == '^') { pp++; e = pNeg(); return mk(N_POW, b, e); }
  return b;
}
static short pNeg(void) {
  unsigned char c = pk();
  if (c == CH_NEG || c == '-') { pp++; return mk(N_NEG, pNeg(), -1); }
  return pPow();
}
static short pMul(void) {
  short a = pNeg(), b;
  unsigned char c;
  while (!perr && ((c = pk()) == '*' || c == '/')) {
    pp++; b = pNeg();
    a = mk(c == '*' ? N_MUL : N_DIV, a, b);
  }
  return a;
}
static short pAdd(void) {
  short a = pMul(), b;
  unsigned char c;
  while (!perr && ((c = pk()) == '+' || c == '-' || c == CH_NEG)) {
    pp++; b = pMul();
    a = mk(c == '+' ? N_ADD : N_SUB, a, b);
  }
  return a;
}
static short pRel(void) {
  short a = pAdd(), b;
  unsigned char c;
  while (!perr && ((c = pk()) == '=' || c == '<' || c == '>' || c == '|' || c == 156 || c == 157 || c == 158)) {
    pp++; b = pAdd();
    a = mk(N_REL, a, b); N[a].op = c;
  }
  return a;
}
static short wordAt(const char *w) {
  short n = strlen(w);
  skipSp();
  return !strncmp((const char *)ps + pp, w, n) && !isNameCh(ps[pp + n]);
}
static short pLogic(void) {
  short a = pRel(), b, n, s;
  for (;;) {
    if (perr) return a;
    if (wordAt("or")) n = 2; else if (wordAt("and")) n = 3; else if (wordAt("xor")) n = 3; else break;
    s = pp; pp += n; b = pRel();
    a = mk(N_WORD, a, b); N[a].s = s; N[a].n = n;
  }
  return a;
}

static short efail;
static void ech(short r, unsigned char c) {
  Item it;
  memset(&it, 0, sizeof(it));
  it.t = IT_CHAR; it.c = c; it.a = it.b = it.d = NONE;
  if (!insItem(r, R[r].len, &it)) efail = 1;
}
short appendRowChars(short r, const char *s) {
  while (*s) { unsigned char c = *s++; ech(r, c == CH_NEG ? '-' : c); }
  return !efail;
}
static short etpl(short r, unsigned char t, short *k1, short *k2) {
  Item f;
  if (!newTemplate(t, 0, &f)) { efail = 1; return 0; }
  if (!insItem(r, R[r].len, &f)) { freeKids(&f); efail = 1; return 0; }
  *k1 = f.a; *k2 = f.b;
  return 1;
}
static short prec(short n) {
  switch (N[n].k) {
    case N_WORD: return 1;
    case N_REL:  return 2;
    case N_ADD: case N_SUB: return 3;
    case N_MUL:  return 4;
    case N_NEG:  return 5;
    case N_DIV:  return 6;
    case N_POW:  return 7;
    case N_FACT: return 8;
  }
  return 9;
}
static short leftmost(short n) {
  for (;;) {
    unsigned char k = N[n].k;
    if (k == N_ADD || k == N_SUB || k == N_MUL || k == N_POW || k == N_FACT || k == N_REL || k == N_WORD) n = N[n].a;
    else return n;
  }
}
static void emit(short n, short r, short minp);
static void emitList(short first, short r) {
  short e;
  for (e = first; e >= 0; e = N[e].nx) { emit(e, r, 0); if (N[e].nx >= 0) ech(r, ','); }
}
static void emit(short n, short r, short minp) {
  short paren, k1, k2, e, i;
  if (efail || n < 0) return;
  paren = prec(n) < minp;
  if (paren) ech(r, '(');
  switch (N[n].k) {
    case N_NUM: case N_NAME:
      i = 0;
      if (N[n].n == 3 && ps[N[n].s] == 'z' && ps[N[n].s + 1] == 'z') i = 2;   // bien noi bo zzx -> x
      for (; i < N[n].n; i++) {
        unsigned char c = ps[N[n].s + i];
        if (c == '.' && N[n].k == N_NUM && i == N[n].n - 1 && i > 0) break;   // "6." -> "6"
        ech(r, c == CH_NEG ? '-' : c);
      }
      break;
    case N_CALL:
      if (N[n].n == 3 && !strncmp((const char *)ps + N[n].s, "abs", 3) && N[n].a >= 0 && N[N[n].a].nx < 0) {
        if (etpl(r, IT_ABS, &k1, &k2)) emit(N[n].a, k1, 0);
        break;
      }
      for (i = 0; i < N[n].n; i++) ech(r, ps[N[n].s + i]);
      ech(r, '('); emitList(N[n].a, r); ech(r, ')');
      break;
    case N_LIST: ech(r, '{'); emitList(N[n].a, r); ech(r, '}'); break;
    case N_MAT:
      ech(r, '[');
      for (e = N[n].a; e >= 0; e = N[e].nx) { ech(r, '['); emitList(N[e].a, r); ech(r, ']'); }
      ech(r, ']');
      break;
    case N_NEG:  ech(r, '-'); emit(N[n].a, r, 5); break;
    case N_ADD:  emit(N[n].a, r, 3); ech(r, '+'); emit(N[n].b, r, 3); break;
    case N_SUB:  emit(N[n].a, r, 3); ech(r, '-'); emit(N[n].b, r, 4); break;
    case N_MUL:
      emit(N[n].a, r, 4);
      if (N[leftmost(N[n].b)].k == N_NUM) ech(r, '*');
      emit(N[n].b, r, 6);
      break;
    case N_DIV:
      if (etpl(r, IT_FRAC, &k1, &k2)) { emit(N[n].a, k1, 0); emit(N[n].b, k2, 0); }
      break;
    case N_POW:
      emit(N[n].a, r, 8);
      if (etpl(r, IT_POW, &k1, &k2)) emit(N[n].b, k1, 0);
      break;
    case N_SQRT:
      if (etpl(r, IT_SQRT, &k1, &k2)) emit(N[n].a, k1, 0);
      break;
    case N_FACT: emit(N[n].a, r, 8); ech(r, '!'); break;
    case N_REL:  emit(N[n].a, r, 2); ech(r, N[n].op); emit(N[n].b, r, 3); break;
    case N_WORD:
      emit(N[n].a, r, 1); ech(r, ' ');
      for (i = 0; i < N[n].n; i++) ech(r, ps[N[n].s + i]);
      ech(r, ' '); emit(N[n].b, r, 2);
      break;
  }
  if (paren) ech(r, ')');
}
void buildRowFromText(short r, const char *text) {
  short root, len0 = R[r].len;
  if (text[0] == '"') {                               // chuoi thong bao: bo dau ngoac kep
    const char *s = text + 1;
    efail = 0;
    while (*s && *s != '"') { char c[2]; c[0] = *s++; c[1] = 0; appendRowChars(r, c); }
    return;
  }
  if (!N) N = malloc(sizeof(Node) * NMAX);
  efail = 0;
  if (N) {
    nn = 0; pp = 0; perr = 0; ps = (const unsigned char *)text;
    root = pLogic();
    if (!perr && pk() == 0) {
      emit(root, r, 0);
      if (!efail) return;
    }
  }
  // khong doc duoc -> hien nguyen chuoi
  while (R[r].len > len0) { freeKids(&R[r].it[R[r].len - 1]); R[r].len--; }
  efail = 0;
  appendRowChars(r, text);
}
