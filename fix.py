import re, sys

src = sys.argv[1]
out = src.replace('.tex', '_fixed.tex')
t = open(src, encoding='utf-8').read().replace('\r\n', '\n')

def rep(pattern, repl, label, flags=0):
    global t
    t, n = re.subn(pattern, repl, t, flags=flags)
    print(('OK   ' if n == 1 else 'CEK  ') + f'{label}: {n} perubahan')

def ws(s):  # cocokkan teks walau beda baris/spasi
    return r'\s+'.join(re.escape(w) for w in s.split())

CELL = (r'\s*\\end\{minipage\}\s*&\s*\\begin\{minipage\}\[b\]\{\\linewidth\}'
        r'\\centering\s*')

# 1. Tabel 1: peringkat Dataset640
rep(re.escape(r'9 (menurut gap)'), lambda m: r'8 (menurut gap)', 'Tabel 1 peringkat D640')

# 2. Tabel 12: baris BCE
rep(re.escape(r'2,78\%'), lambda m: r'2,69\%', 'Tabel 12 gap BCE')
rep(re.escape(r'23 / 40 / 4'), lambda m: r'22 / 40 / 4', 'Tabel 12 menang/seri/kalah BCE')

# 3. Tabel 15: gap BCE di Dataset640 (cari lewat 65,9 + 45,5 khas baris BCE)
rep(r'(65,9\\%' + CELL + r'45,5\\%' + CELL + r')0,52(\\%)',
    lambda m: m.group(1) + '2,26' + m.group(2), 'Tabel 15 D640 gap BCE')

# 3. Tabel 5: BCE tidak layak di Dataset640
rep(r'(G2\\_P12, G8\\_S01.*?)7 soal',
    lambda m: m.group(1) + '1 soal', 'Tabel 5 BCE D640', flags=re.S)

# ---- fungsi untuk ganti sel per baris tabel ----
def setcells(body, newvals):
    i = [-1]
    def f(m):
        i[0] += 1
        if i[0] in newvals:
            isi = re.sub(r'\d[\d,]*', lambda _: newvals[i[0]], m.group(2), count=1)
            return m.group(1) + isi + m.group(3)
        return m.group(0)
    return re.sub(r'(\\centering\n)([^\n]*)(\n\\end)', f, body)

def fix_table(start, end, data, label):
    global t
    a = t.index(start)
    b = t.index(end, a)
    reg = t[a:b]
    jumlah = 0
    for name, newvals in data.items():
        pat = (r'(\\raggedright\n(?:\\textbf\{)?' + re.escape(name) +
               r'\}?\n\\end\{minipage\})(.*?)( \\\\\n)')
        def f(m):
            return m.group(1) + setcells(m.group(2), newvals) + m.group(3)
        reg, n = re.subn(pat, f, reg, count=1, flags=re.S)
        jumlah += n
        if n != 1:
            print(f'CEK  {label}: baris {name} tidak ketemu')
    t = t[:a] + reg + t[b:]
    print(f'OK   {label}: {jumlah}/{len(data)} baris diubah')

# 3b. Tabel 17 (cell 0 = median, 1 = rata-rata D69, 2 = rata-rata D640)
t17 = {
 'TMA-VAM': ('3,38', '4,27'), 'SAM (Modif SSM)': ('6,03', '6,35'),
 'HR-FVAM': ('5,78', '6,37'), 'SSM (original)': ('5,70', '6,27'),
 'SSM': ('5,58', '6,62'), 'RBSM': ('6,28', '6,73'), 'CSM': ('6,20', '6,74'),
 'TOCM-MT': ('6,07', '11,36'), 'BCE': ('6,13', '8,84'), 'JHM': ('5,76', '6,27'),
 'IHW': ('9,52', '34,41'), 'AMM': ('5,67', '7,45'), 'VAM': ('5,07', '5,85'),
 'EXP-ICHW': ('6,09', '8,71'), 'MRM': ('5,68', '6,48'), 'TOCM-SC': ('3,36', '3,67'),
 'PowerRank': ('4,80', '5,99'), 'THP': ('3,43', '4,24'), 'FVAM': ('5,89', '8,52'),
 'RCWMCAM': ('5,88', '7,54'), 'LCM': ('5,64', '6,53'), 'NWCM': ('5,63', '7,82'),
}
fix_table(r'\textbf{Dataset640 rata-rata (ms)}', r'\emph{Tabel 17.',
          {k: {1: v[0], 2: v[1]} for k, v in t17.items()}, 'Tabel 17')

rep(ws('butuh sekitar 5 ms'), lambda m: 'butuh sekitar 5--6 ms', 'teks Tabel 17 (5 ms)')
rep(ws('(32 ms rata-rata)'), lambda m: '(34 ms rata-rata)', 'teks Tabel 17 (IHW)')

# 4. Tabel 18 (cell 0 = rata-rata)
t18 = {'VAM': '27,52', 'SSM': '2,97', 'Modif SSM': '2,86', 'CSM': '3,22',
       'RBSM': '3,28', 'THP': '21,54', 'TOCM-SC': '6,60', 'TMA-VAM': '27,14'}
fix_table(r'\textbf{Metode (ibfs.c)}', r'\emph{Tabel 18.',
          {k: {0: v} for k, v in t18.items()}, 'Tabel 18')

# Teks yang ikut berubah akibat Tabel 18
rep(ws('TMA-VAM rata-rata 15,83 μs per soal Dataset69, lebih lama dari keluarga SSM (sekitar 2 μs) karena nilai T dihitung ulang tiap iterasi.'),
    lambda m: ('TMA-VAM rata-rata 27,14 μs per soal Dataset69, hampir sama dengan VAM '
               '(27,52 μs) dan lebih lama dari keluarga SSM (sekitar 3 μs) karena nilai '
               'T dihitung ulang tiap iterasi.'), 'Ringkasan: runtime')
rep(ws('Keluarga SSM paling cepat (sekitar 2 μs)'),
    lambda m: 'Keluarga SSM paling cepat (sekitar 3 μs)', 'Bagian 6.3 butir 1')
rep(ws('TMA-VAM paling lambat (15,83 μs rata-rata, 274,51 μs pada soal 30 × 32) karena setiap iterasi menghitung ulang nilai T, rata-rata T, dan skor S. Tetap jauh di bawah satu milidetik.'),
    lambda m: ('TMA-VAM dan VAM paling lambat (27,14 dan 27,52 μs rata-rata) karena TMA-VAM '
               'menghitung ulang nilai T, rata-rata T, dan skor S di setiap iterasi. '
               'Tetap jauh di bawah satu milidetik.'), 'Bagian 6.3 butir 2')
rep(ws('serta paling lambat di antara metode di ibfs.c'),
    lambda m: 'serta, bersama VAM, paling lambat di antara metode di ibfs.c', 'Kesimpulan')

open(out, 'w', encoding='utf-8').write(t)
print('Selesai ->', out)