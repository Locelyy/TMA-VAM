#!/usr/bin/env python3
"""prep.py - menyiapkan dataset untuk run_all.py.
Untuk setiap folder data/<dataset>_raw/ dibuat:
  data/<dataset>/      soal tanpa nilai optimal di akhir file (untuk ibfs.c / TMA-VAM)
  data/<dataset>_bal/  soal yang sudah seimbang (dummy biaya 0) untuk kode pembanding
dan data/info.json berisi ukuran, status seimbang, nilai optimal di file, dan nilai optimal
hasil program linear (scipy). Pemakaian: python3 prep.py dataset69 dataset96 dataset640 kasus"""
import glob, os, json, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load(p):
    v = open(p).read().split(); m, n = int(v[0]), int(v[1]); k = 2
    C = [list(map(int, v[k + i * n:k + (i + 1) * n])) for i in range(m)]; k += m * n
    S = list(map(int, v[k:k + m])); k += m; D = list(map(int, v[k:k + n])); k += n
    opt = int(float(v[k])) if len(v) > k else None
    return C, S, D, opt


def write(p, C, S, D):
    with open(p, 'w') as f:
        f.write(f"{len(C)} {len(C[0])}\n\n")
        for r in C: f.write(' '.join(map(str, r)) + '\n')
        f.write('\n' + ' '.join(map(str, S)) + '\n\n' + ' '.join(map(str, D)) + '\n')


def balance(C, S, D):
    Cb, Sb, Db = [r[:] for r in C], S[:], D[:]
    if sum(S) > sum(D): [r.append(0) for r in Cb]; Db.append(sum(S) - sum(D))
    elif sum(D) > sum(S): Cb.append([0] * len(D)); Sb.append(sum(D) - sum(S))
    return Cb, Sb, Db


def optimum(C, S, D):
    """nilai optimal dengan program linear (scipy.optimize.linprog, metode HiGHS)"""
    import numpy as np
    from scipy.optimize import linprog
    C, S, D = balance(C, S, D); m, n = len(C), len(C[0])
    A = []; b = []
    for i in range(m):
        r = np.zeros(m * n); r[i * n:(i + 1) * n] = 1; A.append(r); b.append(S[i])
    for j in range(n):
        r = np.zeros(m * n); r[j::n] = 1; A.append(r); b.append(D[j])
    res = linprog(np.array(C, float).ravel(), A_eq=np.array(A)[:-1], b_eq=np.array(b)[:-1], method='highs')
    return round(res.fun)


if __name__ == '__main__':
    path = os.path.join(HERE, 'data', 'info.json')
    info = json.load(open(path)) if os.path.exists(path) else {}
    for ds in sys.argv[1:]:
        os.makedirs(os.path.join(HERE, 'data', ds), exist_ok=True); os.makedirs(os.path.join(HERE, 'data', ds + '_bal'), exist_ok=True)
        info[ds] = {}
        for p in sorted(glob.glob(os.path.join(HERE, 'data', ds + '_raw', '*.txt'))):
            name = os.path.basename(p)[:-4]
            C, S, D, opt = load(p)
            write(os.path.join(HERE, 'data', ds, name + '.txt'), C, S, D)
            write(os.path.join(HERE, 'data', ds + '_bal', name + '.txt'), *balance(C, S, D))
            info[ds][name] = dict(m=len(C), n=len(C[0]), bal=sum(S) == sum(D), opt_file=opt, opt_lp=optimum(C, S, D))
        mism = [(k, v['opt_file'], v['opt_lp']) for k, v in info[ds].items() if v['opt_file'] is not None and v['opt_file'] != v['opt_lp']]
        print(ds, len(info[ds]), 'soal, tak seimbang:', sum(not v['bal'] for v in info[ds].values()),
              '| nilai optimal di file berbeda dari LP:', mism)
    json.dump(info, open(path, 'w'), indent=0)
