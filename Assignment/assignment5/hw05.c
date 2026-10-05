#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_DATA   100
#define N_SEARCH 50
#define MAX_VAL  1000

typedef struct Node {
    int data;
    struct Node *left;
    struct Node *right;
} Node;

/* BST 삽입: 방문한 기존 노드마다 비교 1회로 계산 */
Node *insert(Node *root, int value, int *cmp)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    newNode->data = value;
    newNode->left = newNode->right = NULL;

    if (root == NULL)
        return newNode;

    Node *cur = root;
    while (1) {
        (*cmp)++;
        if (value < cur->data) {
            if (cur->left == NULL) { cur->left = newNode; break; }
            cur = cur->left;
        } else {
            if (cur->right == NULL) { cur->right = newNode; break; }
            cur = cur->right;
        }
    }
    return root;
}

/* 순차 탐색: arr[i]와 key를 비교할 때마다 1회 */
int seqSearch(const int arr[], int n, int key, int *cmp)
{
    for (int i = 0; i < n; i++) {
        (*cmp)++;
        if (arr[i] == key)
            return 1;
    }
    return 0;
}

/* BST 탐색: 실제로 실행되는 숫자 비교(==, <)를 각각 1회로 계산
 *  - 현재 노드와 같으면: == 1회
 *  - 같지 않으면: == 1회 + < 1회 = 2회 */
int bstSearch(const Node *root, int key, int *cmp)
{
    const Node *cur = root;
    while (cur != NULL) {
        (*cmp)++;                       /* key == cur->data */
        if (key == cur->data)
            return 1;
        (*cmp)++;                       /* key < cur->data */
        if (key < cur->data)
            cur = cur->left;
        else
            cur = cur->right;
    }
    return 0;
}

int height(const Node *root)
{
    if (root == NULL) return 0;
    int l = height(root->left);
    int r = height(root->right);
    return (l > r ? l : r) + 1;
}

void freeTree(Node *root)
{
    if (root == NULL) return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

int main(int argc, char *argv[])
{
    unsigned int seed = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10)
                                   : (unsigned int)time(NULL);
    srand(seed);

    int arr[N_DATA];
    int used[MAX_VAL + 1] = {0};
    Node *root = NULL;
    int buildCmp = 0;

    /* 1. 서로 다른 정수 100개 생성 -> 배열(발생 순서 그대로) + BST */
    for (int i = 0; i < N_DATA; ) {
        int v = rand() % (MAX_VAL + 1);
        if (used[v]) continue;          /* 중복이면 다시 생성 */
        used[v] = 1;
        arr[i++] = v;
        root = insert(root, v, &buildCmp);
    }

    printf("Seed: %u\n\n", seed);
    printf("[Generated %d distinct integers]\n", N_DATA);
    for (int i = 0; i < N_DATA; i++)
        printf("%4d%s", arr[i], (i % 10 == 9) ? "\n" : " ");

    printf("\n[BST construction]\n");
    printf("Total comparisons : %d\n", buildCmp);
    printf("Tree height       : %d\n", height(root));

    /* 2. 탐색 대상 50개 생성 */
    int keys[N_SEARCH];
    for (int i = 0; i < N_SEARCH; i++)
        keys[i] = rand() % (MAX_VAL + 1);

    printf("\n[Search results]\n");
    printf("%-4s %-10s %-10s %-12s %-12s\n",
           "No.", "Key", "Result", "Seq cmp", "BST cmp");

    long seqTotal = 0, bstTotal = 0;
    int found = 0;
    int seqMax = 0, bstMax = 0;

    for (int i = 0; i < N_SEARCH; i++) {
        int sc = 0, bc = 0;
        int sr = seqSearch(arr, N_DATA, keys[i], &sc);
        int br = bstSearch(root, keys[i], &bc);

        if (sr != br) {
            fprintf(stderr, "Error: search results differ for key %d\n", keys[i]);
            return 1;
        }
        if (sr) found++;
        seqTotal += sc;
        bstTotal += bc;
        if (sc > seqMax) seqMax = sc;
        if (bc > bstMax) bstMax = bc;

        printf("%-4d %-10d %-10s %-12d %-12d\n",
               i + 1, keys[i], sr ? "Found" : "Not found", sc, bc);
    }

    double seqAvg = (double)seqTotal / N_SEARCH;
    double bstAvg = (double)bstTotal / N_SEARCH;

    printf("\nNumber of searches: %d (found %d, not found %d)\n",
           N_SEARCH, found, N_SEARCH - found);

    printf("\nSequential Search\n");
    printf("Total comparisons   : %ld\n", seqTotal);
    printf("Average comparisons : %.2f\n", seqAvg);
    printf("Max comparisons     : %d\n", seqMax);

    printf("\nBST Search\n");
    printf("Total comparisons   : %ld\n", bstTotal);
    printf("Average comparisons : %.2f\n", bstAvg);
    printf("Max comparisons     : %d\n", bstMax);

    /* 5. BST 생성 비용까지 고려한 비교 */
    long bstAll = bstTotal + buildCmp;
    printf("\n[Cost comparison including BST construction]\n");
    printf("(Measured on these %d searches)\n", N_SEARCH);
    printf("Sequential total             : %ld\n", seqTotal);
    printf("BST build + search total     : %ld (build %d + search %ld)\n",
           bstAll, buildCmp, bstTotal);
    printf("Difference (Seq - BST total) : %ld\n", seqTotal - bstAll);

    /* 손익분기점은 측정값이 아니라 추정치:
     * 이번 50회 탐색의 평균 절감량이 이후 탐색에서도 유지된다고 가정하고 외삽함 */
    double saving = seqAvg - bstAvg;   /* 탐색 1회당 평균 절감 비교 횟수 */
    printf("\n[Break-even estimate - extrapolation, not an exact value]\n");
    if (saving > 0) {
        double est = buildCmp / saving;
        int estCeil = (int)est;
        if (est > estCeil) estCeil++;
        printf("Average saving per search    : %.2f (Seq avg - BST avg)\n", saving);
        printf("Estimated break-even         : %d / %.2f = %.2f -> about %d searches\n",
               buildCmp, saving, est, estCeil);
        if (estCeil > N_SEARCH)
            printf("Note: estimate exceeds the %d measured searches (pure extrapolation)\n",
                   N_SEARCH);
        printf("Assumption: the average saving measured on these %d keys stays the same\n"
               "            for later searches (same data, no more insertions, similar keys).\n",
               N_SEARCH);
    } else {
        printf("BST is not faster per search on these keys, so no break-even is estimated.\n");
    }

    freeTree(root);
    return 0;
}