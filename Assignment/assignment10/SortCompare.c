#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N                   10000      /* 데이터 개수 */
#define TRIALS              100        /* 무작위 실험 반복 횟수 */
#define MAX_KEY             1000000    /* 키 범위 : 0 ~ MAX_KEY */
#define DEFAULT_SEED        20261027u  /* 기본 난수 시드 */
#define PARTIAL_SWAP_RATE   0.05       /* 부분 정렬 : N의 5%(500회)만큼 무작위 위치 쌍 교환 */
#define BLOCK_SIZE          20         /* 추가 분석 : 블록 내부 섞기 크기 */
#define TAIL_RATE           0.10       /* 추가 분석 : 뒤쪽 10%만 섞기 */
#define STAB_KEY_RANGE      100        /* 안정성 시험 데이터 Key 1 범위 : 0 ~ 99 */

/* ------------------------------------------------------------------ */
/* 자료형                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    int key1;   /* 정렬 기준 키 */
    int key2;   /* 보조 키 (정렬 기준 아님) */
    int order;  /* 원래 입력 순서 (안정성 검증용, 정렬 기준 아님) */
} Data;

typedef struct {
    long long comparisons;  /* Key 1 비교 횟수 */
    long long moves;        /* 데이터 이동(복사/대입) 횟수 */
    long long passes;       /* 버블 정렬 순회(Pass) 횟수 */
} Stats;

typedef void (*SortFunc)(Data a[], int n, Stats *st);

/* ------------------------------------------------------------------ */
/* 난수 (xorshift32) : 운영체제마다 rand() 범위가 달라서 직접 구현     */
/* ------------------------------------------------------------------ */

static unsigned int rng_state = DEFAULT_SEED;

static unsigned int next_random(void)
{
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* 0 ~ max 사이의 정수 */
static int random_int(int max)
{
    return (int)(next_random() % (unsigned int)(max + 1));
}

/* ------------------------------------------------------------------ */
/* 정렬 알고리즘 (Key 1 오름차순)                                      */
/*   비교 : Key 1끼리 비교할 때마다 comparisons++                      */
/*   이동 : Data 하나를 통째로 복사/대입할 때마다 moves++              */
/* ------------------------------------------------------------------ */

/* 두 데이터 교환 : 이동 3회 */
static void swap_data(Data *x, Data *y, Stats *st)
{
    Data temp = *x;  st->moves++;
    *x = *y;         st->moves++;
    *y = temp;       st->moves++;
}

/* [1] 선택 정렬 */
void selection_sort(Data a[], int n, Stats *st)
{
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++) {
            st->comparisons++;
            if (a[j].key1 < a[min].key1)
                min = j;
        }
        if (min != i)                 /* 인덱스 비교 : 횟수에 포함하지 않음 */
            swap_data(&a[i], &a[min], st);
    }
}

/* [2] 일반 버블 정렬 : 정해진 n-1번의 순회를 모두 수행 */
void bubble_sort(Data a[], int n, Stats *st)
{
    for (int i = 0; i < n - 1; i++) {
        st->passes++;
        for (int j = 0; j < n - 1 - i; j++) {
            st->comparisons++;
            if (a[j].key1 > a[j + 1].key1)
                swap_data(&a[j], &a[j + 1], st);
        }
    }
}

/* [2-1] 조기 종료 버블 정렬 : 한 순회에서 교환이 없으면 종료 */
void bubble_sort_early(Data a[], int n, Stats *st)
{
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        st->passes++;
        for (int j = 0; j < n - 1 - i; j++) {
            st->comparisons++;
            if (a[j].key1 > a[j + 1].key1) {
                swap_data(&a[j], &a[j + 1], st);
                swapped = 1;
            }
        }
        if (!swapped)
            break;
    }
}

/* [3] 삽입 정렬
 *   allow_equal == 0 : Case A  a[j].key1 >  current.key1  (같은 키는 밀지 않음)
 *   allow_equal == 1 : Case B  a[j].key1 >= current.key1  (같은 키도 뒤로 밀어냄)
 */
void insertion_sort(Data a[], int n, int allow_equal, Stats *st)
{
    for (int i = 1; i < n; i++) {
        Data current = a[i];  st->moves++;          /* 임시 변수에 저장 */
        int j = i - 1;

        while (j >= 0) {                            /* 인덱스 검사 : 횟수 제외 */
            int need_shift;
            st->comparisons++;
            if (allow_equal)
                need_shift = (a[j].key1 >= current.key1);   /* Case B */
            else
                need_shift = (a[j].key1 >  current.key1);   /* Case A */

            if (!need_shift)
                break;
            a[j + 1] = a[j];  st->moves++;          /* 한 칸 뒤로 이동 */
            j--;
        }
        a[j + 1] = current;  st->moves++;           /* 최종 위치에 대입 */
    }
}

