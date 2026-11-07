#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define SEED        20261107u
#define MAX_VALUE   100000
#define BONUS_SIZE  10000
#define BONUS_REPS  100

typedef enum { PIVOT_FIRST, PIVOT_RANDOM, PIVOT_MEDIAN3 } PivotType;

static long long cmp_count;

static int less(int a, int b)    { cmp_count++; return a < b; }
static int greater(int a, int b) { cmp_count++; return a > b; }

static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

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

static int select_pivot(int a[], int left, int right, PivotType type)
{
    switch (type) {
    case PIVOT_FIRST:  return left;
    case PIVOT_RANDOM: return left + rand() % (right - left + 1);
    case PIVOT_MEDIAN3:
    default:           return median_of_three(a, left, right);
    }
}

static int partition(int a[], int left, int right)
{
    int pivot = a[left];
    int i = left, j = right + 1;

    for (;;) {
        do { i++; } while (i <= right && less(a[i], pivot));
        do { j--; } while (greater(a[j], pivot));
        if (i >= j) break;
        swap(&a[i], &a[j]);
    }
    swap(&a[left], &a[j]);
    return j;
}

static int choose_and_partition(int a[], int left, int right, PivotType type)
{
    int p = select_pivot(a, left, right, type);
    swap(&a[left], &a[p]);
    return partition(a, left, right);
}

static void quick_sort_rec(int a[], int left, int right, PivotType type)
{
    if (left >= right) return;
    int p = choose_and_partition(a, left, right, type);
    quick_sort_rec(a, left, p - 1, type);
    quick_sort_rec(a, p + 1, right, type);
}

static void quick_sort_iter(int a[], int n, PivotType type)
{
    int *stack;
    int top = -1;
    size_t count, stack_bytes;

    if (n <= 1) return;

    count = (size_t)n;
    if (count > SIZE_MAX / (2 * sizeof(int)) - 1) {
        fprintf(stderr, "quick_sort_iter: stack size overflow\n");
        return;
    }
    stack_bytes = 2 * (count + 1) * sizeof(int);
    stack = malloc(stack_bytes);
    if (stack == NULL) {
        fprintf(stderr, "quick_sort_iter: stack allocation failed\n");
        return;
    }

    stack[++top] = 0;
    stack[++top] = n - 1;

    while (top >= 0) {
        int right = stack[top--];
        int left  = stack[top--];
        if (left >= right) continue;

        int p = choose_and_partition(a, left, right, type);

        stack[++top] = p + 1; stack[++top] = right;
        stack[++top] = left;  stack[++top] = p - 1;
    }
    free(stack);
}

static int is_sorted(const int a[], int n)
{
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

static void generate(int a[], int n)
{
    for (int i = 0; i < n; i++) a[i] = rand() % MAX_VALUE;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(void)
{
    const int sizes[] = { 1000, 2000, 4000, 8000, 16000, 20000 };
    const int nsizes = sizeof(sizes) / sizeof(sizes[0]);
    const char *names[] = { "Quick Sort (First)    ",
                            "Quick Sort (Random)   ",
                            "Quick Sort (Median-3) " };

    srand(SEED);

    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        int *origin = malloc(sizeof(int) * n);
        int *work   = malloc(sizeof(int) * n);
        if (origin == NULL || work == NULL) {
            fprintf(stderr, "main: memory allocation failed (n = %d)\n", n);
            free(origin);
            free(work);
            return 1;
        }
        generate(origin, n);

        printf("Data Size : %d\n\n", n);
        for (int t = PIVOT_FIRST; t <= PIVOT_MEDIAN3; t++) {
            memcpy(work, origin, sizeof(int) * n);
            cmp_count = 0;
            quick_sort_rec(work, 0, n - 1, (PivotType)t);
            printf("%s: %lld comparisons%s\n", names[t], cmp_count,
                   is_sorted(work, n) ? "" : "  [ERROR: not sorted]");
        }
        printf("\n");
        free(origin);
        free(work);
    }

    {
        int n = BONUS_SIZE;
        int *origin = malloc(sizeof(int) * n);
        int *work   = malloc(sizeof(int) * n);
        double rec_total = 0, iter_total = 0, t0;
        int ok = 1;

        if (origin == NULL || work == NULL) {
            fprintf(stderr, "main: memory allocation failed (n = %d)\n", n);
            free(origin);
            free(work);
            return 1;
        }
        generate(origin, n);

        for (int r = 0; r < BONUS_REPS; r++) {
            memcpy(work, origin, sizeof(int) * n);
            t0 = now_ms();
            quick_sort_rec(work, 0, n - 1, PIVOT_MEDIAN3);
            rec_total += now_ms() - t0;
            ok &= is_sorted(work, n);

            memcpy(work, origin, sizeof(int) * n);
            t0 = now_ms();
            quick_sort_iter(work, n, PIVOT_MEDIAN3);
            iter_total += now_ms() - t0;
            ok &= is_sorted(work, n);
        }

        printf("Data Size : %d\n", n);
        printf("Pivot     : Median-of-Three\n");
        printf("(average of %d runs)\n\n", BONUS_REPS);
        printf("Recursive Quick Sort : %.4f ms\n", rec_total / BONUS_REPS);
        printf("Iterative Quick Sort : %.4f ms\n", iter_total / BONUS_REPS);
        if (!ok) printf("[ERROR: not sorted]\n");

        free(origin);
        free(work);
    }
    return 0;
}