/*
 * HRFVAM.c : Hybrid Rollout Fractional-Penalty Vogel's Approximation Method
 *
 * Kompilasi : gcc -O2 -o hrfvam HRFVAM.c -lm
 * Jalankan  : ./hrfvam soal_4x5.txt        (tampilkan semua iterasi)
 *             ./hrfvam soal_4x5.txt -q     (hanya hasil akhir)
 *
 * Format input (teks):
 *   m n
 *   c11 ... c1n          (m baris matriks biaya)
 *   ...
 *   s1 ... sm            (supply)
 *   d1 ... dn            (demand)
 *
 * ALUR PROGRAM = FLOWCHART HR-FVAM DI SLIDE
 *   Langkah 1  Mulai
 *   Langkah 2  Seimbangkan soal, tandai garis dummy        -> balance()
 *   Langkah 3  Hitung m1 dan m2 tiap baris/kolom aktif    -> line_penalty()
 *   Langkah 4  P = (m2 - m1) x (m2/m1)^0,25                -> line_penalty()
 *   Langkah 5  Pilih garis P terbesar (seri: baris dulu)   -> pick_line()
 *   Langkah 6  A = sel termurah, B = sel c - alt terkecil  -> cheapest(), cand_b()
 *   Langkah 7  A = B ?  ya: pakai A / tidak: ke langkah 8  -> main()
 *   Langkah 8  Rollout: simulasikan A dan B, ambil Z terkecil -> main(), finish()
 *   Langkah 9  Alokasi x = min(supply, demand), coret yang habis -> apply()
 *   Langkah 10 Masih ada sisa? ya: ulangi dari langkah 3   -> while di main()
 *   Langkah 11 Hitung Z = sum(c x x), selesai              -> akhir main()
 *
 * Aturan tambahan (ditulis tegas di slide):
 *   - m1 = 0 -> P = 0
 *   - tinggal satu sel aktif di garis -> P = biaya sel itu
 *   - penalti seri -> baris didahulukan
 *   - garis dummy tidak dipakai saat menghitung alt
 *   - bila sebuah sel tidak punya pengganti di garis yang menyilang,
 *     alt dianggap 0 (asumsi program ini; tidak tertulis di slide)
 *   - seri skor c - alt pada kandidat B dipecah dengan biaya terkecil
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAXN  205          /* maks 200 + 1 garis dummy + cadangan */
#define K_EXP 0.25         /* pangkat k pada (m2/m1)^k            */
#define INF   (1 << 30)

typedef struct {           /* data soal (setelah diseimbangkan)   */
    int c[MAXN][MAXN];     /* biaya per unit                      */
    int dummyR[MAXN], dummyC[MAXN];
    int m, n;
} Problem;

typedef struct {           /* keadaan penyelesaian (bisa disalin untuk rollout) */
    int sup[MAXN], dem[MAXN];
    int ra[MAXN], ca[MAXN];    /* 1 = baris/kolom masih aktif     */
    int x[MAXN][MAXN];         /* alokasi                         */
    long z;                    /* total biaya sejauh ini          */
} State;

static Problem P;

static int rows_left(const State *s) { int k = 0; for (int i = 0; i < P.m; i++) k += s->ra[i]; return k; }
static int cols_left(const State *s) { int k = 0; for (int j = 0; j < P.n; j++) k += s->ca[j]; return k; }

/* nama sel untuk dicetak: X13 bila indeks satu digit, X10,12 bila lebih */
static const char *cell(int i, int j) {
    static char buf[4][32];
    static int k = 0;
    k = (k + 1) & 3;
    if (P.m < 10 && P.n < 10) snprintf(buf[k], 32, "X%d%d", i + 1, j + 1);
    else snprintf(buf[k], 32, "X%d,%d", i + 1, j + 1);
    return buf[k];
}

/* ---------------------------------------------------------------
 * LANGKAH 3 + 4: hitung m1, m2 satu garis, lalu penalti hibrid.
 * isRow = 1 -> baris idx, isRow = 0 -> kolom idx.
 * Mengembalikan -1 bila garis tidak punya sel aktif.
 * --------------------------------------------------------------- */
