#if !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SEED        20261107u   /* 이전 과제와 같은 고정 시드 */
#ifndef MAX_VALUE
#define MAX_VALUE   100000      /* 데이터 값 범위 : 0 ~ 99999 (중복 허용) */
#endif
#define OP_RATIO    5           /* 연산 횟수 = n / 5 (n의 20%) */
#define TIME_REPS   10          /* 실행시간 평균을 위한 반복 횟수 */

/* ======================= 측정용 전역 변수 ======================= */

static long long cmp_count;             /* 데이터 값 비교 횟수 */
static long long move_count;            /* 데이터 값 복사/대입 횟수 */

static size_t    heap_cur, heap_peak;   /* 자료구조 메모리 (현재 / 최대) */
static uintptr_t stack_base, stack_low; /* 재귀 스택 사용량 측정용 */

/* 데이터 값 비교 : 호출될 때마다 비교 횟수 1 증가 */
static int less(int a, int b)       { cmp_count++; return a < b;  }
static int greater(int a, int b)    { cmp_count++; return a > b;  }
static int less_equal(int a, int b) { cmp_count++; return a <= b; }
static int equal(int a, int b)      { cmp_count++; return a == b; }

/* 데이터 값 대입 : 실행할 때마다 이동 횟수 1 증가 */
#define MOVE(dst, src) do { (dst) = (src); move_count++; } while (0)

static void swap(int *a, int *b)        /* 이동 3회 */
{
    int t;
    MOVE(t, *a);
    MOVE(*a, *b);
    MOVE(*b, t);
}

/* 스택 사용량 기록 : 지역 변수의 주소가 낮을수록 스택을 더 깊이 사용한 것 */
#define STACK_PROBE()                                         \
    do {                                                      \
        volatile char probe_;                                 \
        uintptr_t addr_ = (uintptr_t)&probe_;                 \
        if (addr_ < stack_low) stack_low = addr_;             \
    } while (0)

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

/* ===================== [1] 퀵 정렬 (Median-of-Three) ===================== */

/* a[left], a[mid], a[right]를 lo <= md <= hi 순서로 다시 놓고 mid 반환 (원소 3개 이상일 때만) */
static int median_of_three(int a[], int left, int right)
{
    int mid = left + (right - left) / 2;
    int x, y, z, lo, md, hi;

    MOVE(x, a[left]); MOVE(y, a[mid]); MOVE(z, a[right]);

    if (less(x, y)) {
        if (less(y, z))      { lo = x; md = y; hi = z; }
        else if (less(x, z)) { lo = x; md = z; hi = y; }
        else                 { lo = z; md = x; hi = y; }
    } else {
        if (less(x, z))      { lo = y; md = x; hi = z; }
        else if (less(y, z)) { lo = y; md = z; hi = x; }
        else                 { lo = z; md = y; hi = x; }
    }
    move_count += 3;                     /* lo, md, hi 대입 3회 */

    MOVE(a[left], lo); MOVE(a[mid], md); MOVE(a[right], hi);
    return mid;
}

/* 분할 : 피벗 = a[left] */
static int partition(int a[], int left, int right)
{
    int pivot, i = left, j = right + 1;

    STACK_PROBE();
    MOVE(pivot, a[left]);

    for (;;) {
        do { i++; } while (i <= right && less(a[i], pivot));   /* i <= right : 인덱스 검사 (제외) */
        do { j--; } while (greater(a[j], pivot));              /* a[left] = pivot 에서 반드시 멈춤 */
        if (i >= j) break;
        swap(&a[i], &a[j]);
    }
    swap(&a[left], &a[j]);
    return j;
}

static void quick_sort_rec(int a[], int left, int right)
{
    int p;

    STACK_PROBE();
    if (left < right) {
        if (right - left >= 2) {                        /* 원소 3개 이상 : 중앙값을 맨 앞으로 */
            int mid = median_of_three(a, left, right);
            swap(&a[left], &a[mid]);
        }
        p = partition(a, left, right);
        quick_sort_rec(a, left, p - 1);
        quick_sort_rec(a, p + 1, right);
    }
}

