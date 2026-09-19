#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME 32
#define MAX_INPUT 65536

typedef struct Node {
    char name[MAX_NAME];
    struct Node *left, *right;
} Node;

static Node *root = NULL;

/* ---------------- 파싱 ---------------- */

static void skipSpace(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static Node *newNode(const char *name) {
    Node *n = malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "오류: 메모리 할당 실패\n"); exit(1); }
    strncpy(n->name, name, MAX_NAME - 1);
    n->name[MAX_NAME - 1] = '\0';
    n->left = n->right = NULL;
    return n;
}

static void freeTree(Node *n) {
    if (!n) return;
    freeTree(n->left);
    freeTree(n->right);
    free(n);
}

static Node *parseTree(const char **p) {
    char name[MAX_NAME];
    int len = 0;

    skipSpace(p);
    while (**p && **p != '(' && **p != ')' && **p != ',' &&
           !isspace((unsigned char)**p)) {
        if (len < MAX_NAME - 1) name[len++] = **p;
        (*p)++;
    }
    name[len] = '\0';

    if (len == 0) return NULL; /* 빈 자식 */

    Node *node = newNode(name);

    skipSpace(p);
    if (**p == '(') {
        (*p)++;
        skipSpace(p);
        if (**p != ',' && **p != ')') {
            node->left = parseTree(p);
            skipSpace(p);
        }
        if (**p == ',') {
            (*p)++;
            skipSpace(p);
            if (**p != ')') {
                node->right = parseTree(p);
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
    return node;
}

static void buildTreeFromInput(const char *input) {
    freeTree(root);
    const char *p = input;
    root = parseTree(&p);
}

/* ---------------- [1] 이진트리 출력 (왼쪽으로 눕힌 형태) ---------------- */
/* 전위 순회(preorder) 순서로 출력하며, 깊이만큼 들여쓰고 "+---"로 가지를 표시한다. */

static void printTree(Node *n, int depth) {
    if (!n) return;

    if (depth == 0) {
        printf("%s\n", n->name);
    } else {
        for (int j = 0; j < depth - 1; j++) printf("    ");
        printf("+---%s\n", n->name);
    }

    printTree(n->left, depth + 1);
    printTree(n->right, depth + 1);
}

/* ---------------- [2] 트리 정보 출력 ---------------- */

static int countNodes(Node *n) {
    if (!n) return 0;
    return 1 + countNodes(n->left) + countNodes(n->right);
}
static int countLeaf(Node *n) {
    if (!n) return 0;
    if (!n->left && !n->right) return 1;
    return countLeaf(n->left) + countLeaf(n->right);
}
static int treeHeight(Node *n) { /* 빈 트리 0, 단말 1 (루트 레벨=1 기준) */
    if (!n) return 0;
    int hl = treeHeight(n->left);
    int hr = treeHeight(n->right);
    return 1 + (hl > hr ? hl : hr);
}
static int treeDegree(Node *n) {
    if (!n) return 0;
    int d = (n->left != NULL) + (n->right != NULL);
    int dl = treeDegree(n->left);
    int dr = treeDegree(n->right);
    if (dl > d) d = dl;
    if (dr > d) d = dr;
    return d;
}

static void printTreeInfo(void) {
    int total = countNodes(root);
    int leaf = countLeaf(root);
    printf("1. 전체 노드의 수     : %d\n", total);
    printf("2. 단말 노드의 수     : %d\n", leaf);
    printf("3. 비단말 노드의 수   : %d\n", total - leaf);
    printf("4. 트리의 높이(height): %d\n", treeHeight(root));
    printf("5. 트리의 차수(degree): %d\n", treeDegree(root));
}

/* ---------------- [3] 이진트리 형태 판별 ---------------- */

static int isFull(Node *n) {
    if (!n) return 1;
    if (!n->left && !n->right) return 1;
    if (n->left && n->right)
        return treeHeight(n->left) == treeHeight(n->right) &&
               isFull(n->left) && isFull(n->right);
    return 0; /* 자식이 하나만 있음 */
}

/* 큐를 이용한 레벨 순회로 완전이진트리 판정 */
static int isComplete(Node *root_) {
    if (!root_) return 1;
    int qcap = 1024, qh = 0, qt = 0;
    Node **queue = malloc(sizeof(Node *) * qcap);
    int seenGap = 0;
    int ok = 1;

    queue[qt++] = root_;
    while (qh < qt) {
        if (qt + 2 >= qcap) {
            qcap *= 2;
            queue = realloc(queue, sizeof(Node *) * qcap);
        }
        Node *cur = queue[qh++];
        if (cur == NULL) {
            seenGap = 1;
        } else {
            if (seenGap) { ok = 0; break; }
            queue[qt++] = cur->left;
            queue[qt++] = cur->right;
        }
    }
    free(queue);
    return ok;
}

static int isSkewed(Node *n) {
    if (!n) return 1;
    if (n->left && n->right) return 0;
    return isSkewed(n->left) && isSkewed(n->right);
}

static void printShapeInfo(void) {
    if (!root) { printf("빈 트리입니다.\n"); return; }
    int total = countNodes(root);
    printf("완전 이진트리 여부 : %s\n", isComplete(root) ? "예" : "아니오");
    printf("포화 이진트리 여부 : %s\n", isFull(root) ? "예" : "아니오");
    printf("편향 이진트리 여부 : %s%s\n", isSkewed(root) ? "예" : "아니오",
           (total <= 1) ? "  (노드가 1개 이하라 편향/비편향 구분이 무의미함)" : "");
}

/* ---------------- [4] 노드 관계 조회 ---------------- */

static Node *findWithParent(Node *cur, Node *parent, const char *name, Node **outParent) {
    if (!cur) return NULL;
    if (strcmp(cur->name, name) == 0) { *outParent = parent; return cur; }
    Node *found = findWithParent(cur->left, cur, name, outParent);
    if (found) return found;
    return findWithParent(cur->right, cur, name, outParent);
}

static void printNodeRelation(void) {
    if (!root) { printf("빈 트리입니다.\n"); return; }

    char name[MAX_NAME];
    printf("조회할 노드 이름: ");
    if (scanf("%31s", name) != 1) return;

    Node *parent = NULL;
    Node *target = findWithParent(root, NULL, name, &parent);
    if (!target) {
        printf("'%s' 노드를 찾을 수 없습니다.\n", name);
        return;
    }

    printf("[%s] 자식 노드 : ", name);
    int hasChild = 0;
    if (target->left)  { printf("%s(왼쪽) ", target->left->name);  hasChild = 1; }
    if (target->right) { printf("%s(오른쪽) ", target->right->name); hasChild = 1; }
    if (!hasChild) printf("없음");
    printf("\n");

    if (!parent) {
        printf("[%s] 부모 노드 : 없음(루트)\n", name);
        printf("[%s] 형제 노드 : 없음\n", name);
    } else {
        printf("[%s] 부모 노드 : %s\n", name, parent->name);
        Node *sib = (parent->left == target) ? parent->right : parent->left;
        printf("[%s] 형제 노드 : %s\n", name, sib ? sib->name : "없음");
    }
}

/* ---------------- 메인 메뉴 ---------------- */

static void readInputTree(void) {
    static char input[MAX_INPUT];
    printf("이진트리를 괄호 표기법으로 입력하세요.\n");
    printf("예) A(B(D,E),C(,F))\n> ");
    if (scanf(" %65535[^\n]", input) != 1) input[0] = '\0';
    buildTreeFromInput(input);
    printf("트리 입력 완료 (노드 수: %d)\n", countNodes(root));
}

int main(void) {
    readInputTree();

    int sel;
    do {
        printf("\n========== [연결 자료구조 기반 이진트리] ==========\n");
        printf("1. 이진트리 출력\n");
        printf("2. 트리 정보 출력\n");
        printf("3. 이진트리 형태 판별\n");
        printf("4. 노드 관계 조회 (자식/부모/형제)\n");
        printf("5. 새 트리 입력\n");
        printf("0. 종료\n");
        printf("선택> ");
        if (scanf("%d", &sel) != 1) break;

        switch (sel) {
            case 1: printTree(root, 0); break;
            case 2: printTreeInfo(); break;
            case 3: printShapeInfo(); break;
            case 4: printNodeRelation(); break;
            case 5: readInputTree(); break;
            case 0: printf("종료합니다.\n"); break;
            default: printf("잘못된 선택입니다.\n");
        }
    } while (sel != 0);

    freeTree(root);
    return 0;
}