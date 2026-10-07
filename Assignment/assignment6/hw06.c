#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_GEN 100
#define N_SEARCH 50
#define MAX_VAL 1000

typedef struct Node {
    int key;
    int height;              /* AVL 전용: 노드 수 기준 높이 */
    struct Node *left, *right;
} Node;


/* ---------- 배열 ---------- */
int arr_find(const int *a, int n, int key, long *cnt) {
    for (int i = 0; i < n; i++) {
        (*cnt)++;
        if (a[i] == key) return 1;
    }
    return 0;
}

/* ---------- 공통 ---------- */
Node *new_node(int key) {
    Node *p = (Node *)malloc(sizeof(Node));
    p->key = key; p->height = 1; p->left = p->right = NULL;
    return p;
}

/* 3분 비교(같음/작음/큼)를 노드 하나당 비교 1회로 계산 */
int tree_find(Node *root, int key, long *cnt) {
    Node *cur = root;
    while (cur) {
        (*cnt)++;
        if (key == cur->key) return 1;
        cur = (key < cur->key) ? cur->left : cur->right;
    }
    return 0;
}

int tree_height(Node *p) { /* BST 높이: 재귀 계산 (비교 횟수에 포함 안 함) */
    if (!p) return 0;
    int l = tree_height(p->left), r = tree_height(p->right);
    return (l > r ? l : r) + 1;
}

void tree_free(Node *p) {
    if (!p) return;
    tree_free(p->left); tree_free(p->right); free(p);
}

/* ---------- BST ---------- */
/* 삽입 성공 시 1, 중복이면 0 */
int bst_insert(Node **root, int key, long *cnt) {
    Node **pp = root;
    while (*pp) {
        (*cnt)++;
        if (key == (*pp)->key) return 0;
        pp = (key < (*pp)->key) ? &(*pp)->left : &(*pp)->right;
    }
    *pp = new_node(key);
    return 1;
}

/* ---------- AVL ---------- */
static int h(Node *p) { return p ? p->height : 0; }
static int max2(int a, int b) { return a > b ? a : b; }
static void update(Node *p) { p->height = max2(h(p->left), h(p->right)) + 1; }
static int balance(Node *p) { return h(p->left) - h(p->right); }

static Node *rotate_right(Node *y) { /* LL */
    Node *x = y->left;
    y->left = x->right;
    x->right = y;
    update(y); update(x);
    return x;
}
static Node *rotate_left(Node *x) { /* RR */
    Node *y = x->right;
    x->right = y->left;
    y->left = x;
    update(x); update(y);
    return y;
}

static int inserted; /* 직전 삽입 성공 여부 */

Node *avl_insert_rec(Node *p, int key, long *cnt) {
    if (!p) { inserted = 1; return new_node(key); }
    (*cnt)++;
    if (key == p->key) { inserted = 0; return p; }
    if (key < p->key) p->left = avl_insert_rec(p->left, key, cnt);
    else              p->right = avl_insert_rec(p->right, key, cnt);

    if (!inserted) return p;
    update(p);
    int b = balance(p);
    if (b > 1) {
        if (balance(p->left) < 0) p->left = rotate_left(p->left); /* LR */
        return rotate_right(p);                                   /* LL */
    }
    if (b < -1) {
        if (balance(p->right) > 0) p->right = rotate_right(p->right); /* RL */
        return rotate_left(p);                                        /* RR */
    }
    return p;
}

int avl_insert(Node **root, int key, long *cnt) {
    *root = avl_insert_rec(*root, key, cnt);
    return inserted;
}

int main(int argc, char **argv) {
    srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL));

    int gen[N_GEN];
    int arr[N_GEN], n = 0;
    Node *bst = NULL, *avl = NULL;
    long c_arr = 0, c_bst = 0, c_avl = 0;
    int dup = 0;

    for (int i = 0; i < N_GEN; i++) gen[i] = rand() % (MAX_VAL + 1);

    printf("Generated 100 integers:\n");
    for (int i = 0; i < N_GEN; i++) {
        printf("%4d%s", gen[i], (i % 10 == 9) ? "\n" : " ");
    }

    for (int i = 0; i < N_GEN; i++) {
        int v = gen[i];
        if (!arr_find(arr, n, v, &c_arr)) arr[n++] = v;
        else dup++;
        bst_insert(&bst, v, &c_bst);
        avl_insert(&avl, v, &c_avl);
    }

    printf("\nStored values : %d\n", n);
    printf("Duplicates skipped : %d\n", dup);
    printf("\nConstruction\n");
    printf("Array comparisons : %ld\n", c_arr);
    printf("BST comparisons   : %ld\n", c_bst);
    printf("AVL comparisons   : %ld\n", c_avl);
    printf("\nStructure\n");
    printf("Array length : %d\n", n);
    printf("BST height   : %d\n", tree_height(bst));
    printf("AVL height   : %d\n", tree_height(avl));

    int keys[N_SEARCH];
    for (int i = 0; i < N_SEARCH; i++) keys[i] = rand() % (MAX_VAL + 1);

    printf("\nSearch keys:\n");
    for (int i = 0; i < N_SEARCH; i++)
        printf("%4d%s", keys[i], (i % 10 == 9) ? "\n" : " ");

    long t_seq = 0, t_bst = 0, t_avl = 0;
    for (int i = 0; i < N_SEARCH; i++) {
        long cs = 0, cb = 0, ca = 0;
        int rs = arr_find(arr, n, keys[i], &cs);
        int rb = tree_find(bst, keys[i], &cb);
        int ra = tree_find(avl, keys[i], &ca);
        t_seq += cs; t_bst += cb; t_avl += ca;
        printf("\nSearch Key : %d\n", keys[i]);
        printf("Sequential Search\n  Result      : %s\n  Comparisons : %ld\n", rs ? "Found" : "Not Found", cs);
        printf("BST Search\n  Result      : %s\n  Comparisons : %ld\n", rb ? "Found" : "Not Found", cb);
        printf("AVL Search\n  Result      : %s\n  Comparisons : %ld\n", ra ? "Found" : "Not Found", ca);
    }

    printf("\nSearches : %d\n", N_SEARCH);
    printf("\nSequential Search\nTotal comparisons   : %ld\nAverage comparisons : %.2f\n", t_seq, (double)t_seq / N_SEARCH);
    printf("\nBST Search\nTotal comparisons   : %ld\nAverage comparisons : %.2f\n", t_bst, (double)t_bst / N_SEARCH);
    printf("\nAVL Search\nTotal comparisons   : %ld\nAverage comparisons : %.2f\n", t_avl, (double)t_avl / N_SEARCH);

    tree_free(bst); tree_free(avl);
    return 0;
}