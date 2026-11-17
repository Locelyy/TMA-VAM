#!/usr/bin/env python3
"""
run_all.py - menjalankan SEMUA program IBFS (kode asli dari folder Program IBFS) dan TMA-VAM
pada dataset69, dataset96, dan dataset640, lalu mencatat Z dan runtime tiap metode.

Pemakaian:  python3 run_all.py [dataset69 dataset96 dataset640] [--rep N]
Hasil   :  hasil/<dataset>_Z.csv, hasil/<dataset>_runtime_ms.csv, hasil/<dataset>_gagal.txt
"""
import os, re, sys, json, time, shutil, subprocess, tempfile, statistics, resource

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, 'bin')
DATA = os.path.join(HERE, 'data')

# nama tampilan, binary, cara input, pola hasil
METHODS = [
    ('NWCM',      'NWCM_FIX',        'file',  r'Cost=\s*(-?\d+)'),
    ('LCM',       'LCM2',            'file',  r'Biaya Minimumnya\s*:\s*(-?\d+)'),
    ('VAM',       'VAM',             'file',  r'Total Transportation Cost\s*=\s*(-?\d+)'),
    ('JHM',       'JHM_langsung',    'file',  r'total biaya\s*=\s*(-?\d+)'),
    ('TOCM-MT',   'TOCM-TDM1',       'file',  r'total biaya\s*=\s*(-?\d+)'),
    ('BCE',       'BCE',             'file',  r'total biaya\s*=\s*(-?\d+)'),
    ('SSM',       'SSM',             'file',  r'Total Alokasi tabel adalah:\s*(-?\d+)'),
    ('SSM (original)', 'SSM-original', 'file', r'Total Alokasi tabel adalah:\s*(-?\d+)'),
    ('SAM (Modif SSM)', 'SAM',       'file',  r'Total Alokasi tabel adalah:\s*(-?\d+)'),
    ('CSM',       'CSM2',            'menu',  r'TCC\s*=\s*(-?\d+)'),
    ('RBSM',      'RBSM',            'menu',  r'TCC\s*=\s*(-?\d+)'),
    ('FVAM',      'fractional_vam2', 'menu',  r'TCC\s*=\s*(-?\d+)'),
    ('PowerRank', 'PowerRank',       'menu',  r'TCC\s*=\s*(-?\d+)'),
    ('IHW',       'IHW',             'ihw',   r'Total biaya transportasi dari metode yang dipilih adalah:\s*(-?\d+)'),
    ('EXP-ICHW',  'EXP-ICHW',        'exp',   r'Total biaya \(menggunakan TCM asli\):\s*(-?\d+)'),
    ('RCWMCAM',   'RCWMCAM',         'rcw',   r'TOTAL COST\s*=\s*(-?\d+)'),
    ('MRM',       'MRM',             'str',   r'Total Cost:\s*(-?\d+)'),
    ('AMM',       'AMM',             'str',   r'Total IBFS Cost using Sandwich Method:\s*(-?\d+)'),
    ('HR-FVAM',   'HRFVAM',          'arg',   r'TCC\s*=\s*(-?\d+)'),
    ('THP',       'ibfs',            'ibfs:THP',    r'Z\s*=\s*(-?\d+)'),
    ('TOCM-SC',   'ibfs',            'ibfs:TOCMSC', r'Z\s*=\s*(-?\d+)'),
    ('TMA-VAM',   'ibfs',            'ibfs:TMAVAM', r'Z\s*=\s*(-?\d+)'),
]
# implementasi ulang di ibfs.c, hanya untuk cek silang dengan kode asli
CHECK = [('ibfs VAM', 'VAM'), ('ibfs SSM', 'SSM'), ('ibfs Modif SSM', 'MSSM'), ('ibfs CSM', 'CSM'), ('ibfs RBSM', 'RBSM')]


def read(p):
    v = open(p).read().split(); m, n = int(v[0]), int(v[1])
    C = v[2:2 + m * n]; S = v[2 + m * n:2 + m * n + m]; D = v[2 + m * n + m:2 + m * n + m + n]
    return m, n, C, S, D


