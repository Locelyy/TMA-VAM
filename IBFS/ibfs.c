/*
 * ibfs.c  -  Program IBFS (Initial Basic Feasible Solution) Masalah Transportasi
 *            Riset Operasi, Teknik Informatika ITS - Kelompok 6
 *
 * Metode yang diprogram:
 *   1. VAM        Vogel's Approximation Method (Taha, 2007)
 *   2. SSM        Supply Selection Method (Amaliah, Fatichah & Suryani, 2022)
 *   3. MSSM       Modif SSM / Supply Average Method, AS = rata-rata biaya x supply
 *   4. CSM        Cost Supply Method, TC = total biaya baris, TCS = TC x supply
 *   5. RBSM       Rihan-Bilqis-Saikhu Method, TC = total biaya baris
 *   6. THP        Two Highest Penalties (Amaliah, Fatichah & Suryani, 2021)
 *   7. TOCM-SC    Total Opportunity Cost Matrix - Supreme Cell (Amaliah dkk., 2020)
 *   8. TMA-VAM    metode usulan kelompok 6
 *   Aturan SSM, MSSM, CSM, RBSM mengikuti dokumen "perbedaan SSM, CSM, RBSM dan Modif SSM".
 *
 * Kompilasi :  gcc -O2 -o ibfs ibfs.c -lm
 *
 * Cara pakai:
 *   ./ibfs soal.txt                    semua metode, hasil akhir + runtime
 *   ./ibfs soal.txt -v TMAVAM          langkah demi langkah satu metode
 *                                      (VAM, SSM, MSSM, CSM, RBSM, THP, TOCMSC, TMAVAM, ALL)
 *   ./ibfs soal.txt -z TMAVAM          hanya mencetak Z satu metode (dipakai skrip pembanding)
 *   ./ibfs -d dataset69                uji seluruh dataset (optimal dibaca dari akhir file
 *                                      atau dari file optimal: ./ibfs -d dataset96 optimal96.txt)
 *
 * Format file soal:
 *   m n
 *   c11 c12 ... c1n
 *   ...
 *   cm1 ... cmn
 *   s1 ... sm
 *   d1 ... dn
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>

#define MAXN 128
#define EPS 1e-9
#define NMETHOD 8

typedef struct {
    int m, n;                 /* ukuran setelah diseimbangkan            */
    int m0, n0;               /* ukuran asli                             */
    double c[MAXN][MAXN];
    double s[MAXN], d[MAXN];
    int dummyRow, dummyCol;   /* indeks baris/kolom dummy, -1 jika tidak ada */
    double opt;               /* nilai optimal bila tertulis di akhir file, -1 jika tidak ada */
} Problem;

static const char *MNAME[NMETHOD] = {"VAM", "SSM", "MSSM", "CSM", "RBSM", "THP", "TOCMSC", "TMAVAM"};
static const char *MLONG[NMETHOD] = {"VAM", "SSM", "Modif SSM", "CSM", "RBSM", "THP", "TOCM-SC", "TMA-VAM"};
static int VERBOSE = 0;
#define V if (VERBOSE) printf

/* ------------------------------------------------------------------ utilitas */
static int read_problem(const char *path, Problem *P) {
    FILE *f = fopen(path, "r");
    if (!f) { printf("File %s tidak bisa dibuka\n", path); return 0; }
    if (fscanf(f, "%d %d", &P->m0, &P->n0) != 2) { fclose(f); return 0; }
    int m = P->m0, n = P->n0;
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) if (fscanf(f, "%lf", &P->c[i][j]) != 1) { fclose(f); return 0; }
    for (int i = 0; i < m; i++) if (fscanf(f, "%lf", &P->s[i]) != 1) { fclose(f); return 0; }
    for (int j = 0; j < n; j++) if (fscanf(f, "%lf", &P->d[j]) != 1) { fclose(f); return 0; }
    if (fscanf(f, "%lf", &P->opt) != 1) P->opt = -1;   /* dataset69/640: optimal di akhir file */
    fclose(f);
    /* seimbangkan: tambah dummy berbiaya 0 */
    double ts = 0, td = 0;
    for (int i = 0; i < m; i++) ts += P->s[i];
    for (int j = 0; j < n; j++) td += P->d[j];
    P->dummyRow = P->dummyCol = -1;
    if (ts > td + EPS) { for (int i = 0; i < m; i++) P->c[i][n] = 0; P->d[n] = ts - td; P->dummyCol = n; n++; }
    else if (td > ts + EPS) { for (int j = 0; j < n; j++) P->c[m][j] = 0; P->s[m] = td - ts; P->dummyRow = m; m++; }
    P->m = m; P->n = n;
    return 1;
}

static void rname(const Problem *P, int i, char *b) { if (i == P->dummyRow) sprintf(b, "Sd"); else sprintf(b, "S%d", i + 1); }
static void cname(const Problem *P, int j, char *b) { if (j == P->dummyCol) sprintf(b, "Dd"); else sprintf(b, "D%d", j + 1); }
static const char *cell(const Problem *P, int i, int j) {
    static char buf[8][24]; static int k = 0; char a[8], b[8];
    k = (k + 1) & 7; rname(P, i, a); cname(P, j, b); sprintf(buf[k], "(%s,%s)", a, b); return buf[k];
}
static const char *num(double x) {          /* cetak angka: bulat tanpa desimal */
    static char buf[16][32]; static int k = 0; k = (k + 1) & 15;
    if (fabs(x - llround(x)) < 1e-9) sprintf(buf[k], "%lld", (long long)llround(x));
    else sprintf(buf[k], "%.2f", x);
    return buf[k];
}
static double total_cost(const Problem *P, double X[MAXN][MAXN]) {
    double z = 0;
    for (int i = 0; i < P->m; i++) for (int j = 0; j < P->n; j++) z += P->c[i][j] * X[i][j];
    return z;
}

