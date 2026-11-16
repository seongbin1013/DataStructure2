/* clock_gettime 선언을 위해 (gcc -std=c11 처럼 엄격한 모드에서 필요).
   macOS 에서 컴파일 오류가 나면 이 두 줄을 지워도 됨 */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SEED        20261107u   /* 과제 12, 13과 같은 고정 시드 */
#define MAX_VALUE   100000      /* 데이터 값 범위 : 0 ~ 99999 (중복 허용) */
#define TIME_REPS   10          /* 실행시간 평균을 위한 반복 횟수 */

/* 보충 실험용 : 0 = 무작위(기본), 1 = 오름차순 입력  (gcc -DINPUT_SORTED=1 ...) */
#ifndef INPUT_SORTED
#define INPUT_SORTED 0
#endif

typedef enum { PIVOT_FIRST, PIVOT_RANDOM, PIVOT_MEDIAN3 } PivotType;

/* ======================= 측정용 전역 변수 ======================= */

static long long cmp_count;         /* 숫자 비교 횟수 */
static long long heap_build_cmp;    /* 그중 힙 정렬의 힙 구성 단계 비교 횟수 (분석용) */

static size_t    heap_cur, heap_peak;   /* 힙 추가 메모리 (현재 / 최대) */

static uintptr_t stack_base;        /* 정렬 시작 직전의 스택 주소 */
static uintptr_t stack_low;         /* 정렬 중 도달한 가장 깊은(낮은) 스택 주소 */
static int       depth_cur, depth_max;  /* 재귀 깊이 (README 분석용) */

/* 데이터 값 비교 : 호출될 때마다 비교 횟수 1 증가 */
static int less(int a, int b)       { cmp_count++; return a < b;  }
static int greater(int a, int b)    { cmp_count++; return a > b;  }
static int less_equal(int a, int b) { cmp_count++; return a <= b; }

static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/*
 * Random 피벗용 난수
 *  - 첫 번째 정렬 : 과제 12와 똑같이 rand() 사용
 *    → 데이터 생성과 피벗 선택이 rand()를 쓰는 순서가 과제 12와 같아져서,
 *      다음 크기의 데이터도 과제 12와 같게 만들어짐
 *  - 실행시간 평균을 위한 추가 반복 : 별도 난수(LCG)를 사용해 rand() 순서를 건드리지 않음
 */
static int      use_private_rng;
static unsigned private_rng_state = 12345u;

static int pivot_rand(void)
{
    if (!use_private_rng) return rand();
    private_rng_state = private_rng_state * 1103515245u + 12345u;
    return (int)((private_rng_state >> 16) & 0x7FFF);
}

/* 스택 사용량 기록 : 지역 변수의 주소가 낮을수록 스택을 더 깊이 사용한 것 */
#define STACK_PROBE()                                         \
    do {                                                      \
        volatile char probe_;                                 \
        uintptr_t addr_ = (uintptr_t)&probe_;                 \
        if (addr_ < stack_low) stack_low = addr_;             \
    } while (0)

/* 힙 사용량 기록 */
static void *track_malloc(size_t bytes)
{
    void *p = malloc(bytes);
    if (p == NULL) { fprintf(stderr, "malloc failed\n"); exit(1); }
    heap_cur += bytes;
    if (heap_cur > heap_peak) heap_peak = heap_cur;
    return p;
}

static void track_free(void *p, size_t bytes)
{
    free(p);
    heap_cur -= bytes;
}

/* ========================= 힙 정렬 ========================= */

/*
 * a[0..size-1]에서 root를 뿌리로 하는 부분 트리를 최대 힙으로 복구 (아래로 내려 보내기)
 *  - 배열 자체가 힙 : i의 왼쪽 자식 = 2i + 1, 오른쪽 자식 = 2i + 2
 *  - 반복문으로 구현 → 재귀 호출 스택이 쌓이지 않음
 *  - 한 단계마다 값 비교 최대 2회 : (왼쪽 자식 vs 오른쪽 자식), (큰 자식 vs 내려가는 값)
 */
static void sift_down(int a[], int root, int size)
{
    int parent = root, child;

    STACK_PROBE();

    while ((child = 2 * parent + 1) < size) {             /* 인덱스 검사 : 비교 횟수 제외 */
        if (child + 1 < size && less(a[child], a[child + 1]))
            child++;                                       /* 두 자식 중 큰 쪽 선택 */
        if (!less(a[parent], a[child]))                    /* 부모 >= 큰 자식 : 힙 속성 만족 */
            break;
        swap(&a[parent], &a[child]);
        parent = child;
    }
}

static void heap_sort(int a[], int n)
{
    int i;

    STACK_PROBE();

    /* 1) 최대 힙 구성 : 마지막 내부 노드(n/2 - 1)부터 루트까지 sift_down */
    for (i = n / 2 - 1; i >= 0; i--)
        sift_down(a, i, n);
    heap_build_cmp = cmp_count;      /* 힙 구성까지의 비교 횟수를 따로 기록 */

    /* 2) 루트(최댓값)를 힙의 마지막 원소와 교환 → 힙 크기 1 감소 → 힙 속성 복구 */
    for (i = n - 1; i > 0; i--) {
        swap(&a[0], &a[i]);
        sift_down(a, 0, i);
    }
}

