#!/bin/sh
# build.sh - kompilasi semua program (Linux / macOS, butuh gcc dan g++)
cd "$(dirname "$0")"
python3 patch_sources.py > /dev/null || exit 1
mkdir -p bin
for f in patched/*.c;   do gcc -O2 -w -o bin/$(basename "$f" .c) "$f" -lm || echo "GAGAL: $f"; done
for f in patched/*.cpp; do g++ -O2 -w -fpermissive -o bin/$(basename "$f" .cpp) "$f" || echo "GAGAL: $f"; done
echo "Selesai: $(ls bin | wc -l) program"
