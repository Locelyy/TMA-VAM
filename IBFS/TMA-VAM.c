/*
 * TMA-VAM.c  -  Total-opportunity Mean-penalty Alternative-cell VAM
 *               Metode usulan Kelompok 6, Riset Operasi, Teknik Informatika ITS
 *
 * Langkah (lihat flowchart_tmavam.png):
 *   0. Seimbangkan soal: tambah baris/kolom dummy berbiaya 0 bila perlu.
 *   1. Nilai T tiap sel aktif: T = (c - FLC baris) + (c - FLC kolom).
 *      FLC hanya diambil dari sel asli; T sel dummy = 0. T dihitung ulang tiap iterasi.
 *   2. Penalti P tiap garis aktif: P = rata-rata T - FLC (T terkecil di garis).
 *      Garis yang tinggal satu sel: P = T sel itu.
 *   3. Pilih garis dengan P terbesar. Seri: alokasi terbesar, lalu baris, lalu indeks terkecil.
 *   4. Tiap sel di garis terpilih: alternatif = T terkecil di garis yang menyilang
 *      (dummy tidak dipakai), skor S = T - alternatif.
 *   5. Pilih sel dengan S terkecil. Seri: T terkecil, lalu alokasi terbesar.
 *   6. Alokasi x = min(sisa supply, sisa demand), coret garis yang habis.
 *   7. Ulangi dari langkah 1 sampai semua habis, lalu Z = jumlah c x x.
 *
 * Kompilasi :  gcc -O2 -o tmavam TMA-VAM.c -lm
 *
 * Cara pakai:
 *   ./tmavam                         program menanyakan nama file soal
 *   ./tmavam soal.txt                langkah demi langkah + hasil akhir
 *   ./tmavam soal.txt -q             hanya hasil akhir dan runtime
 *   ./tmavam -d dataset69            semua file .txt di folder: Z, optimal, runtime
 *   ./tmavam -d dataset69 optimal69.txt   nilai optimal dibaca dari file (nama nilai)
 *
 * Format file soal (sama dengan dataset69 / dataset96):
 *   m n
 *   c11 ... c1n
 *   ...
 *   s1 ... sm
 *   d1 ... dn
 *   [nilai optimal, opsional]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>

#define MAXN 128
#define EPS 1e-9

typedef struct {
    int m, n;                 /* ukuran setelah diseimbangkan            */
    int m0, n0;               /* ukuran asli                             */
    double c[MAXN][MAXN];
    double s[MAXN], d[MAXN];
    int dummyRow, dummyCol;   /* indeks baris/kolom dummy, -1 jika tidak ada */
    double opt;               /* nilai optimal bila tertulis di akhir file, -1 jika tidak ada */
} Problem;

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


/* ================================================================== METODE TMA-VAM */
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


static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
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

/* ------------------------------------------------------------------ runtime */
static double time_tmavam(const Problem *P, double *z) {
    static double X[MAXN][MAXN];
    int rep = 0; double t0 = now_us(), t1;
    do { *z = run_tmavam(P, X); rep++; t1 = now_us(); } while (t1 - t0 < 20000 && rep < 100000);
    return (t1 - t0) / rep;     /* mikrodetik per penyelesaian */
}

/* ------------------------------------------------------------------ satu soal */
static int run_single(const char *path, int quiet) {
    static Problem P; static double X[MAXN][MAXN];
    if (!read_problem(path, &P)) return 1;
    printf("Soal %s (%d x %d)%s\n", path, P.m0, P.n0,
           P.dummyCol >= 0 ? ", supply > demand: ditambah kolom dummy Dd" : (P.dummyRow >= 0 ? ", demand > supply: ditambah baris dummy Sd" : ", seimbang"));
    print_table(&P, X, NULL, NULL, P.s, P.d, 0);
    VERBOSE = !quiet;
    double z = run_tmavam(&P, X);
    VERBOSE = 0;
    printf("\n"); print_alloc(&P, X);
    printf("Cek kelayakan: %s\n", feasible(&P, X) ? "layak (semua supply dan demand terpenuhi)" : "TIDAK LAYAK");
    if (P.opt >= 0) printf("Nilai optimal di file: %s -> %s\n", num(P.opt), fabs(z - P.opt) < 1e-6 ? "OPTIMAL" : "belum optimal");
    double zz, us = time_tmavam(&P, &zz);
    printf("Runtime: %.3f mikrodetik per penyelesaian\n", us);
    return 0;
}

