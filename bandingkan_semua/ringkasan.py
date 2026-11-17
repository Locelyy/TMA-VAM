#!/usr/bin/env python3
"""ringkasan.py - meringkas hasil run_all.py: akurasi, gap, menang/seri/kalah TMA-VAM, runtime.
Pemakaian: python3 ringkasan.py dataset69 [--json keluaran.json]"""
import csv, sys, json, statistics, os

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = ['TMA-VAM', 'NWCM', 'LCM', 'VAM', 'JHM', 'TOCM-MT', 'BCE', 'SSM', 'SSM (original)', 'SAM (Modif SSM)', 'CSM', 'RBSM',
        'FVAM', 'PowerRank', 'IHW', 'EXP-ICHW', 'RCWMCAM', 'MRM', 'AMM', 'HR-FVAM', 'THP', 'TOCM-SC']


def summarize(ds):
    R = list(csv.DictReader(open(os.path.join(HERE, 'hasil', f'{ds}_Z.csv'))))
    T = {r['Soal']: r for r in csv.DictReader(open(os.path.join(HERE, 'hasil', f'{ds}_runtime_ms.csv')))}
    N = len(R); out = []
    for m in MAIN:
        nopt = 0; gaps = []; bad = []; nb = nbo = nu = nuo = 0; miss = 0; rts = []
        for r in R:
            o = int(r['Optimal']); bal = r['Seimbang'] == 'ya'
            if bal: nb += 1
            else: nu += 1
            if r[m] == '': miss += 1; continue
            z = int(r[m])
            if z < o: bad.append(r['Soal']); continue          # di bawah optimal = solusi tidak layak
            if z == o:
                nopt += 1
                if bal: nbo += 1
                else: nuo += 1
            gaps.append((z - o) / o * 100)
            if T[r['Soal']].get(m): rts.append(float(T[r['Soal']][m]))
        w = l = 0
        if m != 'TMA-VAM':
            for r in R:
                if r[m] == '' or int(r[m]) < int(r['Optimal']): continue
                a, b = int(r['TMA-VAM']), int(r[m])
                w += a < b; l += a > b
        out.append(dict(m=m, n=N, nopt=nopt, acc=100 * nopt / N, gap=statistics.mean(gaps) if gaps else None,
                        gapmax=max(gaps) if gaps else None, bad=bad, miss=miss, nb=nb, nbo=nbo, nu=nu, nuo=nuo,
                        win=w, lose=l, tie=N - w - l - len(bad) - miss if m != 'TMA-VAM' else None,
                        rt_mean=statistics.mean(rts) if rts else None, rt_med=statistics.median(rts) if rts else None))
    return out


if __name__ == '__main__':
    ds = sys.argv[1]
    S = summarize(ds)
    rank = sorted(S, key=lambda s: (-s['nopt'], s['gap']))
    print(f'RINGKASAN {ds}: {S[0]["n"]} soal, {len(S)} metode (diurutkan dari akurasi tertinggi)')
    print(f'{"No":>2} {"Metode":16s} {"Optimal":>8} {"Akurasi":>8} {"Gap":>7} {"GapMaks":>8} {"TdkLayak":>8} {"Runtime(ms)":>11}   TMA-VAM menang/seri/kalah')
    for k, s in enumerate(rank, 1):
        wl = '' if s['m'] == 'TMA-VAM' else f"{s['win']:3d} / {s['tie']:3d} / {s['lose']:3d}"
        print(f"{k:2d} {s['m']:16s} {s['nopt']:4d}/{s['n']:<3d} {s['acc']:7.1f}% {s['gap']:6.2f}% {s['gapmax']:7.2f}% {len(s['bad']):8d} {s['rt_mean']:11.2f}   {wl}")
    bad = [(s['m'], s['bad']) for s in S if s['bad']]
    if bad:
        print('\nSolusi tidak layak (Z di bawah nilai optimal, berarti ada supply/demand yang tidak terpenuhi):')
        for m, b in bad: print(f'  {m}: {", ".join(b)}')
    if '--json' in sys.argv:
        json.dump(S, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