/* 정렬된 배열에 삽입 : 삽입 위치를 선형 탐색 → 뒤의 원소를 한 칸씩 뒤로 이동 */
static void array_insert(int a[], int *size, int v)
{
    int i = 0, j;

    STACK_PROBE();
    while (i < *size && less_equal(a[i], v))   /* v보다 큰 첫 원소 위치 (같은 값 뒤에 삽입) */
        i++;
    for (j = *size; j > i; j--)
        MOVE(a[j], a[j - 1]);
    MOVE(a[i], v);
    (*size)++;
}

/* 정렬된 배열에서 삭제 : 값을 선형 탐색 → 찾으면 뒤의 원소를 한 칸씩 앞으로 이동 */
static int array_delete(int a[], int *size, int v)
{
    int i = 0, j;

    STACK_PROBE();
    while (i < *size && less(a[i], v))         /* v 이상인 첫 원소 위치 (정렬되어 있으므로 여기서 멈춤) */
        i++;
    if (i == *size || !equal(a[i], v))         /* 없으면 삭제하지 않음 */
        return 0;
    for (j = i; j < *size - 1; j++)
        MOVE(a[j], a[j + 1]);
    (*size)--;
    return 1;
}

/* ========================= [2] AVL 트리 ========================= */

typedef struct AVLNode {
    int key;                    /* 데이터 값 */
    int count;                  /* 같은 값의 개수 (중복 저장) */
    int height;                 /* 노드 높이 (잎 = 1) */
    struct AVLNode *left, *right;
} AVLNode;

static int height(const AVLNode *t) { return t ? t->height : 0; }

static void update_height(AVLNode *t)
{
    int hl = height(t->left), hr = height(t->right);
    t->height = (hl > hr ? hl : hr) + 1;
}

/* 회전 : 포인터 연결만 바뀌므로 데이터 이동 없음 */
static AVLNode *rotate_right(AVLNode *y)
{
    AVLNode *x = y->left;
    y->left = x->right;
    x->right = y;
    update_height(y);
    update_height(x);
    return x;
}

static AVLNode *rotate_left(AVLNode *x)
{
    AVLNode *y = x->right;
    x->right = y->left;
    y->left = x;
    update_height(x);
    update_height(y);
    return y;
}

/*
 * 균형 복구 : 높이(균형 인수)만 보고 LL/LR/RR/RL을 판단
 *  → 데이터 값을 비교하지 않으므로 비교 횟수에 포함되지 않음
 */
static AVLNode *rebalance(AVLNode *t)
{
    int bf;

    update_height(t);
    bf = height(t->left) - height(t->right);

    if (bf > 1) {                                                   /* 왼쪽이 무거움 */
        if (height(t->left->left) < height(t->left->right))         /* LR */
            t->left = rotate_left(t->left);
        return rotate_right(t);                                     /* LL */
    }
    if (bf < -1) {                                                  /* 오른쪽이 무거움 */
        if (height(t->right->right) < height(t->right->left))       /* RL */
            t->right = rotate_right(t->right);
        return rotate_left(t);                                      /* RR */
    }
    return t;
}

static AVLNode *avl_insert(AVLNode *t, int v)
{
    STACK_PROBE();

    if (t == NULL) {                                    /* 새 노드 */
        AVLNode *n = track_malloc(sizeof(AVLNode));
        MOVE(n->key, v);                                /* 데이터 값 대입 1회 */
        n->count = 1;
        n->height = 1;
        n->left = n->right = NULL;
        return n;
    }
    if (less(v, t->key))
        t->left = avl_insert(t->left, v);
    else if (greater(v, t->key))
        t->right = avl_insert(t->right, v);
    else {                                              /* 같은 값 : 개수만 증가 */
        t->count++;
        return t;
    }
    return rebalance(t);
}

/* 오른쪽 부분 트리의 최소 노드를 떼어냄 : 왼쪽으로만 내려가므로 값 비교 없음 */
static AVLNode *avl_detach_min(AVLNode *t, AVLNode **min)
{
    STACK_PROBE();

    if (t->left == NULL) {
        *min = t;
        return t->right;
    }
    t->left = avl_detach_min(t->left, min);
    return rebalance(t);
}