/* ------------------------------------------------------------------ satu folder */
static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int run_folder(const char *dir, const char *optfile) {
    char *names[2048]; int nf = 0;
    DIR *dp = opendir(dir);
    if (!dp) { printf("Folder %s tidak ditemukan\n", dir); return 1; }
    struct dirent *e;
    while ((e = readdir(dp)) && nf < 2048) { int L = strlen(e->d_name); if (L > 4 && !strcmp(e->d_name + L - 4, ".txt")) names[nf++] = strdup(e->d_name); }
    closedir(dp);
    qsort(names, nf, sizeof(char *), cmpstr);
    static char on[2048][64]; static double ov[2048]; int no = 0;
    FILE *fo = optfile ? fopen(optfile, "r") : NULL;
    if (fo) { while (no < 2048 && fscanf(fo, "%63s %lf", on[no], &ov[no]) == 2) no++; fclose(fo); }
    static Problem P; static double X[MAXN][MAXN];
    int nopt = 0, nknown = 0, nbad = 0; double dev = 0, tsum = 0;
    printf("%-12s %-7s %12s %12s %8s %14s\n", "Soal", "Ukuran", "Optimal", "TMA-VAM", "Gap(%)", "Runtime(us)");
    for (int f = 0; f < nf; f++) {
        char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, names[f]);
        if (!read_problem(path, &P)) continue;
        char base[64]; strncpy(base, names[f], 63); base[63] = 0; base[strlen(base) - 4] = 0;
        double opt = P.opt; for (int a = 0; a < no; a++) if (!strcmp(on[a], base)) opt = ov[a];
        double z, us = time_tmavam(&P, &z); tsum += us;
        run_tmavam(&P, X); if (!feasible(&P, X)) nbad++;
        char sz[16]; snprintf(sz, sizeof sz, "%dx%d", P.m0, P.n0);
        if (opt >= 0) {
            double g = (z - opt) / opt * 100; nknown++; dev += g;
            if (fabs(z - opt) < 1e-6) nopt++;
            printf("%-12s %-7s %12s %11s%s %8.2f %14.3f\n", base, sz, num(opt), num(z), fabs(z - opt) < 1e-6 ? "*" : " ", g, us);
        } else printf("%-12s %-7s %12s %12s %8s %14.3f\n", base, sz, "?", num(z), "-", us);
    }
    printf("\n* = sama dengan nilai optimal\n");
    printf("Jumlah soal         : %d\n", nf);
    if (nknown) {
        printf("Soal optimal        : %d dari %d (akurasi %.1f%%)\n", nopt, nknown, 100.0 * nopt / nknown);
        printf("Gap rata-rata       : %.2f%%\n", dev / nknown);
    }
    printf("Runtime rata-rata   : %.3f mikrodetik per soal\n", nf ? tsum / nf : 0);
    printf("Cek kelayakan       : %s\n", nbad ? "ADA solusi tidak layak" : "semua solusi layak");
    return 0;
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "-d")) return run_folder(argv[2], argc >= 4 ? argv[3] : NULL);
    if (argc >= 2) return run_single(argv[1], argc >= 3 && !strcmp(argv[2], "-q"));
    char name[512];
    printf("TMA-VAM - masukkan nama file soal: ");
    if (scanf("%511s", name) != 1) return 1;
    return run_single(name, 0);
}