static double line_penalty(const State *s, int isRow, int idx) {
    int m1 = INF, m2 = INF, k = 0;
    int len = isRow ? P.n : P.m;
    for (int t = 0; t < len; t++) {
        int i = isRow ? idx : t, j = isRow ? t : idx;
        if (isRow ? !s->ca[j] : !s->ra[i]) continue;   /* sel sudah dicoret */
        int v = P.c[i][j];
        k++;
        if (v < m1) { m2 = m1; m1 = v; }               /* m1 = termurah     */
        else if (v < m2) m2 = v;                       /* m2 = termurah ke-2 */
    }
    if (k == 0) return -1;
    if (k == 1) return (double)m1;                     /* tinggal 1 sel: P = biaya sel */
    if (m1 == 0) return 0.0;                           /* m1 = 0: P = 0                */
    return (m2 - m1) * pow((double)m2 / m1, K_EXP);    /* P = (m2-m1) x (m2/m1)^0,25   */
}

/* ---------------------------------------------------------------
 * LANGKAH 5: pilih garis dengan P terbesar.
 * Baris dicek lebih dulu dan hanya diganti bila P benar-benar lebih
 * besar (">"), sehingga saat seri baris didahulukan.
 * --------------------------------------------------------------- */
static int pick_line(const State *s, int *isRow, int *idx, double *pv) {
    double best = -1;
    int bi = -1, br = 0;
    for (int i = 0; i < P.m; i++) {
        if (!s->ra[i]) continue;
        double p = line_penalty(s, 1, i);
        if (p > best) { best = p; bi = i; br = 1; }
    }
    for (int j = 0; j < P.n; j++) {
        if (!s->ca[j]) continue;
        double p = line_penalty(s, 0, j);
        if (p > best) { best = p; bi = j; br = 0; }
    }
    if (bi < 0) return 0;
    *isRow = br; *idx = bi; *pv = best;
    return 1;
}

/* ---------------------------------------------------------------
 * LANGKAH 6a: kandidat A = sel termurah di garis terpilih.
 * --------------------------------------------------------------- */
static void cheapest(const State *s, int isRow, int idx, int *r, int *c) {
    int best = INF, len = isRow ? P.n : P.m;
    for (int t = 0; t < len; t++) {
        int i = isRow ? idx : t, j = isRow ? t : idx;
        if (isRow ? !s->ca[j] : !s->ra[i]) continue;
        if (P.c[i][j] < best) { best = P.c[i][j]; *r = i; *c = j; }
    }
}

/* alt sel (i,j) = biaya termurah di garis yang menyilang (sel lain yang
 * masih aktif, garis dummy tidak dihitung). -1 bila tidak ada. */
static int alt_cost(const State *s, int isRow, int i, int j) {
    int best = INF;
    if (isRow) {            /* garis menyilang = kolom j */
        for (int i2 = 0; i2 < P.m; i2++)
            if (i2 != i && s->ra[i2] && !P.dummyR[i2] && P.c[i2][j] < best) best = P.c[i2][j];
    } else {                /* garis menyilang = baris i */
        for (int j2 = 0; j2 < P.n; j2++)
            if (j2 != j && s->ca[j2] && !P.dummyC[j2] && P.c[i][j2] < best) best = P.c[i][j2];
    }
    return best == INF ? -1 : best;
}

/* ---------------------------------------------------------------
 * LANGKAH 6b: kandidat B = sel dengan skor (c - alt) terkecil.
 * Skor negatif = sel jauh lebih murah dari penggantinya.
 * Seri skor dipecah dengan biaya terkecil.
 * --------------------------------------------------------------- */
static void cand_b(const State *s, int isRow, int idx, int *r, int *c, int print) {
    int bs = INF, bc = INF, len = isRow ? P.n : P.m;
    for (int t = 0; t < len; t++) {
        int i = isRow ? idx : t, j = isRow ? t : idx;
        if (isRow ? !s->ca[j] : !s->ra[i]) continue;
        int a = alt_cost(s, isRow, i, j);
        int sc = P.c[i][j] - (a < 0 ? 0 : a);
        if (print) printf("    skor c - alt %s: %d - %d = %d\n", cell(i, j), P.c[i][j], a < 0 ? 0 : a, sc);
        if (sc < bs || (sc == bs && P.c[i][j] < bc)) { bs = sc; bc = P.c[i][j]; *r = i; *c = j; }
    }
}