def run_one(mode, binary, pat, file_bal, file_orig, opt, tmp):
    """jalankan sekali, kembalikan (Z, detik)"""
    shutil.copy(file_bal, os.path.join(tmp, 'input.txt'))
    exe = os.path.join(BIN, binary); args = ['stdbuf', '-oL', exe]; stdin = ''   # stdbuf: hasil tetap tercatat walau program crash setelahnya
    if mode == 'file':
        pass
    elif mode == 'menu':
        stdin = '1\n3\n'
    elif mode == 'ihw':
        stdin = '1\ninput.txt\n'
    elif mode == 'rcw':
        stdin = '1\ninput.txt\n'
    elif mode == 'exp':          # EXP-ICHW butuh angka hasil di akhir file
        with open(os.path.join(tmp, 'input.txt'), 'a') as f: f.write(f'\n{opt if opt is not None else 0}\n')
        stdin = '1\ninput.txt\nexit\n'
    elif mode == 'str':
        m, n, C, S, D = read(file_bal)
        stdin = '; '.join(' '.join(C[i * n:(i + 1) * n]) for i in range(m)) + '\n' + ', '.join(S) + '\n' + ', '.join(D) + '\n'
    elif mode == 'arg':
        args = ['stdbuf', '-oL', exe, 'input.txt', '-q']
    elif mode.startswith('ibfs:'):
        args = [exe, file_orig, '-z', mode.split(':')[1]]
    def lim():   # batasi keluaran (program dengan loop tak berhenti) dan memori
        resource.setrlimit(resource.RLIMIT_FSIZE, (64 << 20, 64 << 20))
        resource.setrlimit(resource.RLIMIT_AS, (2 << 30, 2 << 30))
    outp = os.path.join(tmp, 'stdout.txt')
    t0 = time.perf_counter()
    try:
        with open(outp, 'wb') as fo:
            r = subprocess.run(args, input=stdin.encode(), cwd=tmp, stdout=fo, stderr=subprocess.DEVNULL, timeout=20, preexec_fn=lim)
    except subprocess.TimeoutExpired:
        return None, None, 'timeout'
    dt = time.perf_counter() - t0
    with open(outp, 'rb') as fo: out = fo.read(64 << 20).decode('latin-1')
    mm = re.search(pat, out)
    if not mm: return None, dt, f'tidak ada hasil (exit {r.returncode})'
    return int(mm.group(1)), dt, None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--') and not a.isdigit()]
    rep = 3
    if '--rep' in sys.argv: rep = int(sys.argv[sys.argv.index('--rep') + 1])
    datasets = args or ['dataset69', 'dataset96', 'dataset640']
    info = json.load(open(os.path.join(DATA, 'info.json')))
    os.makedirs(os.path.join(HERE, 'hasil'), exist_ok=True)
    allm = METHODS + [(n, 'ibfs', 'ibfs:' + k, r'Z\s*=\s*(-?\d+)') for n, k in CHECK]
    for ds in datasets:
        names = sorted(info[ds])
        Z = {}; T = {}; fail = []
        tmp = tempfile.mkdtemp()
        cache = os.path.join(HERE, 'hasil', f'{ds}_cache.jsonl')   # agar bisa dilanjutkan bila terhenti
        done = {}
        if os.path.exists(cache):
            for line in open(cache):
                d = json.loads(line); done[d['name']] = d
        for k, name in enumerate(names):
            if name in done:
                for mn in done[name]['Z']: Z[(name, mn)] = done[name]['Z'][mn]; T[(name, mn)] = done[name]['T'][mn]
                fail += done[name]['fail']
                continue
            nf0 = len(fail)
            fb = os.path.join(DATA, ds + '_bal', name + '.txt'); fo = os.path.join(DATA, ds, name + '.txt')
            opt = info[ds][name]['opt_lp']
            for (mn, b, mode, pat) in allm:
                best = None; z0 = None
                for _ in range(rep if not mn.startswith('ibfs ') else 1):
                    z, dt, err = run_one(mode, b, pat, fb, fo, opt, tmp)
                    if err: fail.append(f'{name}\t{mn}\t{err}'); z0 = None; break
                    z0 = z; best = dt if best is None else min(best, dt)
                Z[(name, mn)] = z0; T[(name, mn)] = best
            with open(cache, 'a') as fc:
                fc.write(json.dumps(dict(name=name, Z={m[0]: Z[(name, m[0])] for m in allm}, T={m[0]: T[(name, m[0])] for m in allm}, fail=fail[nf0:])) + '\n')
            if (k + 1) % 50 == 0: print(f'  {ds}: {k + 1}/{len(names)}', flush=True)
        shutil.rmtree(tmp)
        cols = [m[0] for m in allm]
        with open(os.path.join(HERE, 'hasil', f'{ds}_Z.csv'), 'w') as f:
            f.write('Soal,Ukuran,Seimbang,Optimal,' + ','.join(cols) + '\n')
            for name in names:
                i = info[ds][name]
                f.write(f"{name},{i['m']}x{i['n']},{'ya' if i['bal'] else 'tidak'},{i['opt_lp']}," +
                        ','.join('' if Z[(name, c)] is None else str(Z[(name, c)]) for c in cols) + '\n')
        with open(os.path.join(HERE, 'hasil', f'{ds}_runtime_ms.csv'), 'w') as f:
            f.write('Soal,Ukuran,' + ','.join(m[0] for m in METHODS) + '\n')
            for name in names:
                i = info[ds][name]
                f.write(f"{name},{i['m']}x{i['n']}," + ','.join('' if T[(name, c)] is None else f'{T[(name, c)] * 1000:.3f}' for c in [m[0] for m in METHODS]) + '\n')
        with open(os.path.join(HERE, 'hasil', f'{ds}_gagal.txt'), 'w') as f: f.write('\n'.join(fail) + '\n')
        print(ds, 'selesai,', len(fail), 'kegagalan')


if __name__ == '__main__':
    main()