/* kosongkan alokasi (hanya ukuran m x n agar runtime tidak terdistorsi) */
static void clear_x(const Problem *P, double X[MAXN][MAXN]) {
    for (int i = 0; i < P->m; i++) for (int j = 0; j < P->n; j++) X[i][j] = 0;
}

/* cek kelayakan: jumlah alokasi tiap baris = supply, tiap kolom = demand */
static int feasible(const Problem *P, double X[MAXN][MAXN]) {
    for (int i = 0; i < P->m; i++) { double t = 0; for (int j = 0; j < P->n; j++) { if (X[i][j] < -EPS) return 0; t += X[i][j]; } if (fabs(t - P->s[i]) > 1e-6) return 0; }
    for (int j = 0; j < P->n; j++) { double t = 0; for (int i = 0; i < P->m; i++) t += X[i][j]; if (fabs(t - P->d[j]) > 1e-6) return 0; }
    return 1;
}

/* tabel biaya [alokasi] dengan kolom tambahan opsional */
static void print_table(const Problem *P, double X[MAXN][MAXN], const double *extra, const char *extraName,
                        const double *sup, const double *dem, int showTA) {
    char b[8];
    printf("        ");
    for (int j = 0; j < P->n; j++) { cname(P, j, b); printf("%14s", b); }
    printf("%9s", "Supply");
    if (showTA) printf("%8s %-6s", "TA", "Status");
    if (extra) printf("%10s", extraName);
    printf("\n");
    for (int i = 0; i < P->m; i++) {
        rname(P, i, b); printf("  %-5s ", b);
        double ta = 0;
        for (int j = 0; j < P->n; j++) {
            char t[40];
            if (X[i][j] > EPS) sprintf(t, "%s[%s]", num(P->c[i][j]), num(X[i][j]));
            else sprintf(t, "%s", num(P->c[i][j]));
            printf("%14s", t); ta += X[i][j];
        }
        printf("%9s", num(sup[i]));
        if (showTA) {
            const char *st = ta > sup[i] + EPS ? "ER" : (fabs(ta - sup[i]) < EPS ? "S" : "NS");
            printf("%8s %-6s", num(ta), st);
        }
        if (extra) printf("%10s", num(extra[i]));
        printf("\n");
    }
    printf("  %-5s ", "Dmd");
    for (int j = 0; j < P->n; j++) printf("%14s", num(dem[j]));
    printf("\n");
}

/* ================================================================== 1. VAM */
static double run_vam(const Problem *P, double X[MAXN][MAXN]) {
    int m = P->m, n = P->n, it = 0;
    double s[MAXN], d[MAXN];
    memcpy(s, P->s, sizeof s); memcpy(d, P->d, sizeof d);
    clear_x(P, X);
    while (1) {
        int any = 0;
        for (int i = 0; i < m; i++) if (s[i] > EPS) any = 1;
        if (!any) break;
        it++;
        /* penalti = selisih dua biaya terkecil; garis terpilih = penalti terbesar,
           seri -> biaya sel terkecil -> alokasi terbesar -> baris dulu, indeks kecil */
        double bp = -1, bc = 0, bq = 0; int bi = -1, bj = -1, bt = 0, bk = -1;
        for (int t = 0; t < 2; t++) {
            int L = t == 0 ? m : n;
            for (int k = 0; k < L; k++) {
                if (t == 0 ? s[k] <= EPS : d[k] <= EPS) continue;
                double m1 = 1e300, m2 = 1e300; int ci = -1, cj = -1;
                int L2 = t == 0 ? n : m;
                for (int l = 0; l < L2; l++) {                 /* dua biaya terkecil */
                    int i = t == 0 ? k : l, j = t == 0 ? l : k;
                    if (s[i] <= EPS || d[j] <= EPS) continue;
                    double v = P->c[i][j];
                    if (v < m1) { m2 = m1; m1 = v; } else if (v < m2) m2 = v;
                }
                for (int l = 0; l < L2; l++) {                 /* sel termurah, seri -> alokasi terbesar */
                    int i = t == 0 ? k : l, j = t == 0 ? l : k;
                    if (s[i] <= EPS || d[j] <= EPS || fabs(P->c[i][j] - m1) > EPS) continue;
                    if (ci < 0 || fmin(s[i], d[j]) > fmin(s[ci], d[cj]) + EPS) { ci = i; cj = j; }
                }
                double p = m2 > 1e299 ? m1 : m2 - m1;
                double q = fmin(s[ci], d[cj]);
                int better = 0;
                if (p > bp + EPS) better = 1;
                else if (fabs(p - bp) < EPS) {
                    if (P->c[ci][cj] < bc - EPS) better = 1;
                    else if (fabs(P->c[ci][cj] - bc) < EPS && q > bq + EPS) better = 1;
                }
                if (better) { bp = p; bc = P->c[ci][cj]; bq = q; bi = ci; bj = cj; bt = t; bk = k; }
            }
        }
        double q = fmin(s[bi], d[bj]);
        if (VERBOSE) { char b[8]; if (bt == 0) rname(P, bk, b); else cname(P, bk, b);
            printf("Iterasi %d: penalti terbesar %s di garis %s, sel termurah %s biaya %s, alokasi %s\n",
                   it, num(bp), b, cell(P, bi, bj), num(P->c[bi][bj]), num(q)); }
        X[bi][bj] += q; s[bi] -= q; d[bj] -= q;
    }
    return total_cost(P, X);
}