/* ---------------------------------------------------------------
 * LANGKAH 9: alokasi x = min(supply, demand), kurangi supply dan
 * demand, coret baris/kolom yang habis. Mengembalikan x.
 * --------------------------------------------------------------- */
static int apply(State *s, int r, int c) {
    int x = s->sup[r] < s->dem[c] ? s->sup[r] : s->dem[c];
    s->x[r][c] += x;
    s->sup[r] -= x;
    s->dem[c] -= x;
    s->z += (long)x * P.c[r][c];
    if (s->sup[r] == 0) s->ra[r] = 0;
    if (s->dem[c] == 0) s->ca[c] = 0;
    return x;
}

/* ---------------------------------------------------------------
 * LANGKAH 8 (bagian simulasi): selesaikan sisa soal sampai habis
 * dan kembalikan Z akhir. State diterima by value, jadi simulasi
 * tidak mengubah soal yang sebenarnya.
 *   rule 0 = aturan "termurah" (isi sel termurah di garis terpilih)
 *   rule 1 = aturan "sel-alt"  (isi kandidat B di garis terpilih)
 * Garis dipilih dengan penalti hibrid yang sama (langkah 3 sampai 5).
 * --------------------------------------------------------------- */
static long finish(State s, int rule) {
    while (rows_left(&s) && cols_left(&s)) {
        int isRow, idx, r = 0, c = 0;
        double pv;
        if (!pick_line(&s, &isRow, &idx, &pv)) break;
        if (rule == 0) cheapest(&s, isRow, idx, &r, &c);
        else           cand_b(&s, isRow, idx, &r, &c, 0);
        apply(&s, r, c);
    }
    return s.z;
}

static long min2(long a, long b) { return a < b ? a : b; }

/* ---------------------------------------------------------------
 * LANGKAH 2: seimbangkan soal. Supply > demand -> tambah kolom
 * dummy; demand > supply -> tambah baris dummy. Biaya dummy = 0 dan
 * garis ditandai dummyR/dummyC (dipakai saat menghitung alt).
 * --------------------------------------------------------------- */
