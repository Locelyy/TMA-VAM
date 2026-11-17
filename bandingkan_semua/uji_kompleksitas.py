#!/usr/bin/env python3
"""uji_kompleksitas.py - percobaan kompleksitas waktu.
1. Membuat soal acak seimbang n x n (n = 10, 20, 30, 40, 50, 60, 80, 100), 3 soal per ukuran,
   biaya 1..100, seed 2026 (hasil bisa diulang persis).
2. Menjalankan 22 metode (run_all.py) -> waktu menjalankan program.
3. Menjalankan ibfs.c mode dataset -> waktu algoritma murni 8 metode (mikrodetik).
4. Menghitung jumlah iterasi dan kemiringan log-log (eksponen pertumbuhan) n = 40..100.
Pemakaian: python3 uji_kompleksitas.py   (setelah ./build.sh)"""
import os, sys, random, subprocess, json, csv, math, statistics, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SIZES = [10, 20, 30, 40, 50, 60, 80, 100]


def slope(xs, ys):
    lx = [math.log(x) for x in xs]; ly = [math.log(y) for y in ys]
    mx, my = statistics.mean(lx), statistics.mean(ly)
    return sum((a - mx) * (b - my) for a, b in zip(lx, ly)) / sum((a - mx) ** 2 for a in lx)


def main():
    raw = os.path.join(HERE, 'data', 'skala_raw'); os.makedirs(raw, exist_ok=True)
    random.seed(2026)
    for n in SIZES:
        for r in range(1, 4):
            C = [[random.randint(1, 100) for _ in range(n)] for _ in range(n)]
            S = [random.randint(20, 120) for _ in range(n)]
            rest = sum(S) - n
            cuts = sorted(random.randint(0, rest) for _ in range(n - 1))
            D = [1 + b - a for a, b in zip([0] + cuts, cuts + [rest])]
            with open(os.path.join(raw, f'N{n:03d}_R{r}.txt'), 'w') as f:
                f.write(f"{n} {n}\n\n" + '\n'.join(' '.join(map(str, row)) for row in C) + '\n\n' + ' '.join(map(str, S)) + '\n\n' + ' '.join(map(str, D)) + '\n')
    py = sys.executable
    subprocess.run([py, 'prep.py', 'skala'], cwd=HERE, check=True)
    cache = os.path.join(HERE, 'hasil', 'skala_cache.jsonl')
    if os.path.exists(cache): os.remove(cache)
    subprocess.run([py, 'run_all.py', 'skala', '--rep', '1'], cwd=HERE, check=True)
    info = json.load(open(os.path.join(HERE, 'data', 'info.json')))['skala']
    tmp = os.path.join(HERE, 'hasil', 'tmp_skala'); os.makedirs(tmp, exist_ok=True)
    with open(os.path.join(tmp, 'opt.txt'), 'w') as f:
        for k, v in info.items(): f.write(f"{k} {v['opt_lp']}\n")
    subprocess.run([os.path.join(HERE, 'bin', 'ibfs'), '-d', os.path.join(HERE, 'data', 'skala'), 'opt.txt'], cwd=tmp, stdout=subprocess.DEVNULL, check=True)
    shutil.copy(os.path.join(tmp, 'runtime.csv'), os.path.join(HERE, 'hasil', 'skala_ibfs_runtime_us.csv'))
    R = list(csv.DictReader(open(os.path.join(HERE, 'hasil', 'skala_ibfs_runtime_us.csv'))))
    print('\nWAKTU ALGORITMA (mikrodetik, rata-rata 3 soal) dan eksponen pertumbuhan n = 40..100')
    print(f"{'Metode':12s}" + ''.join(f'{n:>10d}' for n in SIZES) + '   eksponen')
    for m in list(R[0])[2:]:
        avg = [statistics.mean(float(r[m]) for r in R if r['Ukuran'] == f'{n}x{n}') for n in SIZES]
        print(f"{m.replace(' (us)', ''):12s}" + ''.join(f'{a:10.1f}' for a in avg) + f'   {slope(SIZES[3:], avg[3:]):.2f}')
    print('\nJUMLAH ITERASI (rata-rata 3 soal); batas atas m + n - 1 = 2n - 1')
    print(f"{'Metode':12s}" + ''.join(f'{n:>8d}' for n in SIZES))
    for m in ['TMAVAM', 'VAM', 'THP', 'SSM', 'MSSM', 'CSM', 'RBSM']:
        row = []
        for n in SIZES:
            v = []
            for r in (1, 2, 3):
                o = subprocess.run([os.path.join(HERE, 'bin', 'ibfs'), os.path.join(HERE, 'data', 'skala', f'N{n:03d}_R{r}.txt'), '-v', m], capture_output=True, text=True).stdout
                k = o.count('\nIterasi')
                v.append(k - 1 if m in ('SSM', 'MSSM', 'CSM', 'RBSM') else k)   # keluarga SSM: iterasi terakhir hanya pengecekan
            row.append(statistics.mean(v))
        print(f'{m:12s}' + ''.join(f'{x:8.1f}' for x in row))


if __name__ == '__main__':
    main()