/* ================================================================== 2-5. Keluarga SSM */
enum { F_SSM = 1, F_MSSM, F_CSM, F_RBSM };

static double run_ssm_family(const Problem *P, double X[MAXN][MAXN], int var) {
    int m = P->m, n = P->n;
    double W[MAXN], TCS[MAXN];    /* W = bobot pemutus seri (TC atau AS) */
    const char *wname = NULL;
    for (int i = 0; i < m; i++) {
        double tc = 0; for (int j = 0; j < n; j++) tc += P->c[i][j];
        if (var == F_MSSM) W[i] = tc / n * P->s[i];
        else W[i] = tc;
        TCS[i] = tc * P->s[i];
    }
    if (var == F_MSSM) wname = "AS"; else if (var == F_CSM || var == F_RBSM) wname = "TC";
    clear_x(P, X);

    /* Langkah: FLC tiap kolom, alokasikan seluruh demand ke FLC */
    V("Langkah awal: FLC tiap kolom (seri -> %s), seluruh demand dialokasikan ke FLC\n",
      var == F_SSM ? "indeks baris terkecil" : (var == F_MSSM ? "AS terbesar" : "TC terbesar"));
    for (int j = 0; j < n; j++) {
        int bi = 0;
        for (int i = 1; i < m; i++) {
            if (P->c[i][j] < P->c[bi][j] - EPS) bi = i;
            else if (var != F_SSM && fabs(P->c[i][j] - P->c[bi][j]) < EPS && W[i] > W[bi] + EPS) bi = i;
        }
        X[bi][j] = P->d[j];
        V("  kolom %d: FLC = %s di %s, alokasi %s\n", j + 1, num(P->c[bi][j]), cell(P, bi, j), num(P->d[j]));
    }
    if (VERBOSE && var == F_CSM) { printf("  TCS tiap baris:"); for (int i = 0; i < m; i++) printf(" %s", num(TCS[i])); printf("\n"); }

    for (int it = 1; it <= 10000; it++) {
        double TA[MAXN]; int er = 0, isER[MAXN], isS[MAXN];
        for (int i = 0; i < m; i++) {
            TA[i] = 0; for (int j = 0; j < n; j++) TA[i] += X[i][j];
            isER[i] = TA[i] > P->s[i] + EPS; isS[i] = fabs(TA[i] - P->s[i]) < EPS; er += isER[i];
        }
        if (VERBOSE) { printf("\nIterasi %d\n", it); print_table(P, X, wname ? W : NULL, wname, P->s, P->d, 1); }
        if (!er) { V("Tidak ada baris ER lagi, alokasi selesai.\n"); break; }

        /* kandidat: setiap sel teralokasi di baris ER, cari SLC (bukan baris ER, bukan baris S) */
        int cr[MAXN * MAXN], cs_[MAXN * MAXN], cj[MAXN * MAXN], nc = 0;
        double cdiff[MAXN * MAXN];
        for (int r = 0; r < m; r++) {
            if (!isER[r]) continue;
            for (int j = 0; j < n; j++) {
                if (X[r][j] <= EPS) continue;
                int sl = -1;
                for (int i = 0; i < m; i++) {
                    if (i == r || isER[i] || isS[i]) continue;
                    if (sl < 0 || P->c[i][j] < P->c[sl][j] - EPS) sl = i;
                    else if (var != F_SSM && fabs(P->c[i][j] - P->c[sl][j]) < EPS && W[i] > W[sl] + EPS) sl = i;
                }
                if (sl < 0) continue;
                cr[nc] = r; cs_[nc] = sl; cj[nc] = j; cdiff[nc] = P->c[sl][j] - P->c[r][j];
                V("  sel ER %s: FLC %s, SLC %s di %s, Diff = %s\n", cell(P, r, j), num(P->c[r][j]),
                  num(P->c[sl][j]), cell(P, sl, j), num(cdiff[nc]));
                nc++;
            }
        }
        if (!nc) { V("Tidak ada SLC yang tersedia.\n"); break; }

        /* aturan pemindahan: target 0 = penuhi baris FLC, 1 = penuhi baris SLC */
        #define TARGET(k) ({ int r_ = cr[k], s2_ = cs_[k], j_ = cj[k], t_; \
            if (fabs(P->s[r_] - P->s[s2_]) < EPS) t_ = 0; \
            else if (var == F_SSM) { if (er == 1) t_ = P->s[r_] < P->s[s2_] ? 0 : 1; else t_ = P->s[r_] > P->s[s2_] ? 0 : 1; } \
            else if (var == F_MSSM) { if (er == 1) t_ = W[r_] < W[s2_] ? 0 : 1; else t_ = W[r_] > W[s2_] ? 0 : 1; } \
            else if (var == F_CSM) t_ = TCS[r_] >= TCS[s2_] ? 0 : 1; \
            else t_ = P->c[r_][j_] * P->s[r_] >= P->c[s2_][j_] * P->s[s2_] ? 0 : 1; \
            t_; })
        #define UNITS(k, t) ({ int r_ = cr[k], s2_ = cs_[k], j_ = cj[k]; double x_ = X[r_][j_]; \
            double exc_ = TA[r_] - P->s[r_], sl_ = P->s[s2_] - TA[s2_], u_; \
            if ((t) == 0) { if (x_ >= exc_ - EPS) u_ = exc_; else if (x_ >= sl_ - EPS) u_ = sl_; else u_ = x_; } \
            else { if (x_ >= sl_ - EPS) u_ = sl_; else u_ = x_; } \
            u_; })

        /* SDiff dan pemutus serinya */
        int best = 0;
        for (int k = 1; k < nc; k++) {
            if (cdiff[k] < cdiff[best] - EPS) { best = k; continue; }
            if (fabs(cdiff[k] - cdiff[best]) > EPS) continue;
            if (var == F_SSM || var == F_MSSM) {          /* unit terbesar pada FLC */
                if (X[cr[k]][cj[k]] > X[cr[best]][cj[best]] + EPS) best = k;
            } else {                                       /* TUC = TU x biaya SLC terkecil */
                double a = UNITS(k, TARGET(k)) * P->c[cs_[k]][cj[k]];
                double b = UNITS(best, TARGET(best)) * P->c[cs_[best]][cj[best]];
                if (a < b - EPS || (fabs(a - b) < EPS && P->c[cs_[k]][cj[k]] < P->c[cs_[best]][cj[best]] - EPS)) best = k;
            }
        }
        int r = cr[best], sl = cs_[best], j = cj[best];
        int t = TARGET(best);
        double u = UNITS(best, t);
        if (VERBOSE) {
            char a[8], b[8]; rname(P, r, a); rname(P, sl, b);
            printf("  SDiff = %s di kolom D%d. Jumlah ER = %d.\n", num(cdiff[best]), j + 1, er);
            if (fabs(P->s[r] - P->s[sl]) < EPS) printf("  Supply baris FLC (%s) = supply baris SLC (%s) -> penuhi baris FLC.\n", a, b);
            else if (var == F_SSM) printf("  Supply %s = %s, supply %s = %s -> penuhi baris %s (supply %s).\n", a, num(P->s[r]), b, num(P->s[sl]), t ? b : a, er == 1 ? "terkecil" : "terbesar");
            else if (var == F_MSSM) printf("  AS %s = %s, AS %s = %s -> penuhi baris %s (AS %s).\n", a, num(W[r]), b, num(W[sl]), t ? b : a, er == 1 ? "terkecil" : "terbesar");
            else if (var == F_CSM) printf("  TCS %s = %s, TCS %s = %s -> penuhi baris %s (TCS terbesar).\n", a, num(TCS[r]), b, num(TCS[sl]), t ? b : a);
            else printf("  cost x supply: %s = %s, %s = %s -> penuhi baris %s (terbesar).\n", a, num(P->c[r][j] * P->s[r]), b, num(P->c[sl][j] * P->s[sl]), t ? b : a);
            printf("  Pindahkan %s unit dari %s ke %s.\n", num(u), cell(P, r, j), cell(P, sl, j));
        }
        if (u <= EPS) u = X[r][j];
        X[r][j] -= u; X[sl][j] += u;
    }
    return total_cost(P, X);
}

