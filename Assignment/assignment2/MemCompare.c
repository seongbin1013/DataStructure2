#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME 32

/* array_bst.c 의 노드 한 칸과 동일한 표현 */
typedef struct { char name[MAX_NAME]; } ArrayNode;

/* linked_bst.c 의 노드와 동일한 표현 */
typedef struct LinkedNode {
    char name[MAX_NAME];
    struct LinkedNode *left, *right;
} LinkedNode;

/* ---- ArrayBtree.c 와 동일한 괄호 표기법 파서 (arr==NULL 이면 크기 측정만) ---- */
static void skipSpace(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}
static void scanTree(const char **p, ArrayNode *arr, int cap, int index, int *maxIndex, int *count) {
    int len = 0;
    const char *start;
    skipSpace(p);
    start = *p;
    while (**p && **p != '(' && **p != ')' && **p != ',' && !isspace((unsigned char)**p)) {
        len++; (*p)++;
    }
    if (len == 0) return;
    if (index > *maxIndex) *maxIndex = index;
    (*count)++;
    if (arr) { /* 2차 파싱: ArrayBtree.c처럼 실제 calloc된 배열에 채워 넣는다 */
        int cplen = len < MAX_NAME - 1 ? len : MAX_NAME - 1;
        memcpy(arr[index].name, start, cplen);
        arr[index].name[cplen] = '\0';
    }
    skipSpace(p);
    if (**p == '(') {
        (*p)++; skipSpace(p);
        if (**p != ',' && **p != ')') { scanTree(p, arr, cap, 2 * index + 1, maxIndex, count); skipSpace(p); }
        if (**p == ',') {
            (*p)++; skipSpace(p);
            if (**p != ')') { scanTree(p, arr, cap, 2 * index + 2, maxIndex, count); skipSpace(p); }
        }
        if (**p == ')') (*p)++;
    }
}

/* linked_bst.c 와 동일한 방식으로 실제 malloc 하여 n을 측정 */
static LinkedNode *buildLinked(const char **p) {
    int len = 0;
    const char *start = *p;
    skipSpace(p);
    start = *p;
    while (**p && **p != '(' && **p != ')' && **p != ',' && !isspace((unsigned char)**p)) {
        len++; (*p)++;
    }
    if (len == 0) return NULL;
    LinkedNode *node = malloc(sizeof(LinkedNode));
    memset(node, 0, sizeof(LinkedNode));
    int cplen = len < MAX_NAME - 1 ? len : MAX_NAME - 1;
    memcpy(node->name, start, cplen);
    node->name[cplen] = '\0';

    skipSpace(p);
    if (**p == '(') {
        (*p)++; skipSpace(p);
        if (**p != ',' && **p != ')') { node->left = buildLinked(p); skipSpace(p); }
        if (**p == ',') {
            (*p)++; skipSpace(p);
            if (**p != ')') { node->right = buildLinked(p); skipSpace(p); }
        }
        if (**p == ')') (*p)++;
    }
    return node;
}
static int countLinked(LinkedNode *n) {
    if (!n) return 0;
    return 1 + countLinked(n->left) + countLinked(n->right);
}
static void freeLinked(LinkedNode *n) {
    if (!n) return;
    freeLinked(n->left); freeLinked(n->right); free(n);
}

typedef struct {
    const char *label;
    const char *bracket;
} Sample;

int main(void) {
    Sample samples[] = {
        { "완전(포화) 이진트리", "A(B(D(H,I),E(J,K)),C(F(L,M),G(N,O)))" },
        { "일반 이진트리      ", "A(B(D(H,I),E(,J(,L(,N)))),C(F,G(,K(,M(,O)))))" },
        { "편향 이진트리      ", "P1(P2(P3(P4(P5(P6(P7(P8(P9(P10(P11(P12(P13(P14(P15))))))))))))))" },
    };
    int nsamples = 3;

    printf("N = 15 로 노드 수를 통일한 세 가지 이진트리에 대한 실제 메모리 측정\n");
    printf("sizeof(ArrayNode)  = %zu byte  (배열 한 칸: 이름 문자열)\n", sizeof(ArrayNode));
    printf("sizeof(LinkedNode) = %zu byte  (이름 + 왼쪽포인터 + 오른쪽포인터)\n", sizeof(LinkedNode));
    printf("[측정 범위] 두 값 모두 '트리 노드를 저장하는 데 실제로 요청한 할당 크기'만\n");
    printf("포함한다: 배열 쪽은 ArrayBtree.c와 동일하게 capacity만큼 calloc한 뒤 실제로\n");
    printf("채운 블록의 크기(capacity*sizeof(ArrayNode)), 연결 쪽은 노드마다 malloc한\n");
    printf("블록 n개의 합(n*sizeof(LinkedNode))이다. 입력 버퍼, 파싱 재귀 호출 스택,\n");
    printf("(연결 구현의) 순회용 큐, malloc/calloc 내부 관리 오버헤드(청크 헤더 등)는\n");
    printf("포함하지 않는다. 즉 '요청 바이트 수' 비교이며 OS 수준 RSS 실측이 아니다.\n\n");

    printf("%-22s%8s%10s%16s%18s%18s%10s\n",
           "트리 모양", "N", "높이", "배열capacity", "배열 메모리(B)", "연결 메모리(B)", "배열/연결");

    for (int i = 0; i < nsamples; i++) {
        /* 배열 구현: 1차 파싱으로 capacity 측정 -> ArrayBtree.c와 동일하게 실제 calloc 후 채움 */
        const char *p1 = samples[i].bracket;
        int maxIndex = -1, count = 0;
        scanTree(&p1, NULL, 0, 0, &maxIndex, &count);
        int capacity = maxIndex + 1;

        ArrayNode *arr = calloc(capacity, sizeof(ArrayNode));
        if (!arr) { fprintf(stderr, "오류: 메모리 할당 실패\n"); exit(1); }
        const char *p1b = samples[i].bracket;
        int maxIndex2 = -1, count2 = 0;
        scanTree(&p1b, arr, capacity, 0, &maxIndex2, &count2); /* 실제로 채움 */

        /* 연결 구현: 실제로 malloc 해서 n 측정 */
        const char *p2 = samples[i].bracket;
        LinkedNode *root = buildLinked(&p2);
        int n = countLinked(root);

        size_t arrayBytes = (size_t)capacity * sizeof(ArrayNode);
        size_t linkedBytes = (size_t)n * sizeof(LinkedNode);

        /* 높이(레벨, 루트=1) : capacity 로부터 역산 */
        int height = 0; { long v = maxIndex + 1; while (v > 0) { height++; v >>= 1; } }

        printf("%-22s%8d%10d%16d%18zu%18zu%9.1fx\n",
               samples[i].label, n, height, capacity, arrayBytes, linkedBytes,
               (double)arrayBytes / (double)linkedBytes);

        free(arr);
        freeLinked(root);
    }

    printf("\n[해석]\n");
    printf("- 완전(포화) 이진트리는 배열 capacity == N 이라 배열 표현이 가장 유리함.\n");
    printf("- 편향 이진트리는 인덱스가 2배씩 벌어져 capacity가 지수적으로 커지므로,\n");
    printf("  배열 표현이 연결 표현보다 압도적으로 많은 메모리를 요구함.\n");
    printf("- 일반 이진트리는 그 중간 정도의 낭비를 보임.\n");
    printf("- 연결 표현은 트리 모양과 무관하게 항상 N * sizeof(LinkedNode) 로 선형임.\n");
    return 0;
}