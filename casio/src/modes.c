// modes.c - Phuong trinh / he phuong trinh, bat phuong trinh, ti le, bang gia tri
#include "casio.h"

#define CELLW 60
static char cell[20][CELLW];        // o he so
#define RESW 220
static char res[10][RESW];          // ket qua
static const char *resp[10];
// bo dem dung chung (cac che do khong chay cung luc)
static char BUF_EQ[600], BUF_O[300], BUF_P[320], BUF_T[300], BUF_O2[200];
static const char *labels[10];
static char labelBuf[10][24];

static void cleanCopy(char *dst, const char *src, short max) {   // doi dau tru TI -> '-'
  short i;
  for (i = 0; src[i] && i < max - 1; i++) dst[i] = (unsigned char)src[i] == CH_NEG ? '-' : src[i];
  dst[i] = 0;
  if (i > 1 && dst[i - 1] == '.') dst[i - 1] = 0;
}

/* ---------------- bang nhap he so ---------------- */
// tra ve 1 khi bam ENTER (tinh), 0 khi ESC
static short __attribute__((noinline)) gridEdit(const char *title, short rows, short cols, const char *const *colNames, const char *rowPrefix) {
  short sr = 0, sc = 0, k, i, j;
  short cw = 150 / cols;
  for (;;) {
    beginDraw();
    setClip(0, 0, 159, 99);
    fillRect(0, 0, 159, 7, A_NORMAL);
    setFont(1); DrawStr(2, 1, title, A_REVERSE);
    for (j = 0; j < cols; j++) drawStrS(9 + j * cw + 1, 10, colNames[j], 1);
    for (i = 0; i < rows; i++) {
      short y = 18 + i * 9;
      if (rowPrefix) { char p[4]; sprintf(p, "%d", i + 1); drawStrS(1, y + 1, p, 1); }
      for (j = 0; j < cols; j++) {
        char t[40];
        short x = 9 + j * cw, n;
        cleanCopy(t, cell[i * cols + j], sizeof(t));
        n = (cw - 2) / (bigFont ? 6 : 4);
        if ((short)strlen(t) > n) { t[n - 1] = '>'; t[n] = 0; }
        drawStrS(x + 1, y + 1, t, 1);
        DrawLine(x, y - 1, x, y + 7, A_NORMAL);
        if (i == sr && j == sc) fillRect(x + 1, y, x + cw - 1, y + 7, A_XOR);
      }
      DrawLine(9, y + 8, 9 + cols * cw, y + 8, A_NORMAL);
    }
    {
      short r = allocRow(NONE);
      if (r >= 0) {
        buildRowFromText(r, cell[sr * cols + sc]);
        setClip(0, 62, 159, 90);
        drawRowAt(r, 1, 66, 157, 1);
        freeRow(r);
      }
      setClip(0, 0, 159, 99);
    }
    drawStrS(1, HINTY, bigFont ? "Go so=sua  ENTER=giai" : "Go so de sua o. ENTER=giai, ESC=quay lai", 1);
    endDraw();
    k = ngetchx();
    if (k == KEY_ESC) return 0;
    if (k == KEY_ENTER) return 1;
    if (k == KEY_UP && sr > 0) sr--;
    else if (k == KEY_DOWN && sr < rows - 1) sr++;
    else if (k == KEY_LEFT) { if (sc > 0) sc--; else if (sr > 0) { sr--; sc = cols - 1; } }
    else if (k == KEY_RIGHT) { if (sc < cols - 1) sc++; else if (sr < rows - 1) { sr++; sc = 0; } }
    else if (k == KEY_BACKSPACE || k == KEY_CLEAR) strcpy(cell[sr * cols + sc], "0");
    else if ((k >= 32 && k < 256) || (k >= KEY_F1 && k <= KEY_F4) || k == 4138 || k == 4190) {
      char v[CELLW], t[40];
      sprintf(t, "O %s%d: nhap gia tri", colNames[sc], sr + 1);
      if (editLineKey(t, v, sizeof(v), 1, k)) {
        strcpy(cell[sr * cols + sc], v);
        if (sc < cols - 1) sc++; else if (sr < rows - 1) { sr++; sc = 0; }
      }
    }
  }
}