static void balance(long *ts, long *td, int *sup, int *dem) {
    *ts = *td = 0;
    for (int i = 0; i < P.m; i++) *ts += sup[i];
    for (int j = 0; j < P.n; j++) *td += dem[j];
    if (*ts > *td) {
        for (int i = 0; i < P.m; i++) P.c[i][P.n] = 0;
        dem[P.n] = (int)(*ts - *td);
        P.dummyC[P.n] = 1;
        P.n++;
    } else if (*td > *ts) {
        for (int j = 0; j < P.n; j++) P.c[P.m][j] = 0;
        sup[P.m] = (int)(*td - *ts);
        P.dummyR[P.m] = 1;
        P.m++;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Pemakaian: %s berkas_soal.txt [-q]\n", argv[0]); return 1; }
    int verbose = !(argc > 2 && strcmp(argv[2], "-q") == 0);

    /* ---- LANGKAH 1: Mulai, baca soal ---- */
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("berkas"); return 1; }
    int m, n;
    if (fscanf(f, "%d %d", &m, &n) != 2 || m < 1 || n < 1 || m > 200 || n > 200) {
        fprintf(stderr, "Ukuran soal tidak valid (maks 200x200)\n"); return 1;
    }
    P.m = m; P.n = n;
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            if (fscanf(f, "%d", &P.c[i][j]) != 1) { fprintf(stderr, "Matriks biaya kurang\n"); return 1; }
    static int sup[MAXN], dem[MAXN];
    for (int i = 0; i < m; i++) if (fscanf(f, "%d", &sup[i]) != 1) { fprintf(stderr, "Supply kurang\n"); return 1; }
    for (int j = 0; j < n; j++) if (fscanf(f, "%d", &dem[j]) != 1) { fprintf(stderr, "Demand kurang\n"); return 1; }
    fclose(f);

    /* ---- LANGKAH 2: seimbangkan soal, tandai garis dummy ---- */
    long ts, td;
    balance(&ts, &td, sup, dem);
    if (verbose && ts != td)
        printf("Langkah 2: soal tak seimbang (supply %ld, demand %ld), ditambah garis dummy biaya 0\n", ts, td);

    static State s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < P.m; i++) { s.sup[i] = sup[i]; s.ra[i] = 1; }
    for (int j = 0; j < P.n; j++) { s.dem[j] = dem[j]; s.ca[j] = 1; }

    int it = 0, rollouts = 0;

    /* ---- LANGKAH 10: ulangi selama masih ada sisa ---- */
    while (rows_left(&s) && cols_left(&s)) {
        int isRow, idx, ar = 0, ac = 0, br = 0, bc = 0;
        double pv;

        /* LANGKAH 3, 4, 5: m1/m2, penalti hibrid, pilih garis P terbesar */
        if (!pick_line(&s, &isRow, &idx, &pv)) break;
        it++;
        if (verbose) {
            printf("Iterasi %d\n", it);
            printf("  Langkah 3-5 | garis terpilih : %s%d (P = %.2f)\n", isRow ? "S" : "D", idx + 1, pv);
        }

        /* LANGKAH 6: A = sel termurah, B = sel c - alt terkecil */
        cheapest(&s, isRow, idx, &ar, &ac);
        cand_b(&s, isRow, idx, &br, &bc, verbose);
        if (verbose) {
            printf("  Langkah 6   | kandidat A (termurah): S%d-D%d (biaya %d)\n", ar + 1, ac + 1, P.c[ar][ac]);
            printf("              | kandidat B (sel-alt) : S%d-D%d (biaya %d)\n", br + 1, bc + 1, P.c[br][bc]);
        }

        int cr = ar, cc = ac;                      /* sel yang akan dialokasikan */

        /* LANGKAH 7: A = B ? */
        if (ar == br && ac == bc) {
            if (verbose) printf("  Langkah 7   | A = B, tanpa rollout\n");
        } else {
            /* LANGKAH 8: rollout. Alokasikan tiap kandidat sementara, lalu
             * selesaikan sisa soal dengan aturan termurah dan aturan sel-alt.
             * Nilai kandidat = Z terkecil dari dua aturan itu. */
            rollouts++;
            State sa = s, sb = s;                  /* salinan; soal asli tidak berubah */
            apply(&sa, ar, ac);
            apply(&sb, br, bc);
            long a0 = finish(sa, 0), a1 = finish(sa, 1);
            long b0 = finish(sb, 0), b1 = finish(sb, 1);
            long va = min2(a0, a1), vb = min2(b0, b1);
            if (verbose) {
                printf("  Langkah 7   | A != B, jalankan rollout\n");
                printf("  Langkah 8   | rollout A: Z(termurah) = %ld, Z(alt) = %ld -> %ld\n", a0, a1, va);
                printf("              | rollout B: Z(termurah) = %ld, Z(alt) = %ld -> %ld\n", b0, b1, vb);
                printf("              | dipilih kandidat %s\n", vb < va ? "B" : "A");
            }
            if (vb < va) { cr = br; cc = bc; }     /* seri: tetap A */
        }

        /* LANGKAH 9: alokasi x = min(supply, demand), coret yang habis */
        int x = apply(&s, cr, cc);
        if (verbose) printf("  Langkah 9   | alokasi %d unit ke %s\n", x, cell(cr, cc));
    }

    /* ---- LANGKAH 11: hitung Z = sum(c x x), selesai ---- */
    printf("\nAlokasi akhir:\n");
    for (int i = 0; i < P.m; i++)
        for (int j = 0; j < P.n; j++)
            if (s.x[i][j])
                printf("  %s = %d  (biaya %d)%s\n", cell(i, j), s.x[i][j], P.c[i][j],
                       (P.dummyR[i] || P.dummyC[j]) ? "  [dummy]" : "");
    printf("Jumlah rollout : %d\n", rollouts);
    printf("TCC = %ld\n", s.z);
    return 0;
}