static AVLNode *avl_delete(AVLNode *t, int v, int *removed)
{
    STACK_PROBE();

    if (t == NULL)                                      /* 값이 없음 : 삭제하지 않음 */
        return NULL;

    if (less(v, t->key))
        t->left = avl_delete(t->left, v, removed);
    else if (greater(v, t->key))
        t->right = avl_delete(t->right, v, removed);
    else {
        *removed = 1;
        if (t->count > 1) {                             /* 중복 값 : 한 개만 제거 */
            t->count--;
            return t;
        }
        if (t->left == NULL || t->right == NULL) {      /* 자식 0~1개 : 자식으로 대체 */
            AVLNode *child = t->left ? t->left : t->right;
            track_free(t, sizeof(AVLNode));
            return child;
        }
        {                                               /* 자식 2개 : 중위 후속자의 값으로 대체 */
            AVLNode *succ;
            t->right = avl_detach_min(t->right, &succ);
            MOVE(t->key, succ->key);                    /* 데이터 값 복사 1회 */
            t->count = succ->count;                     /* 개수 정보 (데이터 값 아님) */
            track_free(succ, sizeof(AVLNode));
        }
    }
    return rebalance(t);
}

static void avl_free(AVLNode *t)
{
    if (t == NULL) return;
    avl_free(t->left);
    avl_free(t->right);
    track_free(t, sizeof(AVLNode));
}

/* 중위 순회로 out[]에 오름차순으로 꺼냄 (결과 확인용 : 측정 제외) */
static void avl_inorder(const AVLNode *t, int out[], int *k)
{
    if (t == NULL) return;
    avl_inorder(t->left, out, k);
    for (int c = 0; c < t->count; c++)
        out[(*k)++] = t->key;
    avl_inorder(t->right, out, k);
}

/* AVL 조건 확인 (결과 확인용) : 모든 노드의 |균형 인수| <= 1 이고 높이 값이 정확한지 */
static int avl_check(const AVLNode *t, int *h)
{
    int hl, hr;
    if (t == NULL) { *h = 0; return 1; }
    if (!avl_check(t->left, &hl) || !avl_check(t->right, &hr)) return 0;
    *h = (hl > hr ? hl : hr) + 1;
    return (hl - hr <= 1 && hr - hl <= 1 && t->height == *h);
}

