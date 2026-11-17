#!/usr/bin/env python3
"""satu_soal.py - jalankan semua 22 metode (kode asli) pada satu file soal.
Pemakaian: python3 satu_soal.py data/dataset69/G3_N33.txt"""
import sys, os, tempfile, shutil, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_all import METHODS, run_one
from prep import load, write, balance
f = sys.argv[1]
C, S, D, opt = load(f)
tmp = tempfile.mkdtemp(); fb = os.path.join(tmp, 'bal.txt'); fo = os.path.join(tmp, 'orig.txt')
write(fo, C, S, D)
write(fb, *balance(C, S, D))
if opt is None:
    try:
        from prep import optimum; opt = optimum(C, S, D)
    except ImportError: pass
w = os.path.join(tmp, 'w'); os.makedirs(w)
print(f'Soal {f} ({len(C)} x {len(C[0])})' + (f', optimal = {opt}' if opt else ''))
print(f'{"Metode":18s} {"Z":>10} {"waktu (ms)":>11}')
for (mn, b, mode, pat) in METHODS:
    z, dt, err = run_one(mode, b, pat, fb, fo, opt, w)
    tag = '' if z is None or opt is None else ('  optimal' if z == opt else ('  TIDAK LAYAK' if z < opt else ''))
    print(f'{mn:18s} {str(z) if z is not None else err:>10} {dt * 1000 if dt else 0:11.2f}{tag}')
shutil.rmtree(tmp)