/* ================================================================== 6. THP */
static double run_thp(const Problem *P, double X[MAXN][MAXN]) {
    int m = P->m, n = P->n, it = 0;
    double s[MAXN], d[MAXN];
    memcpy(s, P->s, sizeof s); memcpy(d, P->d, sizeof d);
    clear_x(P, X);
    while (1) {
        int any = 0; for (int i = 0; i < m; i++) if (s[i] > EPS) any = 1;
        if (!any) break;
        it++;
        /* penalti = biaya maks - biaya min tiap garis aktif */
        double pen[2 * MAXN]; int typ[2 * MAXN], idx[2 * MAXN], L = 0;
        for (int t = 0; t < 2; t++) for (int k = 0; k < (t ? n : m); k++) {
            if (t ? d[k] <= EPS : s[k] <= EPS) continue;
            double mx = -1e300, mn = 1e300;
            for (int l = 0; l < (t ? m : n); l++) {
                int i = t ? l : k, j = t ? k : l;
                if (s[i] <= EPS || d[j] <= EPS) continue;
                if (P->c[i][j] > mx) mx = P->c[i][j];
                if (P->c[i][j] < mn) mn = P->c[i][j];
            }
            pen[L] = mx - mn; typ[L] = t; idx[L] = k; L++;
        }
        /* THP langkah 3a-b: penalti tertinggi dan penalti tertinggi berikutnya (nilai berbeda),
           semua garis yang nilainya seri ikut dipilih */
        double p1 = -1, p2 = -1;
        for (int a = 0; a < L; a++) if (pen[a] > p1) p1 = pen[a];
        for (int a = 0; a < L; a++) if (pen[a] < p1 - EPS && pen[a] > p2) p2 = pen[a];
        double thr = p2 >= 0 ? p2 : p1;
        double bv = 1e300, bc = 0, bq = 0, bp = 0; int bi = -1, bj = -1;
        for (int a = 0; a < L; a++) {
            if (pen[a] < thr - EPS) continue;
            int t = typ[a], k = idx[a]; double mn = 1e300;
            for (int l = 0; l < (t ? m : n); l++) { int i = t ? l : k, j = t ? k : l; if (s[i] > EPS && d[j] > EPS && P->c[i][j] < mn) mn = P->c[i][j]; }
            for (int l = 0; l < (t ? m : n); l++) {
                int i = t ? l : k, j = t ? k : l;
                if (s[i] <= EPS || d[j] <= EPS || fabs(P->c[i][j] - mn) > EPS) continue;
                double q = fmin(s[i], d[j]), v = P->c[i][j] * q; int better = 0;
                if (v < bv - EPS) better = 1;
                else if (fabs(v - bv) < EPS) {
                    if (P->c[i][j] < bc - EPS) better = 1;
                    else if (fabs(P->c[i][j] - bc) < EPS) {
                        if (q > bq + EPS) better = 1;
                        else if (fabs(q - bq) < EPS && pen[a] > bp + EPS) better = 1;
                    }
                }
                if (better) { bv = v; bc = P->c[i][j]; bq = q; bp = pen[a]; bi = i; bj = j; }
            }
        }
        double q = fmin(s[bi], d[bj]);
        V("Iterasi %d: dua penalti tertinggi %s dan %s, sel %s dipilih (biaya x alokasi = %s x %s = %s)\n",
          it, num(p1), num(p2 >= 0 ? p2 : p1), cell(P, bi, bj), num(P->c[bi][bj]), num(q), num(P->c[bi][bj] * q));
        X[bi][bj] += q; s[bi] -= q; d[bj] -= q;
    }
    return total_cost(P, X);
}

