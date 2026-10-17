// casio.h - khai bao chung cho chuong trinh may tinh kieu Casio fx-580VN X tren TI-89 Titanium
#ifndef CASIO_H
#define CASIO_H

#ifndef HOST_TEST
#define USE_TI89
#define SAVE_SCREEN
#define MIN_AMS 200
#include <tigcclib.h>
#endif

/* ===================== CAU TRUC BIEU THUC ===================== */
enum {
  IT_CHAR, IT_TOK,
  IT_FRAC,   // a/b            : a=tu, b=mau
  IT_SQRT,   // √a
  IT_POW,    // ^a (mu)
  IT_NROOT,  // a√b            : a=chi so, b=bieu thuc
  IT_ABS,    // |a|
  IT_LOGB,   // log_a(b)
  IT_MIXED,  // a b/d (hon so)
  IT_PERM,   // a P b
  IT_COMB,   // a C b
  IT_DIVR,   // a ÷R b
  IT_FN,     // Pol(a,b), Rec(a,b), RanInt#(a,b)   (c = loai)
  IT_DERIV,  // d/dx(a)|x=b
  IT_INTEG,  // ∫ tu a den b cua d  dx
  IT_SUM,    // Σ x=a den b cua d
  IT_PROD,   // Π x=a den b cua d
  IT_CONV,   // a [doi don vi c]
  IT_OVER,   // gach tren (so thap phan tuan hoan) - chi de hien thi
  IT_NTYPES
};
typedef struct { unsigned char t, c, a, b, d, pad; } Item;

#define RCAP  56
#define NROWS 56
#define NONE  255
typedef struct { unsigned char used, len, parent, pad; Item it[RCAP]; } Row;

enum { FN_POL, FN_REC, FN_RANINT };

/* token (ham dang chu) */
typedef struct { const char *disp; const char *ti; } TokDef;
enum {
  TK_SIN, TK_COS, TK_TAN, TK_ASIN, TK_ACOS, TK_ATAN,
  TK_SINH, TK_COSH, TK_TANH, TK_ASINH, TK_ACOSH, TK_ATANH,
  TK_LN, TK_LOG, TK_ANS, TK_GCD, TK_LCM, TK_INT, TK_INTG, TK_RAN,
  TK_SOLVE, TK_FACTOR, TK_EXPAND, TK_PREANS, TK_CONJ, TK_ARG, TK_REP, TK_IMP,
  TK_CONST0   // 40 hang so vat ly: TK_CONST0 .. TK_CONST0+39
};
#define NCONST 40
#define NCONV  40

extern Row *R;
extern short ROOT, RESR, curRow, curIdx;
extern char tokTI[12][16];          // chuoi TI cua sin^-1 ... (do tim luc khoi dong)
extern unsigned char chSqrt, chPi, chE, chI;
#define CH_NEG  173
#define CH_EE   149
#define CH_DMS  1      // ky hieu do-phut-giay trong o nhap (ve thanh °)

/* bien nho A..F, M, X, Y */
#define NVARS 9
extern const char VARCH[NVARS];
extern char varVal[NVARS][80];
extern char ansText[300], preAnsText[300];
extern char freeVar;          // bien dang giai (SOLVE) - giu dang chu
extern short nextMode;
short varIndex(unsigned char c);

/* cai dat */
extern short setAngle;     // 0 Deg 1 Rad 2 Gra
extern short setFmt;       // 0 Norm1, 1 Norm2, 2 Fix, 3 Sci
extern short setDigits;    // so chu so cho Fix/Sci
extern short setMixed;     // 1 = hien hon so ab/c
extern short setDecOut;    // 1 = ket qua thap phan mac dinh
extern short setComplex;   // 0 a+bi, 1 r∠θ

/* ===================== HAM ===================== */
// core.c
short allocRow(short parent);
short nKids(unsigned char t);
unsigned char kidAt(Item *it, short k);
void freeRow(short r);
void clearRow(short r);
short insItem(short r, short idx, const Item *it);
void delItem(short r, short idx);
void insChar(unsigned char c);
void insTok(unsigned char k);
short insTpl(unsigned char t, unsigned char c);
void insCapture(unsigned char t, unsigned char c);   // khung lay toan hang ben trai (a/b, nCr, ...)
void insPowText(const char *s);                      // x², x³, x⁻¹
void insRootN(const char *idx);
void insTen(void);
void insExp(void);
void moveLeft(void); void moveRight(void); void moveVert(short up);
void backspace(void);
short rowHasType(short r, unsigned char t);
const char *tokDisp(unsigned char k);
const char *tokTi(unsigned char k);

short linearize(short r, char *buf, short size);    // -> chuoi TI, tra ve do dai
short evalText(const char *in, short approx, char *out, short outsz);   // tra ve ma loi (0 = OK)
extern char lastTried[64];
float tiAtof(const char *s);
short evalRow(short r, short approx, char *out, short outsz);
void buildRowFromText(short r, const char *text);   // ket qua dang chu -> khung dep (them vao cuoi hang r)
short appendRowChars(short r, const char *s);
void discoverChars(void);

// render
typedef struct { short w, h, a; } Box;
extern short showCur, cx, cax, csm, dry, curFont, bigFont;
short CA(short sm);
#define HINTY (bigFont ? 92 : 93)   // dong goi y duoi cung
short CH(short sm);
extern SCR_RECT clip;
void setFont(short sm);
void mRow(short r, short sm, Box *b);
void dRow(short r, short x, short ax, short sm);
void cursorXor(void);
void drawStrS(short x, short y, const char *s, short sm);
short strW(const char *s, short sm);
void setClip(short x0, short y0, short x1, short y1);
void beginDraw(void);
void endDraw(void);
void fillRect(short x0, short y0, short x1, short y1, short attr);
void drawRowAt(short r, short x, short ytop, short maxw, short rightAlign);

// ui.c
extern short scroll;
void drawInput(short y0, short y1, short withCursor);
short menuSel(const char *title, const char *const *items, short n, short start);
short editLine(const char *title, char *tiOut, short outsz, short evalIt);
short keyBlinkAt(void);
void showPages(const char *title, const char *const *labels, const char *const *texts, short n);
void message(const char *l1, const char *l2);
void notice(const char *l1, const char *l2);
short editLineKey(const char *title, char *tiOut, short outsz, short evalIt, short firstKey);
void readAngleMode(void);
short handleInsertKey(short k);   // phim chen chung (F1..F5, phim tat); tra ve 1 neu da xu ly

// comp.c
void compMode(short verify);
void setupMenu(void);
void applySetup(void);
void constMenu(void);
void convMenu(void);

// modes.c
void equationMode(void);
void inequalityMode(void);
void ratioMode(void);
void tableMode(void);

#endif
