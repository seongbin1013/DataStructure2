#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int *tmp;

static long long mergeCount(int a[], int lo, int hi)
{
    if (hi - lo < 1) return 0;
    int mid = (lo + hi) / 2;
    long long c = mergeCount(a, lo, mid) + mergeCount(a, mid + 1, hi);
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { tmp[k++] = a[j++]; c += mid - i + 1; }
    }
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= hi)  tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, sizeof(int) * (hi - lo + 1));
    return c;
}

static long long inversions(const int a[], int n)
{
    int *b = malloc(sizeof(int) * n);
    memcpy(b, a, sizeof(int) * n);
    long long c = mergeCount(b, 0, n - 1);
    free(b);
    return c;
}

static long long gapPass(int a[], int n, int gap)
{
    long long cmp = 0;
    for (int i = gap; i < n; i++) {
        int key = a[i], j = i - gap;
        while (j >= 0) {
            cmp++;
            if (a[j] > key) { a[j + gap] = a[j]; j -= gap; }
            else break;
        }
        a[j + gap] = key;
    }
    return cmp;
}

static int origGaps(int n, int g[]) { int k = 0; for (int h = n / 2; h > 0; h /= 2) g[k++] = h; return k; }
static int knuthGaps(int n, int g[])
{
    int h = 1, k = 0;
    while (3 * h + 1 < n) h = 3 * h + 1;
    for (; h >= 1; h = (h - 1) / 3) g[k++] = h;
    return k;
}

static void detail(const char *name, const int src[], int n, int g[], int k)
{
    int *a = malloc(sizeof(int) * n);
    memcpy(a, src, sizeof(int) * n);
    long long total = 0;
    printf("\n[%s] n=%d  start inversions=%lld\n", name, n, inversions(a, n));
    printf("%6s %10s %10s %10s %14s\n", "gap", "cmp", "min(n-h)", "extra", "inv_after");
    for (int s = 0; s < k; s++) {
        long long c = gapPass(a, n, g[s]);
        total += c;
        printf("%6d %10lld %10d %10lld %14lld\n", g[s], c, n - g[s], c - (n - g[s]), inversions(a, n));
    }
    printf("total=%lld\n", total);
    free(a);
}

static long long run(const int src[], int n, int g[], int k)
{
    int *a = malloc(sizeof(int) * n);
    memcpy(a, src, sizeof(int) * n);
    long long t = 0;
    for (int s = 0; s < k; s++) t += gapPass(a, n, g[s]);
    free(a);
    return t;
}

int main(void)
{
    const int sizes[] = {1000, 2000, 4000, 8000, 16000, 20000};
    const int SEEDS = 30;
    int go[64], gk[64];

    /* (1) 단계별 상세: n = 20000, seed = 1 */
    {
        int n = 20000;
        int *src = malloc(sizeof(int) * n);
        tmp = malloc(sizeof(int) * n);
        srand(1);
        for (int i = 0; i < n; i++) src[i] = rand() % 100000;
        int ko = origGaps(n, go), kk = knuthGaps(n, gk);
        detail("Original", src, n, go, ko);
        detail("Knuth", src, n, gk, kk);
        free(src); free(tmp);
    }

    /* (2) 시드 30개 반복: Knuth < Original 횟수와 평균 */
    printf("\n[30 seeds] n, knuth_wins, avgO, avgK, avg_saving%%\n");
    for (int s = 0; s < 6; s++) {
        int n = sizes[s];
        int *src = malloc(sizeof(int) * n);
        int ko = origGaps(n, go), kk = knuthGaps(n, gk);
        int wins = 0; double so = 0, sk = 0;
        for (int seed = 1; seed <= SEEDS; seed++) {
            srand(seed);
            for (int i = 0; i < n; i++) src[i] = rand() % 100000;
            long long o = run(src, n, go, ko), k = run(src, n, gk, kk);
            if (k < o) wins++;
            so += o; sk += k;
        }
        printf("%6d %3d/%d %10.0f %10.0f %6.1f\n", n, wins, SEEDS, so / SEEDS, sk / SEEDS, (1 - sk / so) * 100);
        free(src);
    }
    return 0;
}