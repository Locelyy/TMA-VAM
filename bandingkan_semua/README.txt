BANDINGKAN SEMUA METODE - TMA-VAM vs 21 metode IBFS lain

Kebutuhan: gcc, g++, Python 3, scipy (untuk nilai optimal), Linux atau macOS.

  ./build.sh                                   salin src/ ke patched/ lalu kompilasi 20 program ke bin/
  python3 prep.py dataset69 dataset96 dataset640 kasus
                                               siapkan data/<dataset>/ dan data/<dataset>_bal/, hitung optimal (LP)
  python3 run_all.py dataset69 --rep 3         jalankan 22 metode, simpan hasil/<dataset>_Z.csv dan _runtime_ms.csv
  python3 ringkasan.py dataset69               akurasi, gap, solusi tidak layak, menang/seri/kalah, runtime
  python3 satu_soal.py data/dataset69/G3_N33.txt

src/        kode asli dari folder Program IBFS (tidak diubah), HRFVAM.c, dan ibfs.c
patch_sources.py  daftar SEMUA perubahan pada kode asli. Hanya:
            - nama file input diganti input.txt
            - SSM.c, SSM-original.c: total_alloc diinisialisasi (total tercetak acak)
            - SSM-original, SAM, LCM, NWCM, JHM: berhenti setelah satu soal (loop while(!feof)
              membaca ulang file dan memproses data sampah)
            - CSM, RBSM: "Soal Salah" diganti mencetak total bila alokasi FLC awal sudah layak
            - LCM, NWCM, RCWMCAM: header khusus Windows dihapus, limits.h ditambah
data/       <dataset>_raw = file asli; info.json = ukuran, status seimbang, nilai optimal
hasil/      hasil lengkap (CSV per soal, ringkasan JSON)

Kode pembanding menerima soal yang sudah seimbang (dummy biaya 0). TMA-VAM, THP, TOCM-SC
menerima soal asli lewat ibfs.c. Z di bawah nilai optimal ditandai "tidak layak".

  python3 uji_kompleksitas.py                  percobaan kompleksitas waktu (soal acak 10x10 sampai 100x100)
