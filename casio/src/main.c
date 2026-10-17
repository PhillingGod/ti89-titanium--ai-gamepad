// main.c - khoi dong va chon che do
// May tinh kieu Casio fx-580VN X cho TI-89 Titanium. Chay: casio()
#include "casio.h"

void _main(void) {
  R = malloc(sizeof(Row) * NROWS);
  if (!R) { ST_helpMsg("Khong du bo nho RAM"); return; }
  memset(R, 0, sizeof(Row) * NROWS);
  memset(varVal, 0, sizeof(varVal));
  ansText[0] = preAnsText[0] = 0;
  ROOT = allocRow(NONE);
  RESR = allocRow(NONE);
  discoverChars();
  readAngleMode();
  nextMode = 0;
  while (nextMode >= 0) {
    short m = nextMode;
    nextMode = 0;                     // mac dinh quay ve COMP
    switch (m) {
      case 0: compMode(0); break;
      case 1: compMode(2); break;
      case 9: equationMode(); break;
      case 10: inequalityMode(); break;
      case 11: compMode(1); break;
      case 12: ratioMode(); break;
      case 8: tableMode(); break;
      default:
        message("Che do nay chua co", "Se co trong ban cap nhat sau");
        break;
    }
  }
  free(R);
  GKeyFlush();
}