/* ========================= 병합 정렬 ========================= */

/* 정렬된 a[left..mid]와 a[mid+1..right]를 tmp를 이용해 병합 */
static void merge(int a[], int tmp[], int left, int mid, int right)
{
    int i = left, j = mid + 1, k = left;

    STACK_PROBE();

    while (i <= mid && j <= right) {          /* 인덱스 검사 : 비교 횟수 제외 */
        if (less_equal(a[i], a[j]))           /* 값 비교 : 비교 횟수 포함 (<= 이라 안정 정렬) */
            tmp[k++] = a[i++];
        else
            tmp[k++] = a[j++];
    }
    while (i <= mid)   tmp[k++] = a[i++];     /* 남은 원소 복사 : 비교 없음 */
    while (j <= right) tmp[k++] = a[j++];

    for (k = left; k <= right; k++)
        a[k] = tmp[k];
}

static void merge_sort_rec(int a[], int tmp[], int left, int right)
{
    int mid;

    STACK_PROBE();
    if (++depth_cur > depth_max) depth_max = depth_cur;

    if (left < right) {
        mid = left + (right - left) / 2;
        merge_sort_rec(a, tmp, left, mid);
        merge_sort_rec(a, tmp, mid + 1, right);
        merge(a, tmp, left, mid, right);
    }
    depth_cur--;
}

static void merge_sort(int a[], int n)
{
    int *tmp = track_malloc(sizeof(int) * (size_t)n);   /* 임시 배열 n칸 (한 번만 할당) */
    merge_sort_rec(a, tmp, 0, n - 1);
    track_free(tmp, sizeof(int) * (size_t)n);
}

/* ========================= 퀵 정렬 ========================= */

/*
 * 세 원소의 중앙값 (과제 12의 median_of_three()와 같은 코드)
 *  - a[left], a[mid], a[right]를 비교해(2~3회) lo <= md <= hi 순서로 다시 놓고 mid 반환
 *  - 원소가 3개 이상일 때만 호출해야 함
 *    (2개이면 mid == left가 되어 a[left]와 a[mid]에 같은 칸을 두 번 쓰므로 값 하나가 사라짐.
 *     과제 12 코드에는 이 검사가 없었고, 과제 13에서 select_pivot()에 추가함)
 */
static int median_of_three(int a[], int left, int right)
{
    int mid = left + (right - left) / 2;
    int x = a[left], y = a[mid], z = a[right];
    int lo, md, hi;

    if (less(x, y)) {
        if (less(y, z))      { lo = x; md = y; hi = z; }
        else if (less(x, z)) { lo = x; md = z; hi = y; }
        else                 { lo = z; md = x; hi = y; }
    } else {
        if (less(x, z))      { lo = y; md = x; hi = z; }
        else if (less(y, z)) { lo = y; md = z; hi = x; }
        else                 { lo = z; md = y; hi = x; }
    }
    a[left] = lo; a[mid] = md; a[right] = hi;
    return mid;
}

/* 피벗을 골라 a[left]로 옮김 (이 부분만 방법별로 다름) */
static void select_pivot(int a[], int left, int right, PivotType type)
{
    int idx, mid;

    switch (type) {
    case PIVOT_FIRST:
        break;                                         /* a[left] 그대로 */

    case PIVOT_RANDOM:
        idx = left + pivot_rand() % (right - left + 1); /* 분할할 때마다 새로 뽑음 */
        swap(&a[left], &a[idx]);
        break;

    case PIVOT_MEDIAN3:
        if (right - left < 2) break;                   /* 원소 2개 : 가운데 원소가 없으므로 첫 원소 사용 */
        mid = median_of_three(a, left, right);
        swap(&a[left], &a[mid]);                       /* 중앙값을 맨 앞으로 */
        break;
    }
}

/* 세 방법 공통 분할 : 피벗 = a[left]  (과제 12의 partition()과 같은 코드) */
static int partition(int a[], int left, int right)
{
    int pivot = a[left];
    int i = left, j = right + 1;

    STACK_PROBE();

    for (;;) {
        do { i++; } while (i <= right && less(a[i], pivot));   /* i <= right : 인덱스 검사 (제외) */
        do { j--; } while (greater(a[j], pivot));              /* a[left] = pivot 에서 반드시 멈춤 */
        if (i >= j) break;
        swap(&a[i], &a[j]);
    }
    swap(&a[left], &a[j]);
    return j;
}

static void quick_sort_rec(int a[], int left, int right, PivotType type)
{
    int p;

    STACK_PROBE();
    if (++depth_cur > depth_max) depth_max = depth_cur;

    if (left < right) {
        select_pivot(a, left, right, type);
        p = partition(a, left, right);
        quick_sort_rec(a, left, p - 1, type);
        quick_sort_rec(a, p + 1, right, type);
    }
    depth_cur--;
}

