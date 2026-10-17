#!/bin/sh
# Build casio() with GCC4TI (https://github.com/debrouxl/gcc4ti).
# The program is larger than the 24 KB ASM limit, so it is packed:
# casio.89z is a small launcher, casiodat.89y holds the compressed program.
cd "$(dirname "$0")/src"
tigcc -Os -Wall -fomit-frame-pointer --optimize-code --cut-ranges --reorder-sections \
      --merge-constants -DOPTIMIZE_ROM_CALLS -pack casiodat \
      main.c core.c render.c ui.c comp.c modes.c -o casio
mv -f casio.89z casiodat.89y ../bin/
rm -f casio.9xz casio.v2z