/* ================================================================== 7. TOCM-SC */
/* Amaliah, Fatichah, Suryani & Muswar (2020). TOCM dihitung sekali dari matriks awal.
   RS(x,y) = jumlah TOCM aktif di baris x - n_aktif * TOCM(x,y)
   CS(x,y) = jumlah TOCM aktif di kolom y - m_aktif * TOCM(x,y)
   Nilai supreme sel = max(RS, CS). Isi sel supreme tertinggi; jika baris belum habis lanjut ke
   sel supreme berikutnya di baris itu, jika kolom belum habis lanjut di kolom itu (nilai dari
   iterasi yang sama), lalu hitung ulang RS dan CS. */
static double run_tocmsc(const Problem *P, double X[MAXN][MAXN]) {
    int m = P->m, n = P->n, it = 0;
    double s[MAXN], d[MAXN];
    static double TO[MAXN][MAXN], SV[MAXN][MAXN];
    memcpy(s, P->s, sizeof s); memcpy(d, P->d, sizeof d);
    clear_x(P, X);
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) {
        double rmin = 1e300, cmin = 1e300;
        for (int l = 0; l < n; l++) if (P->c[i][l] < rmin) rmin = P->c[i][l];
        for (int l = 0; l < m; l++) if (P->c[l][j] < cmin) cmin = P->c[l][j];
        TO[i][j] = (P->c[i][j] - rmin) + (P->c[i][j] - cmin);
    }
    while (1) {
        int nr = 0, nc = 0;
        for (int i = 0; i < m; i++) if (s[i] > EPS) nr++;
        for (int j = 0; j < n; j++) if (d[j] > EPS) nc++;
        if (!nr) break;
        it++;
        double rs[MAXN], cs[MAXN];
        for (int i = 0; i < m; i++) { rs[i] = 0; if (s[i] > EPS) for (int j = 0; j < n; j++) if (d[j] > EPS) rs[i] += TO[i][j]; }
        for (int j = 0; j < n; j++) { cs[j] = 0; if (d[j] > EPS) for (int i = 0; i < m; i++) if (s[i] > EPS) cs[j] += TO[i][j]; }
        double best = -1e300; int bi = -1, bj = -1;
        for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) {
            if (s[i] <= EPS || d[j] <= EPS) continue;
            double R = rs[i] - nc * TO[i][j], C = cs[j] - nr * TO[i][j];
            SV[i][j] = R > C ? R : C;
            if (SV[i][j] > best + EPS) { best = SV[i][j]; bi = i; bj = j; }
        }
        V("Iterasi %d: supreme tertinggi %s di %s\n", it, num(best), cell(P, bi, bj));
        /* alokasi berantai di baris/kolom yang sama */
        while (bi >= 0) {
            double q = fmin(s[bi], d[bj]);
            V("  alokasi %s ke %s (biaya %s)\n", num(q), cell(P, bi, bj), num(P->c[bi][bj]));
            X[bi][bj] += q; s[bi] -= q; d[bj] -= q;
            int ni = -1, nj = -1; double nb = -1e300;
            if (s[bi] > EPS) {          /* supply belum habis: sel supreme berikutnya di baris */
                for (int j = 0; j < n; j++) if (d[j] > EPS && SV[bi][j] > nb + EPS) { nb = SV[bi][j]; ni = bi; nj = j; }
            } else if (d[bj] > EPS) {   /* demand belum habis: sel supreme berikutnya di kolom */
                for (int i = 0; i < m; i++) if (s[i] > EPS && SV[i][bj] > nb + EPS) { nb = SV[i][bj]; ni = i; nj = bj; }
            }
            bi = ni; bj = nj;
        }
    }
    return total_cost(P, X);
}

