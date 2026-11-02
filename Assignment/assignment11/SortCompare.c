#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 간격 gap 단위로 삽입 정렬을 수행하고 비교 횟수를 누적한다.
 * gap = 1이면 일반 삽입 정렬과 같다. */
static void gapInsertionSort(int a[], int n, int gap, long long *cmp)
{
    for (int i = gap; i < n; i++) {
        int key = a[i];
        int j = i - gap;
        while (j >= 0) {               /* 인덱스 검사: 비교 횟수 제외 */
            (*cmp)++;                  /* 데이터 값 비교 1회 */
            if (a[j] > key) {
                a[j + gap] = a[j];
                j -= gap;
            } else {
                break;
            }
        }
        a[j + gap] = key;
    }
}

/* [1] 삽입 정렬 */
long long insertionSort(int a[], int n)
{
    long long cmp = 0;
    gapInsertionSort(a, n, 1, &cmp);
    return cmp;
}

/* [2] 쉘 정렬 - 기본 간격 h = n/2, n/4, ..., 1 */
long long shellSortOriginal(int a[], int n)
{
    long long cmp = 0;
    for (int gap = n / 2; gap > 0; gap /= 2)
        gapInsertionSort(a, n, gap, &cmp);
    return cmp;
}

/* [3] 쉘 정렬 - Knuth 수열 h(k+1) = 3h(k) + 1 */
long long shellSortKnuth(int a[], int n)
{
    long long cmp = 0;
    int h = 1;
    while (3 * h + 1 < n)              /* 데이터 크기보다 작은 가장 큰 간격 */
        h = 3 * h + 1;
    for (; h >= 1; h = (h - 1) / 3)    /* 역순 적용, 마지막은 1 */
        gapInsertionSort(a, n, h, &cmp);
    return cmp;
}

/* 오름차순 정렬 확인 (측정 대상 아님) */
int isSorted(const int a[], int n)
{
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

int main(void)
{
    const int sizes[] = {1000, 2000, 4000, 8000, 16000, 20000};
    const int count = sizeof(sizes) / sizeof(sizes[0]);

    srand((unsigned)time(NULL));

    for (int s = 0; s < count; s++) {
        int n = sizes[s];
        int *origin = malloc(sizeof(int) * n);
        int *work   = malloc(sizeof(int) * n);
        if (!origin || !work) {
            fprintf(stderr, "메모리 할당 실패\n");
            return 1;
        }

        /* 원본 데이터 생성 (측정 제외) */
        for (int i = 0; i < n; i++)
            origin[i] = rand() % 100000;

        long long c1, c2, c3;
        int ok = 1;

        memcpy(work, origin, sizeof(int) * n);   /* 복사 (측정 제외) */
        c1 = insertionSort(work, n);
        ok &= isSorted(work, n);

        memcpy(work, origin, sizeof(int) * n);
        c2 = shellSortOriginal(work, n);
        ok &= isSorted(work, n);

        memcpy(work, origin, sizeof(int) * n);
        c3 = shellSortKnuth(work, n);
        ok &= isSorted(work, n);

        printf("Data Size : %d\n\n", n);
        printf("Insertion Sort       : %lld comparisons\n", c1);
        printf("Shell Sort (Original): %lld comparisons\n", c2);
        printf("Shell Sort (Knuth)   : %lld comparisons\n", c3);
        if (!ok) printf("!! 정렬 결과 오류 발생 !!\n");
        printf("\n");

        free(origin);
        free(work);
    }
    return 0;
}