void insertion_sort_gt(Data a[], int n, Stats *st) { insertion_sort(a, n, 0, st); }
void insertion_sort_ge(Data a[], int n, Stats *st) { insertion_sort(a, n, 1, st); }

/* ------------------------------------------------------------------ */
/* 실험 대상 목록                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;       /* 성능 출력용 이름 */
    const char *stab_name;  /* 안정성 출력용 이름 */
    SortFunc    sort;
} Algorithm;

/* 앞의 4개 : 성능 비교 대상 / 5개 전부 : 안정성 검증 대상 */
static const Algorithm ALGS[] = {
    { "Selection Sort",                  "Selection Sort      ", selection_sort    },
    { "Bubble Sort",                     "Bubble Sort         ", bubble_sort       },
    { "Bubble Sort (Early Termination)", "Bubble Sort (ET)    ", bubble_sort_early },
    { "Insertion Sort",                  "Insertion Sort (>)  ", insertion_sort_gt },
    { "Insertion Sort (>=)",             "Insertion Sort (>=) ", insertion_sort_ge },
};
#define PERF_ALG_COUNT 4
#define ALL_ALG_COUNT  5

/* ------------------------------------------------------------------ */
/* 데이터 준비 (여기서의 연산은 비교/이동 횟수에 포함하지 않음)        */
/* ------------------------------------------------------------------ */

/* order를 현재 위치(= 이 입력에서의 원래 순서)로 다시 매김 */
static void renumber(Data a[], int n)
{
    for (int i = 0; i < n; i++)
        a[i].order = i;
}

static void generate_random(Data a[], int n, int key1_max)
{
    for (int i = 0; i < n; i++) {
        a[i].key1 = random_int(key1_max);
        a[i].key2 = random_int(MAX_KEY);
        a[i].order = i;
    }
}

/* 시험 입력 준비용 정렬 : 직접 구현한 삽입 정렬을 쓰되 횟수는 버림 */
static void prepare_sorted(Data dst[], const Data src[], int n)
{
    Stats ignore = {0, 0, 0};
    memcpy(dst, src, sizeof(Data) * n);
    insertion_sort(dst, n, 0, &ignore);
    renumber(dst, n);
}

static void prepare_reverse(Data dst[], const Data sorted[], int n)
{
    for (int i = 0; i < n; i++)
        dst[i] = sorted[n - 1 - i];
    renumber(dst, n);
}

static void raw_swap(Data *x, Data *y)
{
    Data t = *x; *x = *y; *y = t;
}

/* 부분 정렬 (본 실험) : 정렬된 데이터에서 무작위 두 위치를 swaps번 교환 */
static void prepare_partial_swaps(Data dst[], const Data sorted[], int n, int swaps)
{
    memcpy(dst, sorted, sizeof(Data) * n);
    for (int k = 0; k < swaps; k++) {
        int i = random_int(n - 1);
        int j = random_int(n - 1);
        raw_swap(&dst[i], &dst[j]);
    }
    renumber(dst, n);
}

/* 구간 [lo, hi) 무작위 섞기 (Fisher-Yates) */
static void shuffle_range(Data a[], int lo, int hi)
{
    for (int i = hi - 1; i > lo; i--) {
        int j = lo + random_int(i - lo);
        raw_swap(&a[i], &a[j]);
    }
}

/* 부분 정렬 (추가 분석) : 크기 block인 블록 내부만 섞기 */
static void prepare_block_shuffle(Data dst[], const Data sorted[], int n, int block)
{
    memcpy(dst, sorted, sizeof(Data) * n);
    for (int lo = 0; lo < n; lo += block) {
        int hi = (lo + block < n) ? lo + block : n;
        shuffle_range(dst, lo, hi);
    }
    renumber(dst, n);
}

/* 부분 정렬 (추가 분석) : 앞부분은 정렬 유지, 뒤쪽 tail개만 섞기 */
static void prepare_tail_shuffle(Data dst[], const Data sorted[], int n, int tail)
{
    memcpy(dst, sorted, sizeof(Data) * n);
    shuffle_range(dst, n - tail, n);
    renumber(dst, n);
}

/* ------------------------------------------------------------------ */
/* 검증 및 분석 도구                                                   */
/* ------------------------------------------------------------------ */