// tach chuoi theo tu " or " / " and "
static short splitWord(char *s, const char *word, char **out, short maxn) {   // tach tai cho
  short n = 0, wl = strlen(word), d = 0;
  char *p = s;
  out[n++] = s;
  while (*p && n < maxn) {
    if (*p == '(' || *p == '{' || *p == '[') d++;
    else if (*p == ')' || *p == '}' || *p == ']') d--;
    else if (d == 0 && !strncmp(p, word, wl)) { *p = 0; p += wl; out[n++] = p; continue; }
    p++;
  }
  return n;
}
static void setRes(short i, const char *v) { strncpy(res[i], v, RESW - 1); res[i][RESW - 1] = 0; }
static void show(const char *title, short n) {
  short i;
  for (i = 0; i < n; i++) resp[i] = res[i];
  showPages(title, labels, resp, n);
}
static const char *stripVar(const char *s) {     // "zzx=2" -> "2"
  if (s[0] == 'z' && s[1] == 'z' && s[3] == '=') return s + 4;
  return s;
}

/* ---------------- phuong trinh ---------------- */
static void __attribute__((noinline)) solveSimul(short n) {
  static const char V[4] = { 'x', 'y', 'z', 't' };
  char *eq = BUF_EQ, *o = BUF_O, *part[10];
  short i, j, cnt, e;
  eq[0] = 0;
  strcat(eq, "solve(");
  for (i = 0; i < n; i++) {
    if (i) strcat(eq, " and ");
    for (j = 0; j < n; j++) {
      char t[80];
      sprintf(t, "%s(%s)*zz%c", j ? "+" : "", cell[i * (n + 1) + j], V[j]);
      strcat(eq, t);
    }
    strcat(eq, "=("); strcat(eq, cell[i * (n + 1) + n]); strcat(eq, ")");
  }
  strcat(eq, ",{");
  for (j = 0; j < n; j++) { char t[6]; sprintf(t, "%szz%c", j ? "," : "", V[j]); strcat(eq, t); }
  strcat(eq, "})");
  e = evalText(eq, 0, o, 300);
  if (e) { message("Math ERROR", find_error_message(e)); return; }
  if (!strcmp(o, "false")) { message("Vo nghiem", "He khong co nghiem (No Solution)"); return; }
  if (!strcmp(o, "true") || strchr(o, '@')) { message("Vo so nghiem", "(Infinite Solution)"); return; }
  cnt = splitWord(o, " and ", part, 10);
  for (i = 0; i < n; i++) {
    sprintf(labelBuf[i], "%c=", V[i]);
    labels[i] = labelBuf[i];
    setRes(i, "?");
    for (j = 0; j < cnt; j++)
      if (part[j][0] == 'z' && part[j][2] == V[i] && part[j][3] == '=') setRes(i, stripVar(part[j]));
  }
  show("NGHIEM CUA HE", n);
}
static void polyText(short deg, char *out) {
  short i;
  out[0] = 0;
  for (i = 0; i <= deg; i++) {
    char t[90];
    short p = deg - i;
    const char *pl = i ? "+" : "";
    if (p >= 2) sprintf(t, "%s(%s)*zzx^%d", pl, cell[i], p);
    else if (p == 1) sprintf(t, "%s(%s)*zzx", pl, cell[i]);
    else sprintf(t, "%s(%s)", pl, cell[i]);
    strcat(out, t);
  }
}
static void __attribute__((noinline)) solvePoly(short deg) {
  char *poly = BUF_P, *eq = BUF_EQ, *o = BUF_O, *part[8];
  short i, n, e, cnt = 0;
  float a;
  if (!evalText(cell[0], 1, o, 300) && tiAtof(o) == 0) { message("Math ERROR", "He so a phai khac 0"); return; }
  polyText(deg, poly);
  sprintf(eq, "cSolve(%s=0,zzx)", poly);
  e = evalText(eq, 0, o, 300);
  if (!e && (strlen(o) > 200 || strstr(o, "Solve"))) e = evalText(eq, 1, o, 300);
  if (e) { message("Math ERROR", find_error_message(e)); return; }
  n = splitWord(o, " or ", part, 8);
  for (i = 0; i < n; i++) {
    sprintf(labelBuf[cnt], "x%d=", i + 1);
    labels[cnt] = labelBuf[cnt];
    setRes(cnt, stripVar(part[i]));
    cnt++;
  }
  // cuc tri (bac 2: dinh parabol; bac 3: cuc dai/cuc tieu)
  if (deg == 2) {
    char *t = BUF_T;
    evalText(cell[0], 1, o, 300);
    a = tiAtof(o);
    sprintf(t, "(\xAD(%s))/(2*(%s))", cell[1], cell[0]);
    if (!evalText(t, 0, res[cnt], RESW)) {
      sprintf(labelBuf[cnt], "x dinh (%s)", a > 0 ? "cuc tieu" : "cuc dai");
      labels[cnt] = labelBuf[cnt]; cnt++;
      sprintf(t, "(%s)-(%s)^2/(4*(%s))", cell[2], cell[1], cell[0]);
      if (!evalText(t, 0, res[cnt], RESW)) {
        sprintf(labelBuf[cnt], "y %s", a > 0 ? "nho nhat" : "lon nhat");
        labels[cnt] = labelBuf[cnt]; cnt++;
      }
    }
  } else if (deg == 3) {
    char *t = BUF_T, *dp[2];
    short m;
    sprintf(t, "solve(3*(%s)*zzx^2+2*(%s)*zzx+(%s)=0,zzx)", cell[0], cell[1], cell[2]);
    if (!evalText(t, 0, BUF_O2, sizeof(BUF_O2)) && strcmp(BUF_O2, "false")) {
      m = splitWord(BUF_O2, " or ", dp, 2);
      for (i = 0; i < m && cnt < 10; i++) {
        const char *xv = stripVar(dp[i]);
        float s;
        char sv[60];
        sprintf(t, "6*(%s)*(%s)+2*(%s)", cell[0], xv, cell[1]);
        if (evalText(t, 1, sv, sizeof(sv))) continue;
        s = tiAtof(sv);
        if (s == 0) continue;
        setRes(cnt, xv);
        sprintf(labelBuf[cnt], "x %s", s > 0 ? "cuc tieu" : "cuc dai");
        labels[cnt] = labelBuf[cnt]; cnt++;
        sprintf(t, "(%s)|zzx=(%s)", poly, xv);
        if (!evalText(t, 0, res[cnt], RESW)) {
          sprintf(labelBuf[cnt], "y %s", s > 0 ? "cuc tieu" : "cuc dai");
          labels[cnt] = labelBuf[cnt]; cnt++;
        }
      }
    }
  }
  show("NGHIEM PHUONG TRINH", cnt);
}
void equationMode(void) {
  static const char *const T[] = { "He 2 an (x, y)", "He 3 an (x, y, z)", "He 4 an (x, y, z, t)",
                                   "Phuong trinh bac 2", "Phuong trinh bac 3", "Phuong trinh bac 4" };
  static const char *const CS[4][5] = { { "a", "b", "c", "", "" }, { "a", "b", "c", "d", "" },
                                        { "a", "b", "c", "d", "e" }, { "a", "b", "c", "d", "e" } };
  static const char *const CP[5] = { "a", "b", "c", "d", "e" };
  short t, i;
  for (;;) {
    t = menuSel("PHUONG TRINH (EQN)", T, 6, 0);
    if (t < 0) return;
    for (i = 0; i < 20; i++) strcpy(cell[i], "0");
    if (t < 3) {
      short n = t + 2;
      char title[30];
      sprintf(title, "He %d an: a1x+b1y...=d", n);
      while (gridEdit(title, n, n + 1, CS[t], "1")) solveSimul(n);
    } else {
      short deg = t - 1;
      char title[30];
      sprintf(title, "Bac %d: ax^%d+...=0", deg, deg);
      while (gridEdit(title, 1, deg + 1, CP, 0)) solvePoly(deg);
    }
  }
}