/* ================================================================== 8. TMA-VAM */
static double run_tmavam(const Problem *P, double X[MAXN][MAXN]) {
    int m = P->m, n = P->n, it = 0;
    double s[MAXN], d[MAXN];
    static double T[MAXN][MAXN];
    memcpy(s, P->s, sizeof s); memcpy(d, P->d, sizeof d);
    clear_x(P, X);
    while (1) {
        int ra[MAXN], ca[MAXN], nr = 0, ncol = 0;
        for (int i = 0; i < m; i++) if (s[i] > EPS) ra[nr++] = i;
        for (int j = 0; j < n; j++) if (d[j] > EPS) ca[ncol++] = j;
        if (!nr) break;
        it++;
        /* Langkah 1: nilai T = (c - FLC baris) + (c - FLC kolom), FLC hanya dari sel asli */
        double flcR[MAXN], flcC[MAXN];
        for (int a = 0; a < nr; a++) { int i = ra[a]; flcR[i] = 1e300;
            if (i == P->dummyRow) continue;
            for (int b = 0; b < ncol; b++) { int j = ca[b]; if (j != P->dummyCol && P->c[i][j] < flcR[i]) flcR[i] = P->c[i][j]; } }
        for (int b = 0; b < ncol; b++) { int j = ca[b]; flcC[j] = 1e300;
            if (j == P->dummyCol) continue;
            for (int a = 0; a < nr; a++) { int i = ra[a]; if (i != P->dummyRow && P->c[i][j] < flcC[j]) flcC[j] = P->c[i][j]; } }
        for (int a = 0; a < nr; a++) for (int b = 0; b < ncol; b++) {
            int i = ra[a], j = ca[b];
            if (i == P->dummyRow || j == P->dummyCol || flcR[i] > 1e299 || flcC[j] > 1e299) T[i][j] = 0;
            else T[i][j] = (P->c[i][j] - flcR[i]) + (P->c[i][j] - flcC[j]);
        }
        /* Langkah 2-3: penalti P = rata-rata T - FLC (T terkecil); pilih P terbesar,
           seri -> alokasi terbesar -> baris -> indeks terkecil */
        double bp = -1e300, bq = -1; int bt = -1, bk = -1;
        double PR[MAXN], PC[MAXN];
        for (int t = 0; t < 2; t++) {
            int L = t ? ncol : nr;
            for (int a = 0; a < L; a++) {
                int k = t ? ca[a] : ra[a]; int L2 = t ? nr : ncol;
                double sum = 0, mn = 1e300; int mi = -1, mj = -1;
                for (int b = 0; b < L2; b++) {
                    int i = t ? ra[b] : k, j = t ? k : ca[b];
                    sum += T[i][j];
                    if (T[i][j] < mn - EPS) { mn = T[i][j]; mi = i; mj = j; }
                }
                double p = L2 == 1 ? mn : sum / L2 - mn;
                double q = fmin(s[mi], d[mj]);
                if (t) PC[k] = p; else PR[k] = p;
                if (p > bp + EPS || (fabs(p - bp) < EPS && q > bq + EPS)) { bp = p; bq = q; bt = t; bk = k; }
            }
        }
        if (VERBOSE) {
            char b[8];
            printf("\nIterasi %d\nTabel nilai T (sel aktif), lalu rata-rata T, FLC, dan P tiap baris:\n        ", it);
            for (int bb = 0; bb < ncol; bb++) { cname(P, ca[bb], b); printf("%9s", b); }
            printf("%10s%8s%9s\n", "Rata2 T", "FLC", "P");
            for (int a = 0; a < nr; a++) {
                int i = ra[a]; double sum = 0, mn = 1e300; rname(P, i, b); printf("  %-5s ", b);
                for (int bb = 0; bb < ncol; bb++) { printf("%9s", num(T[i][ca[bb]])); sum += T[i][ca[bb]]; if (T[i][ca[bb]] < mn) mn = T[i][ca[bb]]; }
                printf("%10.2f%8s%9.2f\n", sum / ncol, num(mn), PR[i]);
            }
            printf("  Rata2T"); for (int bb = 0; bb < ncol; bb++) { double sm = 0; for (int a = 0; a < nr; a++) sm += T[ra[a]][ca[bb]]; printf("%9.2f", sm / nr); } printf("\n");
            printf("  FLC   "); for (int bb = 0; bb < ncol; bb++) { double mn = 1e300; for (int a = 0; a < nr; a++) if (T[ra[a]][ca[bb]] < mn) mn = T[ra[a]][ca[bb]]; printf("%9s", num(mn)); } printf("\n");
            printf("  P     "); for (int bb = 0; bb < ncol; bb++) printf("%9.2f", PC[ca[bb]]); printf("\n");
            if (bt) cname(P, bk, b); else rname(P, bk, b);
            printf("Garis terpilih: %s %s dengan P = %.2f\n", bt ? "kolom" : "baris", b, bp);
        }
        /* Langkah 4-5: skor S = T - alternatif, alternatif = T terkecil di garis yang menyilang
           (tanpa dummy); pilih S terkecil, seri -> T kecil -> alokasi besar */
        int L2 = bt ? nr : ncol; double bs = 1e300, bT = 0, bqq = 0; int ci = -1, cjj = -1;
        V("  %-10s %8s %14s %8s\n", "Sel", "T", "alternatif", "S");
        for (int b2 = 0; b2 < L2; b2++) {
            int i = bt ? ra[b2] : bk, j = bt ? bk : ca[b2];
            double alt = 1e300; int ai = -1, aj = -1;
            if (bt == 0) { for (int a = 0; a < nr; a++) { int l = ra[a]; if (l == i || l == P->dummyRow) continue; if (T[l][j] < alt - EPS) { alt = T[l][j]; ai = l; aj = j; } } }
            else { for (int bb = 0; bb < ncol; bb++) { int l = ca[bb]; if (l == j || l == P->dummyCol) continue; if (T[i][l] < alt - EPS) { alt = T[i][l]; ai = i; aj = l; } } }
            if (ai < 0) alt = T[i][j];
            double sc = T[i][j] - alt, q = fmin(s[i], d[j]);
            V("  %-10s %8s %8s %-6s %8s\n", cell(P, i, j), num(T[i][j]), num(alt), ai < 0 ? "" : cell(P, ai, aj), num(sc));
            if (sc < bs - EPS || (fabs(sc - bs) < EPS && (T[i][j] < bT - EPS || (fabs(T[i][j] - bT) < EPS && q > bqq + EPS)))) {
                bs = sc; bT = T[i][j]; bqq = q; ci = i; cjj = j; }
        }
        double q = fmin(s[ci], d[cjj]);
        V("Sel terpilih %s (S = %s). Alokasi x = min(%s, %s) = %s, biaya %s x %s = %s\n", cell(P, ci, cjj), num(bs),
          num(s[ci]), num(d[cjj]), num(q), num(q), num(P->c[ci][cjj]), num(q * P->c[ci][cjj]));
        X[ci][cjj] += q; s[ci] -= q; d[cjj] -= q;
    }
    return total_cost(P, X);
}