static void quick_sort(int a[], int n, PivotType type)
{
    quick_sort_rec(a, 0, n - 1, type);   /* 보조 자료구조 없음 : 재귀 스택만 사용 */
}

/* ========================= 측정 도구 ========================= */

static int is_sorted(const int a[], int n)
{
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* 기준 결과 : 표준 라이브러리 qsort()로 정렬한 원본 (값이 사라지거나 바뀌지 않았는지 확인용) */
static int cmp_int(const void *p, const void *q)
{
    int x = *(const int *)p, y = *(const int *)q;
    return (x > y) - (x < y);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* 알고리즘 번호 : 0 = Heap, 1 = Merge, 2 = Quick(First), 3 = Quick(Random), 4 = Quick(Median3) */
#define NUM_ALGOS 5

static void run_sort(int algo, int a[], int n)
{
    if      (algo == 0) heap_sort(a, n);
    else if (algo == 1) merge_sort(a, n);
    else                quick_sort(a, n, (PivotType)(algo - 2));
}

typedef struct {
    long long comparisons;
    long long build_cmp;      /* 힙 정렬 전용 : 힙 구성 단계 비교 횟수 */
    double    time_ms;
    size_t    heap_bytes;
    size_t    stack_bytes;
    int       depth;
    int       sorted_ok;
} Result;

static Result measure(int algo, const int origin[], const int ref[], int work[], int n)
{
    Result r;
    volatile char base_marker;   /* 정렬 함수 호출 직전의 스택 위치 */
    double total = 0.0, t0, t1;

    for (int rep = 0; rep < TIME_REPS; rep++) {
        memcpy(work, origin, sizeof(int) * (size_t)n);   /* 복사 : 측정 제외 */
        use_private_rng = (rep > 0);                      /* 첫 회만 rand() 사용 (과제 12와 같은 순서) */

        cmp_count = heap_build_cmp = 0;
        heap_cur = heap_peak = 0;
        depth_cur = depth_max = 0;
        stack_base = stack_low = (uintptr_t)&base_marker;

        t0 = now_ms();
        run_sort(algo, work, n);                          /* 정렬 구간만 측정 */
        t1 = now_ms();
        total += t1 - t0;

        if (rep == 0) {                                   /* 첫 회 결과를 비교 횟수/메모리로 사용 */
            r.comparisons = cmp_count;
            r.build_cmp   = heap_build_cmp;
            r.heap_bytes  = heap_peak;
            r.stack_bytes = (size_t)(stack_base - stack_low);
            r.depth       = depth_max;
            r.sorted_ok   = is_sorted(work, n) &&                       /* 오름차순인지 */
                            memcmp(work, ref, sizeof(int) * (size_t)n) == 0;  /* 원본과 같은 값들인지 */
        }
    }
    r.time_ms = total / TIME_REPS;
    return r;
}

/* ============================ main ============================ */

int main(void)
{
    const int sizes[] = { 1000, 2000, 4000, 8000, 16000, 20000 };
    const char *names[] = { "Heap Sort", "Merge Sort", "Quick (First)", "Quick (Random)", "Quick (Median3)" };
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    srand(SEED);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        int *origin = malloc(sizeof(int) * (size_t)n);   /* 원본 : 공통 비용 (측정 제외) */
        int *work   = malloc(sizeof(int) * (size_t)n);   /* 복사본 : 공통 비용 (측정 제외) */
        int *ref    = malloc(sizeof(int) * (size_t)n);   /* 정답 확인용 (측정 제외) */
        if (origin == NULL || work == NULL || ref == NULL) {
            fprintf(stderr, "malloc failed\n");
            free(origin); free(work); free(ref);
            return 1;
        }

        for (int i = 0; i < n; i++)
            origin[i] = INPUT_SORTED ? i : rand() % MAX_VALUE;

        memcpy(ref, origin, sizeof(int) * (size_t)n);
        qsort(ref, (size_t)n, sizeof(int), cmp_int);

        printf("Data Size : %d\n\n", n);
        printf("%-16s%-15s%-12s%s\n", "Algorithm", "Comparisons", "Time(ms)", "Memory(Bytes)");
        printf("--------------------------------------------------------\n");

        for (int algo = 0; algo < NUM_ALGOS; algo++) {
            Result r = measure(algo, origin, ref, work, n);
            printf("%-16s%-15lld%-12.4f%zu\n",
                   names[algo], r.comparisons, r.time_ms, r.heap_bytes + r.stack_bytes);
#ifdef SHOW_DETAIL
            printf("    -> heap %zu + stack %zu bytes, max depth %d",
                   r.heap_bytes, r.stack_bytes, r.depth);
            if (algo == 0)
                printf(", heap-build comparisons %lld", r.build_cmp);
            printf("\n");
#endif
            if (!r.sorted_ok)
                printf("    [ERROR] %s : wrong result\n", names[algo]);
        }
        printf("\n");

        free(origin);
        free(work);
        free(ref);
    }
    return 0;
}