/* ---------------- bat phuong trinh ---------------- */
void inequalityMode(void) {
  static const char *const T[] = { "Bac 2", "Bac 3", "Bac 4" };
  static const char *const S[] = { "... > 0", "... < 0", "... >= 0", "... <= 0" };
  static const char *const SG[] = { ">", "<", ">=", "<=" };
  static const char *const CP[5] = { "a", "b", "c", "d", "e" };
  short t, s, i, deg, e;
  char *poly = BUF_P, *eq = BUF_EQ, *o = BUF_O, title[30];
  for (;;) {
    t = menuSel("BAT PHUONG TRINH (INEQ)", T, 3, 0);
    if (t < 0) return;
    s = menuSel("CHON DAU", S, 4, 0);
    if (s < 0) continue;
    deg = t + 2;
    for (i = 0; i < 20; i++) strcpy(cell[i], "0");
    sprintf(title, "ax^%d+...%s0", deg, SG[s]);
    while (gridEdit(title, 1, deg + 1, CP, 0)) {
      polyText(deg, poly);
      sprintf(eq, "solve(%s%s0,zzx)", poly, SG[s]);
      e = evalText(eq, 0, o, 300);
      if (!e && strlen(o) > 200) e = evalText(eq, 1, o, 300);
      if (e) { message("Math ERROR", find_error_message(e)); continue; }
      if (!strcmp(o, "true")) setRes(0, "\"Moi x thuoc R\"");
      else if (!strcmp(o, "false")) setRes(0, "\"Vo nghiem\"");
      else setRes(0, o);
      labels[0] = "Nghiem:";
      show("BAT PHUONG TRINH", 1);
    }
  }
}