/* 정렬 결과가 (1) Key 1 오름차순이고 (2) 입력 데이터를 빠짐없이 그대로 담고 있는지 확인 */
static int is_valid_result(const Data out[], const Data input[], int n)
{
    unsigned char *seen = calloc((size_t)n, 1);
    int ok = 1;

    if (seen == NULL) { fprintf(stderr, "memory error\n"); exit(1); }

    for (int i = 0; i < n && ok; i++) {
        int k = out[i].order;
        if (i > 0 && out[i - 1].key1 > out[i].key1) ok = 0;            /* 오름차순 위반 */
        else if (k < 0 || k >= n || seen[k]) ok = 0;                   /* 누락/중복 */
        else if (input[k].key1 != out[i].key1 ||
                 input[k].key2 != out[i].key2) ok = 0;                 /* 두 키가 함께 이동했는지 */
        else seen[k] = 1;
    }
    free(seen);
    return ok;
}

/* Key 1이 같은 인접 쌍 중 원래 순서가 뒤집힌 쌍의 수 (0이면 안정) */
static long long count_stability_violations(const Data a[], int n)
{
    long long v = 0;
    for (int i = 1; i < n; i++)
        if (a[i - 1].key1 == a[i].key1 && a[i - 1].order > a[i].order)
            v++;
    return v;
}

/* 정렬 결과에서 Key 1이 같은 인접 쌍의 수 (= 안정성 검사 대상 쌍 수) */
static long long count_equal_pairs(const Data a[], int n)
{
    long long c = 0;
    for (int i = 1; i < n; i++)
        if (a[i - 1].key1 == a[i].key1)
            c++;
    return c;
}

/* 역순쌍(Inversion) 수 : i < j 인데 a[i].key1 > a[j].key1 인 쌍의 수 */
static long long count_inversions(const Data a[], int n)
{
    long long inv = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (a[i].key1 > a[j].key1)
                inv++;
    return inv;
}

/* 정렬된 데이터와 비교해 제자리에 있지 않은 원소 수 */
static int count_out_of_place(const Data a[], const Data sorted[], int n)
{
    int c = 0;
    for (int i = 0; i < n; i++)
        if (a[i].key1 != sorted[i].key1)
            c++;
    return c;
}

/* ------------------------------------------------------------------ */
/* 정렬 1회 실행 : 입력을 복사해서 정렬하고 결과를 검증                */
/* ------------------------------------------------------------------ */

static Data work[N];

static int run_sort(const Algorithm *alg, const Data input[], int n,
                    Stats *st, long long *violations)
{
    memcpy(work, input, sizeof(Data) * n);   /* 실험용 복사 : 횟수 제외 */
    st->comparisons = st->moves = st->passes = 0;

    alg->sort(work, n, st);

    if (violations != NULL)
        *violations = count_stability_violations(work, n);
    return is_valid_result(work, input, n);
}

/* ------------------------------------------------------------------ */
/* 출력 보조                                                           */
/* ------------------------------------------------------------------ */

static const double MAX_INV = (double)N * (N - 1) / 2.0;

static void print_line(void)
{
    printf("------------------------------------\n\n");
}

static void print_single_result(const Algorithm *alg, const Stats *st, int ok, int show_passes)
{
    printf("%s\n", alg->name);
    printf("Comparisons : %lld\n", st->comparisons);
    printf("Movements   : %lld\n", st->moves);
    if (show_passes)
        printf("Passes      : %lld\n", st->passes);
    printf("Sorted Check: %s\n\n", ok ? "PASS" : "FAIL");
}

/* ------------------------------------------------------------------ */
/* 실험 1 : 무작위 데이터 100회                                        */
/* ------------------------------------------------------------------ */

static Data base[N];

/* 무작위 실험 중 수집한 안정성 결과 (안정성 섹션에서 출력) */
static int       random_unstable_trials[ALL_ALG_COUNT];
static long long random_violations[ALL_ALG_COUNT];
static long long random_equal_pairs_total;