/* ================================================================== dispatcher */
static double run_method(int k, const Problem *P, double X[MAXN][MAXN]) {
    switch (k) {
        case 0: return run_vam(P, X);
        case 1: return run_ssm_family(P, X, F_SSM);
        case 2: return run_ssm_family(P, X, F_MSSM);
        case 3: return run_ssm_family(P, X, F_CSM);
        case 4: return run_ssm_family(P, X, F_RBSM);
        case 5: return run_thp(P, X);
        case 6: return run_tocmsc(P, X);
        default: return run_tmavam(P, X);
    }
}

static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }

/* runtime rata-rata (mikrodetik) dari beberapa kali pengulangan */
static double time_method(int k, const Problem *P, double *z) {
    static double X[MAXN][MAXN];
    int rep = 0; double t0 = now_us(), t1;
    do { *z = run_method(k, P, X); rep++; t1 = now_us(); } while (t1 - t0 < 20000 && rep < 100000);
    return (t1 - t0) / rep;
}

static void print_alloc(const Problem *P, double X[MAXN][MAXN]) {
    printf("Hasil alokasi:");
    double z = 0; int cnt = 0;
    for (int i = 0; i < P->m; i++) for (int j = 0; j < P->n; j++) if (X[i][j] > EPS) {
        printf(" %s=%s", cell(P, i, j), num(X[i][j])); z += X[i][j] * P->c[i][j]; cnt++; }
    printf("\nZ = ");
    int first = 1;
    for (int i = 0; i < P->m; i++) for (int j = 0; j < P->n; j++) if (X[i][j] > EPS) {
        printf("%s%sx%s", first ? "" : " + ", num(P->c[i][j]), num(X[i][j])); first = 0; }
    printf(" = %s   (sel terisi = %d, m + n - 1 = %d)\n", num(z), cnt, P->m + P->n - 1);
}

