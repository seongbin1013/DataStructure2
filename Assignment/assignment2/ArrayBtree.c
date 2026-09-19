#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME 32     /* 노드 이름 최대 길이(끝문자 포함) */
#define MAX_INPUT 65536 /* 입력 문자열 최대 길이 */

/*
 * 배열 인덱싱(2*i+1, 2*i+2)은 트리가 깊어질수록 인덱스가 2배씩 커진다.
 * 별도의 상한이 없으면 편향 트리가 조금만 깊어져도(약 31단 이상)
 * int 오버플로(정의되지 않은 동작)와 calloc 크기 폭주로 이어진다.
 * 실제로 41단짜리 편향 트리로 재현/확인했다(UBSan: signed integer overflow).
 * 아래 상한은 그 사고를 막기 위한 실용적 안전장치이다.
 */
#define MAX_CAPACITY 1000000 /* 배열 표현이 다룰 수 있는 실용적 상한(약 2^20칸, 32MB 내외) */

typedef char NameArr[MAX_NAME];

static NameArr *tree = NULL; /* 배열 기반 이진트리 본체 */
static int capacity = 0;     /* 현재 할당된 배열 크기 */
static int nodeCount = 0;    /* 실제 노드 수 */

/* ---------------- 파싱 ---------------- */