/* ---------------- ti le ---------------- */
void ratioMode(void) {
  static const char *const T[] = { "a:b = X:d", "a:b = c:X" };
  static const char *const C1[3] = { "a", "b", "d" };
  static const char *const C2[3] = { "a", "b", "c" };
  short t, e;
  char *eq = BUF_T;
  for (;;) {
    t = menuSel("TI LE (RATIO)", T, 2, 0);
    if (t < 0) return;
    strcpy(cell[0], "1"); strcpy(cell[1], "1"); strcpy(cell[2], "0");
    while (gridEdit(T[t], 1, 3, t == 0 ? C1 : C2, 0)) {
      if (t == 0) sprintf(eq, "(%s)*(%s)/(%s)", cell[0], cell[2], cell[1]);
      else sprintf(eq, "(%s)*(%s)/(%s)", cell[1], cell[2], cell[0]);
      e = evalText(eq, 0, res[0], RESW);
      if (e) { message("Math ERROR", find_error_message(e)); continue; }
      labels[0] = "X=";
      show("TI LE", 1);
    }
  }
}

/* ---------------- bang gia tri ---------------- */
#define TMAX 45
static char tx[TMAX][20], tf[TMAX][20], tg[TMAX][20];
void tableMode(void) {
  static char g[240];
  char *f = BUF_P, *t = BUF_EQ, st[60], en[60], sp[60], o[60];
  short useG, n, i, sel = 0, col = 1, top = 0, k;
  float a, b, h;
  char saveFree = freeVar;
  freeVar = 'x';
  if (!editLine("f(x) = ", f, 320, 0)) { freeVar = saveFree; return; }
  {
    static const char *const Q[] = { "Chi f(x)", "Them g(x)" };
    useG = menuSel("BANG GIA TRI", Q, 2, 0) == 1;
  }
  if (useG && !editLine("g(x) = ", g, sizeof(g), 0)) useG = 0;
  freeVar = saveFree;
  if (!editLine("Bat dau (Start)?", st, sizeof(st), 1)) return;
  if (!editLine("Ket thuc (End)?", en, sizeof(en), 1)) return;
  if (!editLine("Buoc nhay (Step)?", sp, sizeof(sp), 1)) return;
  evalText(st, 1, o, sizeof(o)); a = tiAtof(o);
  evalText(en, 1, o, sizeof(o)); b = tiAtof(o);
  evalText(sp, 1, o, sizeof(o)); h = tiAtof(o);
  if (h <= 0 || b < a) { message("Math ERROR", "Start/End/Step khong hop le"); return; }
  n = (short)floor((b - a) / h + 1e-9) + 1;
  if (n > (useG ? 30 : TMAX)) { message("Qua nhieu dong", useG ? "Toi da 30 dong" : "Toi da 45 dong"); return; }
  notice("Dang tinh bang...", 0);
  for (i = 0; i < n; i++) {
    char xv[60];
    sprintf(t, "(%s)+%d*(%s)", st, i, sp);
    if (evalText(t, 0, xv, sizeof(xv))) strcpy(xv, "?");
    cleanCopy(tx[i], xv, 20);
    sprintf(t, "(%s)|zzx=(%s)", f, xv);
    if (evalText(t, 1, o, sizeof(o))) strcpy(o, "ERROR");
    cleanCopy(tf[i], o, 20);
    if (useG) {
      sprintf(t, "(%s)|zzx=(%s)", g, xv);
      if (evalText(t, 1, o, sizeof(o))) strcpy(o, "ERROR");
      cleanCopy(tg[i], o, 20);
    }
  }
  for (;;) {
    short cols = useG ? 3 : 2, cw = 158 / cols, j;
    static const char *const H[3] = { "x", "f(x)", "g(x)" };
    if (sel < top) top = sel;
    if (sel > top + 7) top = sel - 7;
    beginDraw();
    setClip(0, 0, 159, 99);
    for (j = 0; j < cols; j++) drawStrS(j * cw + 2, 1, H[j], 1);
    DrawLine(0, 8, 159, 8, A_NORMAL);
    for (i = 0; i < 8 && top + i < n; i++) {
      short y = 10 + i * 9;
      for (j = 0; j < cols; j++) {
        char c[20];
        strcpy(c, j == 0 ? tx[top + i] : j == 1 ? tf[top + i] : tg[top + i]);
        { short cwd = bigFont ? 6 : 4; if ((short)strlen(c) * cwd > cw - 3) c[(cw - 3) / cwd] = 0; }
        drawStrS(j * cw + 2, y + 1, c, 1);
        if (top + i == sel && j == col) fillRect(j * cw + 1, y, j * cw + cw - 1, y + 7, A_XOR);
      }
    }
    for (j = 1; j < cols; j++) DrawLine(j * cw, 0, j * cw, 82, A_NORMAL);
    DrawLine(0, 83, 159, 83, A_NORMAL);
    drawStrS(1, 86, col == 0 ? tx[sel] : col == 1 ? tf[sel] : tg[sel], 0);
    endDraw();
    k = ngetchx();
    if (k == KEY_ESC || k == KEY_CLEAR) return;
    if (k == KEY_UP && sel > 0) sel--;
    if (k == KEY_DOWN && sel < n - 1) sel++;
    if (k == KEY_LEFT && col > 0) col--;
    if (k == KEY_RIGHT && col < cols - 1) col++;
  }
}