/* ------------------------------------------------------------------ mode dataset */
static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int run_dataset(const char *dir, const char *optfile) {
    char *names[512]; int nf = 0;
    DIR *dp = opendir(dir);
    if (!dp) { printf("Folder %s tidak ditemukan\n", dir); return 1; }
    struct dirent *e;
    while ((e = readdir(dp))) { int L = strlen(e->d_name); if (L > 4 && !strcmp(e->d_name + L - 4, ".txt")) names[nf++] = strdup(e->d_name); }
    closedir(dp);
    qsort(names, nf, sizeof(char *), cmpstr);
    /* nilai optimal: baris "nama nilai" */
    char on[512][64]; double ov[512]; int no = 0;
    FILE *fo = fopen(optfile, "r");
    if (fo) { while (no < 512 && fscanf(fo, "%63s %lf", on[no], &ov[no]) == 2) no++; fclose(fo); }
    FILE *csv = fopen("hasil.csv", "w"), *rt = fopen("runtime.csv", "w");
    fprintf(csv, "Soal,Ukuran,Optimal"); fprintf(rt, "Soal,Ukuran");
    for (int k = 0; k < NMETHOD; k++) { fprintf(csv, ",%s", MLONG[k]); fprintf(rt, ",%s (us)", MLONG[k]); }
    fprintf(csv, "\n"); fprintf(rt, "\n");

    int nopt[NMETHOD] = {0}, nb = 0, nu = 0, noptB[NMETHOD] = {0}, noptU[NMETHOD] = {0};
    double dev[NMETHOD] = {0}, tsum[NMETHOD] = {0};
    int win[NMETHOD] = {0}, lose[NMETHOD] = {0}, nbad = 0;
    printf("%-10s %-6s %10s", "Soal", "Ukuran", "Optimal");
    for (int k = 0; k < NMETHOD; k++) printf(" %10s", MNAME[k]);
    printf("\n");
    static Problem P; static double X[MAXN][MAXN];
    for (int f = 0; f < nf; f++) {
        char path[1024]; sprintf(path, "%s/%s", dir, names[f]);
        if (!read_problem(path, &P)) continue;
        char base[64]; strncpy(base, names[f], 63); base[strlen(base) - 4] = 0;
        double opt = P.opt; for (int a = 0; a < no; a++) if (!strcmp(on[a], base)) opt = ov[a];
        int bal = P.dummyRow < 0 && P.dummyCol < 0; if (bal) nb++; else nu++;
        char sz[16]; sprintf(sz, "%dx%d", P.m0, P.n0);
        printf("%-10s %-6s %10s", base, sz, opt >= 0 ? num(opt) : "?");
        fprintf(csv, "%s,%s,%s", base, sz, opt >= 0 ? num(opt) : ""); fprintf(rt, "%s,%s", base, sz);
        double z[NMETHOD];
        for (int k = 0; k < NMETHOD; k++) {
            double us = time_method(k, &P, &z[k]);
            run_method(k, &P, X);
            if (!feasible(&P, X)) { nbad++; printf("\n[PERINGATAN] %s pada %s tidak layak\n", MLONG[k], base); }
            tsum[k] += us;
            int isopt = opt >= 0 && fabs(z[k] - opt) < 1e-6;
            char t[24]; sprintf(t, "%s%s", num(z[k]), isopt ? "*" : " ");
            printf(" %10s", t);
            fprintf(csv, ",%s", num(z[k])); fprintf(rt, ",%.3f", us);
            if (isopt) { nopt[k]++; if (bal) noptB[k]++; else noptU[k]++; }
            if (opt > 0) dev[k] += (z[k] - opt) / opt * 100;
        }
        for (int k = 0; k < NMETHOD - 1; k++) { if (z[NMETHOD - 1] < z[k] - 1e-6) win[k]++; else if (z[NMETHOD - 1] > z[k] + 1e-6) lose[k]++; }
        printf("\n"); fprintf(csv, "\n"); fprintf(rt, "\n");
    }
    fclose(csv); fclose(rt);
    printf("\n* = sama dengan nilai optimal\n\nRINGKASAN %d soal (%d seimbang, %d tak seimbang)\n", nf, nb, nu);
    printf("%-10s %9s %9s %10s %10s %12s %14s\n", "Metode", "Optimal", "Akurasi", "Deviasi", "Seimbang", "TakSeimbang", "Runtime(us)");
    for (int k = 0; k < NMETHOD; k++)
        printf("%-10s %6d/%d %8.1f%% %9.2f%% %9.1f%% %11.1f%% %14.2f\n", MLONG[k], nopt[k], nf, 100.0 * nopt[k] / nf,
               dev[k] / nf, nb ? 100.0 * noptB[k] / nb : 0, nu ? 100.0 * noptU[k] / nu : 0, tsum[k] / nf);
    printf("Cek kelayakan: %s\n", nbad ? "ADA solusi yang tidak layak" : "semua solusi layak (supply dan demand terpenuhi)");
    printf("\nTMA-VAM dibanding metode lain (jumlah soal): menang / seri / kalah\n");
    for (int k = 0; k < NMETHOD - 1; k++) printf("  vs %-10s %3d / %3d / %3d\n", MLONG[k], win[k], nf - win[k] - lose[k], lose[k]);
    printf("\nHasil per soal ditulis ke hasil.csv, runtime per soal ke runtime.csv\n");
    return 0;
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "-d")) return run_dataset(argv[2], argc >= 4 ? argv[3] : "");
    if (argc < 2) {
        printf("Pemakaian:\n  %s soal.txt\n  %s soal.txt -v METODE   (VAM SSM MSSM CSM RBSM THP TOCMSC TMAVAM ALL)\n  %s soal.txt -z METODE\n  %s -d folder_dataset [optimal.txt]\n", argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }
    static Problem P; static double X[MAXN][MAXN];
    if (!read_problem(argv[1], &P)) return 1;
    if (argc >= 4 && !strcmp(argv[2], "-z")) {
        for (int k = 0; k < NMETHOD; k++) if (!strcmp(argv[3], MNAME[k])) { printf("Z = %s\n", num(run_method(k, &P, X))); return 0; }
        printf("Metode tidak dikenal\n"); return 1;
    }
    printf("Soal %s (%d x %d)%s\n", argv[1], P.m0, P.n0,
           P.dummyCol >= 0 ? ", supply > demand: ditambah kolom dummy Dd" : (P.dummyRow >= 0 ? ", demand > supply: ditambah baris dummy Sd" : ", seimbang"));
    print_table(&P, X, NULL, NULL, P.s, P.d, 0);
    if (argc >= 4 && !strcmp(argv[2], "-v")) {
        for (int k = 0; k < NMETHOD; k++) {
            if (strcmp(argv[3], "ALL") && strcmp(argv[3], MNAME[k])) continue;
            printf("\n================ %s ================\n", MLONG[k]);
            VERBOSE = 1; double z = run_method(k, &P, X); VERBOSE = 0;
            printf("\n"); print_alloc(&P, X); (void)z;
        }
        return 0;
    }
    printf("\n%-10s %12s %16s\n", "Metode", "Z", "Runtime (us)");
    for (int k = 0; k < NMETHOD; k++) {
        double z, us = time_method(k, &P, &z);
        printf("%-10s %12s %16.3f\n", MLONG[k], num(z), us);
    }
    return 0;
}