static void skipSpace(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

/*
 * arr == NULL 이면 크기 측정(1차 파싱)만 수행하고 아무것도 저장하지 않는다.
 * arr != NULL 이면 실제로 이름을 배열에 채운다(2차 파싱).
 */
/* 이 인덱스보다 깊이 들어가면(2*index+1/2 계산 시) 오버플로나 용량 폭주가
 * 일어날 수 있으므로, 재귀로 더 내려가기 전에 미리 걸러낸다. */
static int tooDeepForArray(int index) {
    return index > (MAX_CAPACITY - 2) / 2;
}

static void parseTree(const char **p, NameArr *arr, int cap, int index,
                       int *maxIndex, int *count) {
    char name[MAX_NAME];
    int len = 0, rawLen = 0;

    skipSpace(p);
    while (**p && **p != '(' && **p != ')' && **p != ',' &&
           !isspace((unsigned char)**p)) {
        if (len < MAX_NAME - 1) name[len++] = **p;
        rawLen++;
        (*p)++;
    }
    name[len] = '\0';

    if (rawLen == 0) return; /* 이 자리에는 노드가 없음(빈 자식) */

    if (rawLen >= MAX_NAME) {
        fprintf(stderr, "오류: 노드 이름 '%s...'가 너무 깁니다(최대 %d자).\n",
                name, MAX_NAME - 1);
        exit(1);
    }

    if (index > *maxIndex) *maxIndex = index;
    (*count)++;

    if (arr) {
        if (index >= cap) {
            fprintf(stderr, "오류: 배열 용량(%d) 초과 (index=%d)\n", cap, index);
            exit(1);
        }
        strcpy(arr[index], name);
    }

    skipSpace(p);
    if (**p == '(') {
        (*p)++;
        skipSpace(p);
        if (**p != ',' && **p != ')') {
            if (tooDeepForArray(index)) {
                fprintf(stderr,
                        "오류: 트리가 너무 깊어 배열 표현의 실용적 상한(칸 수 %d)을 "
                        "초과합니다.\n", MAX_CAPACITY);
                exit(1);
            }
            parseTree(p, arr, cap, 2 * index + 1, maxIndex, count); /* 왼쪽 */
            skipSpace(p);
        }
        if (**p == ',') {
            (*p)++;
            skipSpace(p);
            if (**p != ')') {
                if (tooDeepForArray(index)) {
                    fprintf(stderr,
                            "오류: 트리가 너무 깊어 배열 표현의 실용적 상한(칸 수 %d)을 "
                            "초과합니다.\n", MAX_CAPACITY);
                    exit(1);
                }
                parseTree(p, arr, cap, 2 * index + 2, maxIndex, count); /* 오른쪽 */
                skipSpace(p);
            }
        }
        if (**p == ')') {
            (*p)++;
        } else {
            fprintf(stderr, "오류: ')' 가 필요합니다.\n");
            exit(1);
        }
    }
}

/* 입력 전체에서 트리 구문이 끝난 뒤 공백이 아닌 문자가 남아있는지 검사 */
static void checkTrailingGarbage(const char *p) {
    skipSpace(&p);
    if (*p != '\0') {
        fprintf(stderr, "오류: 트리 뒤에 불필요한 문자가 있습니다: \"%s\"\n", p);
        exit(1);
    }
}

static void buildTreeFromInput(const char *input) {
    const char *scan = input;
    skipSpace(&scan);
    int inputIsBlank = (*scan == '\0');

    const char *p1 = input;
    int maxIndex = -1, count = 0;
    parseTree(&p1, NULL, 0, 0, &maxIndex, &count); /* 1차: 크기 측정 */

    if (tree) { free(tree); tree = NULL; }

    if (count == 0) {
        if (!inputIsBlank) {
            /* 공백이 아닌데 유효한 루트 노드를 하나도 못 찾은 경우: 잘못된 입력 */
            fprintf(stderr, "오류: 유효한 루트 노드를 찾을 수 없습니다.\n");
            exit(1);
        }
        checkTrailingGarbage(p1);
        capacity = 0;
        nodeCount = 0;
        return;
    }
    checkTrailingGarbage(p1);

    capacity = maxIndex + 1;
    tree = calloc(capacity, sizeof(NameArr)); /* 0으로 초기화 -> name[0]=='\0' 이 빈 칸 */
    if (!tree) {
        fprintf(stderr, "오류: 메모리 할당 실패\n");
        exit(1);
    }

    const char *p2 = input;
    int maxIndex2 = -1, count2 = 0;
    parseTree(&p2, tree, capacity, 0, &maxIndex2, &count2); /* 2차: 실제로 채움 */
    nodeCount = count2;
}

/* ---------------- 기본 유틸 ---------------- */

static int isEmptyIdx(int idx) {
    return idx < 0 || idx >= capacity || tree[idx][0] == '\0';
}
static int leftIdx(int idx)  { return 2 * idx + 1; }
static int rightIdx(int idx) { return 2 * idx + 2; }
static int parentIdx(int idx){ return (idx - 1) / 2; }

/* ---------------- [1] 이진트리 출력 (왼쪽으로 눕힌 형태) ---------------- */
/* 전위 순회(preorder) 순서로 출력하며, 깊이만큼 들여쓰고 "+---"로 가지를 표시한다. */

static void printTree(int idx, int depth) {
    if (isEmptyIdx(idx)) return;

    if (depth == 0) {
        printf("%s\n", tree[idx]);
    } else {
        for (int j = 0; j < depth - 1; j++) printf("    ");
        printf("+---%s\n", tree[idx]);
    }

    printTree(leftIdx(idx), depth + 1);
    printTree(rightIdx(idx), depth + 1);
}

/* ---------------- [2] 트리 정보 출력 ---------------- */

static int levelOf(int idx) {
    /* 루트 레벨을 1로 정의: level = floor(log2(idx+1)) + 1 */
    int level = 1;
    long v = idx + 1;
    while (v > 1) { v >>= 1; level++; }
    return level;
}

static void printTreeInfo(void) {
    int leaf = 0, height = 0, degree = 0;
    for (int i = 0; i < capacity; i++) {
        if (isEmptyIdx(i)) continue;
        int hasL = !isEmptyIdx(leftIdx(i));
        int hasR = !isEmptyIdx(rightIdx(i));
        if (!hasL && !hasR) leaf++;
        int d = hasL + hasR;
        if (d > degree) degree = d;
        int lv = levelOf(i);
        if (lv > height) height = lv;
    }
    printf("1. 전체 노드의 수     : %d\n", nodeCount);
    printf("2. 단말 노드의 수     : %d\n", leaf);
    printf("3. 비단말 노드의 수   : %d\n", nodeCount - leaf);
    printf("4. 트리의 높이(height): %d\n", height);
    printf("5. 트리의 차수(degree): %d\n", degree);
}

/* ---------------- [3] 이진트리 형태 판별 ---------------- */

static void printShapeInfo(void) {
    if (nodeCount == 0) {
        printf("빈 트리입니다.\n");
        return;
    }

    int height = 0;
    for (int i = 0; i < capacity; i++) {
        if (!isEmptyIdx(i)) {
            int lv = levelOf(i);
            if (lv > height) height = lv;
        }
    }

    /* 완전 이진트리: 사용된 인덱스가 0..nodeCount-1 에 빈틈없이 채워져야 함 */
    int isComplete = (capacity == nodeCount);

    /* 포화 이진트리: 노드 수가 2^height - 1 이면 (그리고 완전이면) 포화 */
    int isFull = (nodeCount == (1 << height) - 1);

    /*
     * 편향 이진트리: "자식이 최대 1개인 사슬"이라는 느슨한 정의로는
     * A(B(,C)) 같은 지그재그(왼쪽-오른쪽 번갈아)도 편향으로 잘못 판정된다.
     * 여러 교재에서 쓰는 좌편향/우편향 정의(모든 노드가 전부 왼쪽으로만,
     * 또는 전부 오른쪽으로만 자식을 가짐)를 따라 더 엄격하게 판정한다.
     * (Report.md 5장에 이 선택 근거를 별도로 문서화함 — 수업에서 느슨한
     *  정의를 쓴다면 결과가 달라질 수 있다.)
     */
    int allLeftChain = 1, allRightChain = 1;
    for (int i = 0; i < capacity; i++) {
        if (isEmptyIdx(i)) continue;
        int hasL = !isEmptyIdx(leftIdx(i));
        int hasR = !isEmptyIdx(rightIdx(i));
        if (hasL && hasR) { allLeftChain = 0; allRightChain = 0; }
        else if (hasL)    { allRightChain = 0; }
        else if (hasR)    { allLeftChain = 0; }
    }
    int isSkewed = allLeftChain || allRightChain; /* 노드 0~1개면 둘 다 참으로 남아 자명하게 예 */

    printf("완전 이진트리 여부 : %s\n", isComplete ? "예" : "아니오");
    printf("포화 이진트리 여부 : %s\n", isFull ? "예" : "아니오");
    printf("편향 이진트리 여부 : %s%s\n", isSkewed ? "예" : "아니오",
           (nodeCount <= 1) ? "  (노드가 1개 이하라 편향/비편향 구분이 무의미함)" : "");
}

/* ---------------- [4] 노드 관계 조회 ---------------- */

/*
 * 이름으로 노드를 찾을 때 배열 전체(0..capacity-1)를 인덱스 순으로 훑으면
 * 편향 트리처럼 capacity가 부풀어 있는 경우 실제 노드 수(n)보다 훨씬 많은
 * 칸을 검사하게 되어 O(capacity)가 된다. 실제 존재하는 자식 링크만 따라
 * 내려가는 전위 순회(preorder)로 찾으면 실제 방문 칸 수가 n을 넘지 않아
 * O(n)이 되고, 연결 구현(LinkedBtree.c)의 탐색 순서와도 정확히 일치한다
 * (동일 이름이 여러 개인 경우에도 두 구현이 같은 노드를 찾게 됨).
 */
static int findIdxPreorder(int idx, const char *name) {
    if (isEmptyIdx(idx)) return -1;
    if (strcmp(tree[idx], name) == 0) return idx;
    int found = findIdxPreorder(leftIdx(idx), name);
    if (found != -1) return found;
    return findIdxPreorder(rightIdx(idx), name);
}

static void printNodeRelation(void) {
    if (nodeCount == 0) { printf("빈 트리입니다.\n"); return; }

    char name[MAX_NAME];
    printf("조회할 노드 이름: ");
    if (scanf("%31s", name) != 1) return;

    int idx = findIdxPreorder(0, name);
    if (idx == -1) {
        printf("'%s' 노드를 찾을 수 없습니다.\n", name);
        return;
    }

    printf("[%s] 자식 노드 : ", name);
    int hasChild = 0;
    if (!isEmptyIdx(leftIdx(idx)))  { printf("%s(왼쪽) ", tree[leftIdx(idx)]);  hasChild = 1; }
    if (!isEmptyIdx(rightIdx(idx))) { printf("%s(오른쪽) ", tree[rightIdx(idx)]); hasChild = 1; }
    if (!hasChild) printf("없음");
    printf("\n");

    if (idx == 0) {
        printf("[%s] 부모 노드 : 없음(루트)\n", name);
        printf("[%s] 형제 노드 : 없음\n", name);
    } else {
        int p = parentIdx(idx);
        printf("[%s] 부모 노드 : %s\n", name, tree[p]);

        int sib = (idx % 2 == 1) ? idx + 1 : idx - 1; /* 홀수=왼쪽자식->형제는 오른쪽, 짝수=오른쪽자식->형제는 왼쪽 */
        printf("[%s] 형제 노드 : %s\n", name, isEmptyIdx(sib) ? "없음" : tree[sib]);
    }
}

/* ---------------- 메인 메뉴 ---------------- */

static void readInputTree(void) {
    static char input[MAX_INPUT];
    printf("이진트리를 괄호 표기법으로 입력하세요.\n");
    printf("예) A(B(D,E),C(,F))\n> ");
    if (scanf(" %65535[^\n]", input) != 1) input[0] = '\0';
    buildTreeFromInput(input);
    printf("트리 입력 완료 (노드 수: %d)\n", nodeCount);
}

int main(void) {
    readInputTree();

    int sel;
    do {
        printf("\n========== [배열 기반 이진트리] ==========\n");
        printf("1. 이진트리 출력\n");
        printf("2. 트리 정보 출력\n");
        printf("3. 이진트리 형태 판별\n");
        printf("4. 노드 관계 조회 (자식/부모/형제)\n");
        printf("5. 새 트리 입력\n");
        printf("0. 종료\n");
        printf("선택> ");
        if (scanf("%d", &sel) != 1) break;

        switch (sel) {
            case 1: printTree(0, 0); break;
            case 2: printTreeInfo(); break;
            case 3: printShapeInfo(); break;
            case 4: printNodeRelation(); break;
            case 5: readInputTree(); break;
            case 0: printf("종료합니다.\n"); break;
            default: printf("잘못된 선택입니다.\n");
        }
    } while (sel != 0);

    free(tree);
    return 0;
}