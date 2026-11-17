#!/usr/bin/env python3
"""patch_sources.py - menyalin kode asli (src/) ke patched/ dengan perubahan sekecil mungkin.
Semua perubahan hanya menyangkut cara membaca input, portabilitas Linux, dan dua bug yang
membuat hasil salah cetak. Logika metode tidak diubah sama sekali."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SRC, OUT = os.path.join(HERE, 'src'), os.path.join(HERE, 'patched')
os.makedirs(OUT, exist_ok=True)
CHANGES = {
 # 1. nama file input yang ditulis tetap di kode -> input.txt
 'BCE.c':          [('"n06.txt"', '"input.txt"')],
 'JHM_langsung.c': [('"s4.txt"', '"input.txt"'),
                    ('\t\tprint_result();', '\t\tprint_result(); break; /* PATCH: satu soal */')],
 'TOCM-TDM1.c':    [('"n03.txt"', '"input.txt"')],
 'VAM.c':          [('"N32.txt"', '"input.txt"')],
 'SSM.c':          [('"ss5.txt"', '"input.txt"'),
                    # 2. bug: total_alloc tidak diinisialisasi sehingga total biaya tercetak acak
                    ('//total_alloc=0;', 'total_alloc=0; /* PATCH: inisialisasi */')],
 'SSM-original.c': [('"N06.txt"', '"input.txt"'),
                    ('//total_alloc=0;', 'total_alloc=0; /* PATCH: inisialisasi */'),
                    # 3. loop while(!feof) membaca ulang file dan memproses data sampah -> berhenti setelah satu soal
                    ('printf("\\n Total Alokasi tabel adalah: %d", total_alloc);', 'printf("\\n Total Alokasi tabel adalah: %d", total_alloc); break; /* PATCH: satu soal */')],
 'SAM.c':          [('"N06.txt"', '"input.txt"'),
                    ('printf("Total Alokasi tabel adalah: %d\\n", total_alloc);', 'printf("Total Alokasi tabel adalah: %d\\n", total_alloc); break; /* PATCH: satu soal */')],
 'LCM2.cpp':       [('"test1.txt"', '"input.txt"'), ('#include<conio.h>', ''), ('#include"windows.h"', ''),
                    ('printf("\\n\\n Biaya Minimumnya : %d\\n\\n",sum);', 'printf("\\n\\n Biaya Minimumnya : %d\\n\\n",sum); break; /* PATCH: satu soal */')],
 'NWCM_FIX.cpp':   [('"p1717.txt"', '"input.txt"'), ('#include<conio.h>', ''),
                    ('printf("\\n Cost=%d",cost);', 'printf("\\n Cost=%d",cost); break; /* PATCH: satu soal */')],
 # 5. bug: bila alokasi FLC awal sudah layak (tidak ada baris ER), program berhenti dengan "Soal Salah"
 #    padahal menurut paper langkah berikutnya adalah langsung menghitung total biaya
 'RBSM.cpp':       [('printf("Soal Salah\\n");', 'countTCC(); /* PATCH: alokasi FLC sudah layak */')],
 'CSM2.cpp':       [('printf("Soal Salah\\n");', 'countTCC(); /* PATCH: alokasi FLC sudah layak */')],
 # 4. portabilitas: LLONG_MAX dipakai tanpa #include <limits.h>
 'RCWMCAM.c':      [('#include <stdio.h>', '#include <stdio.h>\n#include <limits.h> /* PATCH */')],
}
for f in sorted(os.listdir(SRC)):
    with open(os.path.join(SRC, f), encoding='latin-1', newline='') as fh: s = fh.read()
    for a, b in CHANGES.get(f, []):
        n = s.count(a)
        if n != 1: sys.exit(f'{f}: pola {a!r} ditemukan {n} kali')
        s = s.replace(a, b)
    with open(os.path.join(OUT, f), 'w', encoding='latin-1', newline='') as fh: fh.write(s)
    print(('dipatch ' if f in CHANGES else 'disalin ') + f)