static void random_experiment(void)
{
    Stats total[ALL_ALG_COUNT];
    int   pass_count[ALL_ALG_COUNT];
    long long inversions_total = 0;

    memset(total, 0, sizeof total);
    memset(pass_count, 0, sizeof pass_count);

    for (int t = 0; t < TRIALS; t++) {
        generate_random(base, N, MAX_KEY);          /* 이번 반복의 공통 입력 */
        inversions_total += count_inversions(base, N);

        for (int a = 0; a < ALL_ALG_COUNT; a++) {
            Stats st;
            long long v;
            if (run_sort(&ALGS[a], base, N, &st, &v))
                pass_count[a]++;
            total[a].comparisons += st.comparisons;
            total[a].moves       += st.moves;
            total[a].passes      += st.passes;
            random_violations[a] += v;
            if (v > 0)
                random_unstable_trials[a]++;
            if (a == 0)
                random_equal_pairs_total += count_equal_pairs(work, N);
        }
        fprintf(stderr, "\r  random trials : %3d / %d", t + 1, TRIALS);
    }
    fprintf(stderr, "\n");

    printf("Random Data\n");
    printf("Number of Trials   : %d\n", TRIALS);
    printf("Average Inversions : %.2f (%.2f%% of max)\n\n",
           (double)inversions_total / TRIALS,
           100.0 * inversions_total / TRIALS / MAX_INV);

    for (int a = 0; a < PERF_ALG_COUNT; a++) {
        printf("%s\n", ALGS[a].name);
        printf("Total Comparisons   : %lld\n", total[a].comparisons);
        printf("Average Comparisons : %.2f\n", (double)total[a].comparisons / TRIALS);
        printf("Total Movements     : %lld\n", total[a].moves);
        printf("Average Movements   : %.2f\n", (double)total[a].moves / TRIALS);
        if (a == 1 || a == 2)
            printf("Average Passes      : %.2f\n", (double)total[a].passes / TRIALS);
        printf("Sorted Check        : %d / %d PASS\n\n", pass_count[a], TRIALS);
    }

    printf("Bubble Sort vs Early Termination\n");
    printf("Saved Comparisons (total)   : %lld\n",
           total[1].comparisons - total[2].comparisons);
    printf("Saved Comparisons (average) : %.2f (%.4f%%)\n\n",
           (double)(total[1].comparisons - total[2].comparisons) / TRIALS,
           100.0 * (total[1].comparisons - total[2].comparisons) / total[1].comparisons);
}

/* ------------------------------------------------------------------ */
/* 실험 2 : 입력 상태별 (정렬 / 역순 / 부분 정렬)                       */
/* ------------------------------------------------------------------ */

static Data origin[N], sorted_data[N], reverse_data[N], partial_data[N], variant[N];

static void run_state(const char *title, const Data input[])
{
    long long inv = count_inversions(input, N);

    if (title[0] != '\0')
        printf("%s\n", title);
    printf("Inversions   : %lld (%.2f%% of max)\n\n", inv, 100.0 * inv / MAX_INV);

    for (int a = 0; a < PERF_ALG_COUNT; a++) {
        Stats st;
        int ok = run_sort(&ALGS[a], input, N, &st, NULL);
        print_single_result(&ALGS[a], &st, ok, a == 1 || a == 2);
    }
}

static void state_experiment(void)
{
    int swaps = (int)(N * PARTIAL_SWAP_RATE);

    generate_random(origin, N, MAX_KEY);            /* 세 입력의 공통 원본 */
    prepare_sorted(sorted_data, origin, N);
    prepare_reverse(reverse_data, sorted_data, N);
    prepare_partial_swaps(partial_data, sorted_data, N, swaps);

    print_line();
    run_state("Sorted Data", sorted_data);

    print_line();
    run_state("Reverse-Sorted Data", reverse_data);

    print_line();
    printf("Partially Sorted Data\n");
    printf("Method       : sorted data + %d random position swaps (%.0f%% of N)\n",
           swaps, PARTIAL_SWAP_RATE * 100);
    printf("Out of Place : %d / %d (%.2f%%)\n",
           count_out_of_place(partial_data, sorted_data, N), N,
           100.0 * count_out_of_place(partial_data, sorted_data, N) / N);
    run_state("", partial_data);
}

/* ------------------------------------------------------------------ */
/* 실험 3 : 안정성 검증                                                */
/* ------------------------------------------------------------------ */

static Data stab_data[N];

static void print_small(const Data a[], int n)
{
    for (int i = 0; i < n; i++)
        printf("(%d,%d) ", a[i].key1, a[i].key2);
}