/* ========================= 측정 도구 ========================= */

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static int is_sorted(const int a[], int n)
{
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

typedef struct {
    int is_insert;              /* 1 = 삽입, 0 = 삭제 */
    int value;
} Op;

typedef struct {
    double    time_ms;          /* 총 수행시간 (TIME_REPS회 평균) */
    long long comparisons, moves;
    size_t    heap_bytes, stack_bytes;
    int       final_count;
    /* 상세 분석용 (SHOW_DETAIL) */
    long long sort_cmp, sort_move;
    double    sort_ms;          /* 정렬 구간 시간 (TIME_REPS회 평균) */
    int       ins_done, del_done, del_miss, tree_height;
    int       ok;               /* 정렬 결과 / 최종 데이터 확인 */
} Result;

static void reset_counters(volatile char *base)
{
    cmp_count = move_count = 0;
    heap_cur = heap_peak = 0;
    stack_base = stack_low = (uintptr_t)base;
}

/* ---------- [1] 퀵 정렬 + 배열 삽입/삭제 ---------- */
static Result run_quick(const int origin[], int n, const Op ops[], int m, int ins_total,
                        int final_out[], const int ref_sorted[])
{
    Result r;
    volatile char base_marker;
    double total = 0.0, sort_total = 0.0;

    memset(&r, 0, sizeof r);

    for (int rep = 0; rep < TIME_REPS; rep++) {
        int *a, size = n, cap = n + ins_total;   /* 삽입이 모두 들어갈 만큼만 할당 */
        double t0, t1, t2, t3;
        long long c_sort, m_sort;

        reset_counters(&base_marker);

        /* ---- 정렬 구간 ---- */
        t0 = now_ms();
        a = track_malloc(sizeof(int) * (size_t)cap);
        for (int i = 0; i < n; i++)
            MOVE(a[i], origin[i]);               /* 원본 → 배열 적재 (AVL의 노드 적재와 같은 기준) */
        quick_sort_rec(a, 0, n - 1);
        t1 = now_ms();

        c_sort = cmp_count; m_sort = move_count;
        if (rep == 0)                            /* 중간 확인 : 시간 측정 제외 */
            r.ok = is_sorted(a, n) && memcmp(a, ref_sorted, sizeof(int) * (size_t)n) == 0;

        /* ---- 삽입/삭제 구간 ---- */
        t2 = now_ms();
        int ins = 0, del = 0, miss = 0;
        for (int k = 0; k < m; k++) {
            if (ops[k].is_insert) { array_insert(a, &size, ops[k].value); ins++; }
            else if (array_delete(a, &size, ops[k].value)) del++;
            else miss++;
        }
        t3 = now_ms();
        total += (t1 - t0) + (t3 - t2);
        sort_total += t1 - t0;

        if (rep == 0) {
            r.comparisons = cmp_count;
            r.moves       = move_count;
            r.heap_bytes  = heap_peak;
            r.stack_bytes = (size_t)(stack_base - stack_low);
            r.final_count = size;
            r.sort_cmp = c_sort; r.sort_move = m_sort;
            r.ins_done = ins; r.del_done = del; r.del_miss = miss;
            memcpy(final_out, a, sizeof(int) * (size_t)size);
        }
        track_free(a, sizeof(int) * (size_t)cap);
    }
    r.time_ms = total / TIME_REPS;
    r.sort_ms = sort_total / TIME_REPS;
    return r;
}

/* ---------- [2] AVL 트리 정렬 + 트리 삽입/삭제 ---------- */
static Result run_avl(const int origin[], int n, const Op ops[], int m,
                      int final_out[], int buf[], const int ref_sorted[])
{
    Result r;
    volatile char base_marker;
    double total = 0.0, sort_total = 0.0;

    memset(&r, 0, sizeof r);

    for (int rep = 0; rep < TIME_REPS; rep++) {
        AVLNode *root = NULL;
        int count = n, k;
        double t0, t1, t2, t3;
        long long c_sort, m_sort;

        reset_counters(&base_marker);

        /* ---- 정렬 구간 : 원본을 하나씩 AVL 트리에 삽입 ---- */
        t0 = now_ms();
        for (int i = 0; i < n; i++)
            root = avl_insert(root, origin[i]);
        t1 = now_ms();

        c_sort = cmp_count; m_sort = move_count;
        if (rep == 0) {                          /* 중위 순회로 오름차순 확인 : 측정 제외 */
            k = 0;
            avl_inorder(root, buf, &k);
            r.ok = (k == n) && memcmp(buf, ref_sorted, sizeof(int) * (size_t)n) == 0;
        }

        /* ---- 삽입/삭제 구간 ---- */
        t2 = now_ms();
        int ins = 0, del = 0, miss = 0;
        for (int j = 0; j < m; j++) {
            if (ops[j].is_insert) {
                root = avl_insert(root, ops[j].value);
                count++; ins++;
            } else {
                int removed = 0;
                root = avl_delete(root, ops[j].value, &removed);
                if (removed) { count--; del++; } else miss++;
            }
        }
        t3 = now_ms();
        total += (t1 - t0) + (t3 - t2);
        sort_total += t1 - t0;

        if (rep == 0) {
            int h = 0;
            r.comparisons = cmp_count;
            r.moves       = move_count;
            r.heap_bytes  = heap_peak;
            r.stack_bytes = (size_t)(stack_base - stack_low);
            r.final_count = count;
            r.sort_cmp = c_sort; r.sort_move = m_sort;
            r.ins_done = ins; r.del_done = del; r.del_miss = miss;
            if (!avl_check(root, &h)) r.ok = 0;  /* 정렬 확인 결과와 상관없이 높이도 항상 계산 */
            r.tree_height = h;
            k = 0;
            avl_inorder(root, final_out, &k);    /* 최종 데이터 (배열과 비교용) */
            if (k != count) r.ok = 0;
        }
        avl_free(root);                          /* 해제 : 측정 구간 밖 */
    }
    r.time_ms = total / TIME_REPS;
    r.sort_ms = sort_total / TIME_REPS;
    return r;
}

static int cmp_int(const void *p, const void *q)
{
    int x = *(const int *)p, y = *(const int *)q;
    return (x > y) - (x < y);
}

/* ============================ main ============================ */

int main(void)
{
    const int sizes[] = { 1000, 2000, 4000, 8000, 16000, 20000 };
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    srand(SEED);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s], m = n / OP_RATIO, ins_total = 0;
        int *origin = malloc(sizeof(int) * (size_t)n);
        int *ref    = malloc(sizeof(int) * (size_t)n);
        Op  *ops    = malloc(sizeof(Op) * (size_t)m);
        int *fin_q  = malloc(sizeof(int) * (size_t)(n + m));
        int *fin_a  = malloc(sizeof(int) * (size_t)(n + m));
        int *buf    = malloc(sizeof(int) * (size_t)(n + m));
        char tq[32], ta[32];

        if (!origin || !ref || !ops || !fin_q || !fin_a || !buf) {
            fprintf(stderr, "malloc failed\n");
            free(origin); free(ref); free(ops);
            free(fin_q); free(fin_a); free(buf);
            return 1;
        }

        /* ---- 난수 생성 (측정 제외) : 데이터 → 연산 순서대로 ---- */
        for (int i = 0; i < n; i++)
            origin[i] = rand() % MAX_VALUE;
        for (int k = 0; k < m; k++) {
            ops[k].is_insert = (rand() % 2 == 0);       /* 짝수 = 삽입, 홀수 = 삭제 */
            ops[k].value     = rand() % MAX_VALUE;      /* 삽입/삭제할 값은 새 난수 */
            ins_total += ops[k].is_insert;
        }

        memcpy(ref, origin, sizeof(int) * (size_t)n);  /* 정렬 결과 확인용 정답 */
        qsort(ref, (size_t)n, sizeof(int), cmp_int);

        /* ---- 두 방법에 같은 데이터, 같은 연산 적용 ---- */
        Result q = run_quick(origin, n, ops, m, ins_total, fin_q, ref);
        Result a = run_avl(origin, n, ops, m, fin_a, buf, ref);

        int same = (q.final_count == a.final_count) &&
                   memcmp(fin_q, fin_a, sizeof(int) * (size_t)q.final_count) == 0;

        /* ---- 출력 (측정 제외) ---- */
        snprintf(tq, sizeof tq, "%.4f ms", q.time_ms);
        snprintf(ta, sizeof ta, "%.4f ms", a.time_ms);

        printf("Data Size : %d\n", n);
        printf("Operations: %d\n\n", m);
        printf("%-18s%-16s%s\n", "", "Quick Sort", "AVL Tree");
        printf("------------------------------------------------\n");
        printf("%-18s%-16s%s\n", "Total Time", tq, ta);
        printf("%-18s%-16lld%lld\n", "Comparisons", q.comparisons, a.comparisons);
        printf("%-18s%-16lld%lld\n", "Data Movements", q.moves, a.moves);
        snprintf(tq, sizeof tq, "%zu Bytes", q.heap_bytes + q.stack_bytes);
        snprintf(ta, sizeof ta, "%zu Bytes", a.heap_bytes + a.stack_bytes);
        printf("%-18s%-16s%s\n\n", "Memory Usage", tq, ta);
        printf("%-18s%-16d%d\n", "Final Data Count", q.final_count, a.final_count);

#ifdef SHOW_DETAIL
        printf("\n  [detail]           Quick Sort      AVL Tree\n");
        printf("  sort  cmp        %-16lld%lld\n", q.sort_cmp, a.sort_cmp);
        printf("  sort  move       %-16lld%lld\n", q.sort_move, a.sort_move);
        printf("  sort  ms         %-16.4f%.4f\n", q.sort_ms, a.sort_ms);
        printf("  ops   cmp        %-16lld%lld\n", q.comparisons - q.sort_cmp, a.comparisons - a.sort_cmp);
        printf("  ops   move       %-16lld%lld\n", q.moves - q.sort_move, a.moves - a.sort_move);
        printf("  heap / stack     %zu / %zu    %zu / %zu\n",
               q.heap_bytes, q.stack_bytes, a.heap_bytes, a.stack_bytes);
        printf("  insert %d, delete %d, delete miss %d, tree height %d, sizeof(AVLNode) %zu\n",
               a.ins_done, a.del_done, a.del_miss, a.tree_height, sizeof(AVLNode));
#endif
        if (!q.ok)  printf("[ERROR] Quick Sort : wrong result\n");
        if (!a.ok)  printf("[ERROR] AVL Tree : wrong result or not balanced\n");
        if (!same)  printf("[ERROR] final data of array and AVL tree differ\n");
        if (q.ins_done != a.ins_done || q.del_done != a.del_done)
            printf("[ERROR] operation results differ\n");
        printf("\n");

        free(origin); free(ref); free(ops);
        free(fin_q); free(fin_a); free(buf);
    }
    return 0;
}