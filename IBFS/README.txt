PROGRAM IBFS - TMA-VAM (Riset Operasi, Teknik Informatika ITS, Kelompok 6)

Isi folder ini:
  TMA-VAM.c  program TMA-VAM saja (terpisah)
  ibfs.c     TMA-VAM bersama 7 metode lain untuk perbandingan langsung
 Perbandingan dengan SEMUA metode di folder
Program IBFS ada di folder ../bandingkan_semua.

0. Program TMA-VAM saja
   gcc -O2 -o tmavam TMA-VAM.c -lm
   ./tmavam                                 program menanyakan nama file
   ./tmavam dataset69/G3_N33.txt            langkah demi langkah + hasil akhir + runtime
   ./tmavam dataset69/G3_N33.txt -q         hanya hasil akhir
   ./tmavam -d dataset69 optimal69.txt      semua soal di folder: Z, optimal, gap, runtime

1. Kompilasi ibfs.c
   gcc -O2 -o ibfs ibfs.c -lm

2. Menjalankan
   ./ibfs dataset69/G3_N33.txt              Z dan runtime 8 metode
   ./ibfs dataset69/G3_N33.txt -v TMAVAM    langkah demi langkah
                                            (VAM, SSM, MSSM, CSM, RBSM, THP, TOCMSC, TMAVAM, ALL)
   ./ibfs dataset69/G3_N33.txt -z TMAVAM    hanya mencetak Z (dipakai run_all.py)
   ./ibfs -d dataset69 optimal69.txt        seluruh dataset, tulis hasil.csv dan runtime.csv
   ./ibfs -d dataset96 optimal96.txt

3. Metode di ibfs.c
   TMA-VAM (metode usulan), THP dan TOCM-SC (diprogram dari paper, tidak ada kodenya di folder
   Program IBFS), serta VAM, SSM, Modif SSM, CSM, RBSM menurut aturan Word Bu Bilqis.

4. Isi folder
   dataset69/, dataset96/   file soal (dataset69 asli, nilai optimal tertulis di akhir file)
   optimal69.txt            nilai optimal dari program linear. G6_N04 = 1580; nilai di file (555) salah.
   optimal96.txt            nilai optimal dataset96 dari program linear
   validasi/                contoh soal paper: THP (1045, 4525, 2366), TOCM-SC (799)
   contoh_paper_ssm.txt     contoh paper SSM (SSM = 465, VAM = 473)
   run_dataset69.txt        salinan hasil ./ibfs -d dataset69 optimal69.txt
   hasil_ibfs_dataset69.csv, runtime_ibfs_dataset69_us.csv
   capture/                 gambar capture hasil run
   flowchart_*.png          flowchart TMA-VAM dan flowchart program