static void stability_experiment(void)
{
    int unstable[ALL_ALG_COUNT] = {0};

    printf("Stability Verification\n");
    printf("Method : store original input order, sort by Key 1 only,\n");
    printf("         then check every adjacent pair with equal Key 1 keeps order ascending\n\n");

    /* [Test 1] 과제 예시 데이터 */
    {
        Data ex[4] = { {527, 100, 0}, {527, 200, 1}, {125, 300, 2}, {892, 400, 3} };
        printf("[Test 1] Example data from the assignment\n");
        printf("Input                : ");
        print_small(ex, 4);
        printf("\n");
        for (int a = 0; a < ALL_ALG_COUNT; a++) {
            Stats st;
            long long v;
            int ok = run_sort(&ALGS[a], ex, 4, &st, &v);
            printf("%s : ", ALGS[a].stab_name);
            print_small(work, 4);
            printf("-> %s%s\n", v ? "Unstable" : "Stable", ok ? "" : " (SORT FAIL)");
            if (v) unstable[a] = 1;
        }
        printf("\n");
    }

    /* [Test 2] 중복이 많은 시험 데이터 */
    {
        long long equal_pairs;
        Stats st;
        long long v;

        generate_random(stab_data, N, STAB_KEY_RANGE - 1);
        run_sort(&ALGS[1], stab_data, N, &st, NULL);
        equal_pairs = count_equal_pairs(work, N);

        printf("[Test 2] %d items, Key 1 in [0, %d] (duplicate-heavy)\n", N, STAB_KEY_RANGE - 1);
        printf("Equal-Key Adjacent Pairs : %lld\n", equal_pairs);
        printf("%-21s  %-9s %12s %12s %12s  %s\n",
               "Algorithm", "Result", "Violations", "Comparisons", "Movements", "Check");
        for (int a = 0; a < ALL_ALG_COUNT; a++) {
            int ok = run_sort(&ALGS[a], stab_data, N, &st, &v);
            printf("%s   %-9s %12lld %12lld %12lld  %s\n",
                   ALGS[a].stab_name, v ? "Unstable" : "Stable",
                   v, st.comparisons, st.moves, ok ? "PASS" : "FAIL");
            if (v) unstable[a] = 1;
        }
        printf("\n");
    }

    /* [Test 3] 무작위 100회 실험에서 수집한 결과 */
    printf("[Test 3] Random data, %d trials (Key 1 in [0, %d])\n", TRIALS, MAX_KEY);
    printf("Equal-Key Adjacent Pairs : %lld total (%.2f per trial)\n",
           random_equal_pairs_total, (double)random_equal_pairs_total / TRIALS);
    for (int a = 0; a < ALL_ALG_COUNT; a++) {
        printf("%s : violations in %3d / %d trials (total %lld pairs)\n",
               ALGS[a].stab_name, random_unstable_trials[a], TRIALS, random_violations[a]);
        if (random_unstable_trials[a]) unstable[a] = 1;
    }
    printf("\n");

    /* 최종 결과 */
    printf("Result\n");
    for (int a = 0; a < ALL_ALG_COUNT; a++)
        printf("%s : %s\n", ALGS[a].stab_name, unstable[a] ? "Unstable" : "Stable");
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* 추가 분석 : 부분 정렬 방식에 따른 차이                              */
/* ------------------------------------------------------------------ */

static void variant_row(const char *label, const Data input[])
{
    Stats be, ins;
    int ok1 = run_sort(&ALGS[2], input, N, &be, NULL);
    int ok2 = run_sort(&ALGS[3], input, N, &ins, NULL);

    printf("%-26s %11lld %7d %15lld %7lld %15lld %15lld  %s\n",
           label, count_inversions(input, N),
           count_out_of_place(input, sorted_data, N),
           be.comparisons, be.passes, ins.comparisons, ins.moves,
           (ok1 && ok2) ? "PASS" : "FAIL");
}

static void variant_experiment(void)
{
    char label[64];

    printf("Partially Sorted Variants (Additional Analysis)\n");
    printf("All variants are made from the same sorted data used above.\n\n");
    printf("%-26s %11s %7s %15s %7s %15s %15s  %s\n",
           "Method", "Inversions", "OutPos", "BubbleET Comp", "Passes",
           "Insertion Comp", "Insertion Move", "Check");

    snprintf(label, sizeof label, "Random swaps (%.0f%%)", PARTIAL_SWAP_RATE * 100);
    variant_row(label, partial_data);

    prepare_block_shuffle(variant, sorted_data, N, BLOCK_SIZE);
    snprintf(label, sizeof label, "Block shuffle (size %d)", BLOCK_SIZE);
    variant_row(label, variant);

    prepare_tail_shuffle(variant, sorted_data, N, (int)(N * TAIL_RATE));
    snprintf(label, sizeof label, "Tail shuffle (last %.0f%%)", TAIL_RATE * 100);
    variant_row(label, variant);
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc > 1) {
        unsigned long s = strtoul(argv[1], NULL, 10);
        rng_state = (s != 0) ? (unsigned int)s : DEFAULT_SEED;
    }

    printf("Data Count : %d\n", N);
    printf("Seed       : %u\n\n", rng_state);

    random_experiment();
    state_experiment();

    print_line();
    stability_experiment();

    print_line();
    variant_experiment();

    return 0;
}