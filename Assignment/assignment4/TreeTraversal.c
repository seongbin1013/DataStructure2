#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE  4096
#define MAX_STACK 1024

typedef struct Node {
    char data;
    struct Node *left;
    struct Node *right;
} Node;

/* ---------------- 스택 (포인터 기반, 배열 구현) : Node* 전용, 순회/메모리 해제에서 공용 사용 ---------------- */

typedef struct {
    Node *data[MAX_STACK];
    int top;
} Stack;

static void stackInit(Stack *s) { s->top = -1; }
static int  stackIsEmpty(Stack *s) { return s->top == -1; }

static void stackPush(Stack *s, Node *n) {
    if (s->top >= MAX_STACK - 1) {
        fprintf(stderr, "스택 오버플로우\n");
        exit(1);
    }
    s->data[++(s->top)] = n;
}

static Node *stackPop(Stack *s) {
    return s->data[(s->top)--];
}

/* ---------------- 메모리 해제 (반복적) ---------------- */

/* 재귀 없이 스택으로 모든 노드를 방문하며 해제한다. 방문 순서는 상관없다. */
static void freeTree(Node *root) {
    Stack s;
    Node *cur;

    if (root == NULL) return;

    stackInit(&s);
    stackPush(&s, root);

    while (!stackIsEmpty(&s)) {
        cur = stackPop(&s);
        if (cur->left  != NULL) stackPush(&s, cur->left);
        if (cur->right != NULL) stackPush(&s, cur->right);
        free(cur);
    }
}

/* ---------------- 괄호 표기법 파서 (반복적) ---------------- */

static const char *g_input;
static int g_pos;
static int g_error;

static Node *createNode(char data) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        exit(1);
    }
    n->data = data;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static void skipSpaces(void) {
    while (g_input[g_pos] == ' ' || g_input[g_pos] == '\t')
        g_pos++;
}

/* 파싱 진행 상태를 담는 프레임. 재귀 호출 대신 이 프레임들을 스택에 쌓아
   "왼쪽 자식을 기다리는 중 / 오른쪽 자식을 기다리는 중" 상태를 관리한다. */
typedef struct {
    Node *node;
    int state; /* 0 = 왼쪽 자식 파싱 대기, 1 = 오른쪽 자식 파싱 대기 */
} ParseFrame;

typedef struct {
    ParseFrame data[MAX_STACK];
    int top;
} ParseStack;

static void parseStackInit(ParseStack *s) { s->top = -1; }
static int  parseStackIsEmpty(ParseStack *s) { return s->top == -1; }

static void parseStackPush(ParseStack *s, ParseFrame f) {
    if (s->top >= MAX_STACK - 1) {
        fprintf(stderr, "스택 오버플로우\n");
        exit(1);
    }
    s->data[++(s->top)] = f;
}

static void parseStackPop(ParseStack *s) { (s->top)--; }
static ParseFrame *parseStackTop(ParseStack *s) { return &s->data[s->top]; }

/* 괄호 표기법 전체를 반복적으로 파싱한다.
   "(" 데이터 "(" 왼쪽서브트리 ")" "(" 오른쪽서브트리 ")" ")" 형태를
   재귀 호출 없이 명시적 스택(ParseStack)으로 시뮬레이션한다. */
static Node *parseTreeIterative(void) {
    ParseStack ps;
    Node *completed = NULL;   /* 방금 완성된(또는 빈) 서브트리 */
    int expectGroupStart = 1; /* 1: "(" 로 시작하는 새 그룹을 읽어야 함 / 0: completed 를 부모에 붙일 차례 */

    parseStackInit(&ps);

    for (;;) {
        if (g_error) {
            /* 스택에 남아있는 프레임들은 서로 아직 연결되지 않은 상태이므로
               (자식이 완성되어 pop 되어야 부모에 붙는다) 각 프레임의 노드를
               모두 개별적으로 해제해야 한다. */
            int i;
            for (i = 0; i <= ps.top; i++) freeTree(ps.data[i].node);
            return NULL;
        }

        if (expectGroupStart) {
            skipSpaces();
            if (g_input[g_pos] != '(') {
                g_error = 1;
                continue;
            }
            g_pos++; /* '(' 소비 */
            skipSpaces();

            if (g_input[g_pos] == ')') {
                /* 빈 서브트리 "()" */
                g_pos++;
                completed = NULL;
                expectGroupStart = 0;
                continue;
            }

            if (g_input[g_pos] == '\0' || g_input[g_pos] == '(') {
                g_error = 1;
                continue;
            }

            {
                Node *node = createNode(g_input[g_pos]);
                ParseFrame f;
                g_pos++;
                f.node = node;
                f.state = 0; /* 이제 왼쪽 자식을 파싱해야 함 */
                parseStackPush(&ps, f);
                expectGroupStart = 1; /* 왼쪽 자식 그룹 "(" 을 계속 파싱 */
            }
            continue;
        }

        /* expectGroupStart == 0 인 상태: completed 를 부모에 붙이거나, 최종 루트로 반환 */
        if (parseStackIsEmpty(&ps)) {
            return completed; /* 최상위 트리 완성 (빈 트리인 경우 NULL) */
        }

        {
            ParseFrame *top = parseStackTop(&ps);
            if (top->state == 0) {
                top->node->left = completed;
                top->state = 1;
                expectGroupStart = 1; /* 오른쪽 자식 그룹을 파싱 */
            } else {
                Node *finishedNode;
                top->node->right = completed;

                skipSpaces();
                if (g_input[g_pos] != ')') {
                    g_error = 1;
                    continue;
                }
                g_pos++; /* ')' 소비 */

                finishedNode = top->node;
                parseStackPop(&ps);
                completed = finishedNode;
                expectGroupStart = 0; /* completed 를 다시 위 프레임에 붙이거나 루트로 반환 */
            }
        }
    }
}

/* 문자열 전체를 파싱. 성공 시 루트 반환, 실패 시 NULL 반환 및 error 표시 */
static Node *buildTree(const char *str, int *ok) {
    Node *root;
    g_input = str;
    g_pos = 0;
    g_error = 0;

    root = parseTreeIterative();

    if (g_error) {
        *ok = 0;
        return NULL; /* 실패 시 이미 parseTreeIterative 내부에서 해제됨 */
    }

    skipSpaces();
    if (g_input[g_pos] != '\0') {
        freeTree(root); /* 트리는 올바르지만 뒤에 불필요한 문자가 남은 경우 해제 */
        *ok = 0;
        return NULL;
    }

    *ok = 1;
    return root;
}

/* ---------------- 반복적(iterative) 순회 ---------------- */

/* 전위 순회 : Root -> Left -> Right (재귀 미사용, 스택 1개) */
void preorder(Node *root) {
    Stack s;
    Node *cur;
    int first = 1;

    if (root == NULL) return;

    stackInit(&s);
    stackPush(&s, root);

    while (!stackIsEmpty(&s)) {
        cur = stackPop(&s);
        if (!first) printf(" ");
        printf("%c", cur->data);
        first = 0;

        /* 오른쪽을 먼저 push 해야 왼쪽이 먼저 pop 된다 */
        if (cur->right != NULL) stackPush(&s, cur->right);
        if (cur->left  != NULL) stackPush(&s, cur->left);
    }
    printf("\n");
}

/* 중위 순회 : Left -> Root -> Right (재귀 미사용, 스택 1개) */
void inorder(Node *root) {
    Stack s;
    Node *cur = root;
    int first = 1;

    stackInit(&s);

    while (cur != NULL || !stackIsEmpty(&s)) {
        while (cur != NULL) {
            stackPush(&s, cur);
            cur = cur->left;
        }
        cur = stackPop(&s);
        if (!first) printf(" ");
        printf("%c", cur->data);
        first = 0;
        cur = cur->right;
    }
    printf("\n");
}

/* 후위 순회 : Left -> Right -> Root (재귀 미사용, 스택 2개) */
void postorder(Node *root) {
    Stack s1, s2;
    Node *cur;
    int first = 1;

    if (root == NULL) return;

    stackInit(&s1);
    stackInit(&s2);
    stackPush(&s1, root);

    /* s1 에서 (Root, Right, Left) 순서로 꺼내 s2 에 쌓으면
       s2 를 전부 pop 했을 때 (Left, Right, Root) 순서가 된다 */
    while (!stackIsEmpty(&s1)) {
        cur = stackPop(&s1);
        stackPush(&s2, cur);
        if (cur->left  != NULL) stackPush(&s1, cur->left);
        if (cur->right != NULL) stackPush(&s1, cur->right);
    }

    while (!stackIsEmpty(&s2)) {
        cur = stackPop(&s2);
        if (!first) printf(" ");
        printf("%c", cur->data);
        first = 0;
    }
    printf("\n");
}


#define MAX_PREFIX 256

/* 각 노드를 출력할 때 필요한 정보를 담는 스택 프레임.
   prefix : 이 노드 줄 앞에 붙는 들여쓰기+연결선 문자열
   isLeft : 부모의 왼쪽 자식이면 1, 오른쪽 자식이면 0
   isRoot : 루트 노드이면 1 (분기 기호 없이 그대로 출력) */
typedef struct {
    Node *node;
    char prefix[MAX_PREFIX];
    int isLeft;
    int isRoot;
} PrintFrame;

typedef struct {
    PrintFrame data[MAX_STACK];
    int top;
} PrintStack;

static void printStackInit(PrintStack *s) { s->top = -1; }
static int  printStackIsEmpty(PrintStack *s) { return s->top == -1; }

static void printStackPush(PrintStack *s, Node *n, const char *prefix, int isLeft, int isRoot) {
    if (s->top >= MAX_STACK - 1) {
        fprintf(stderr, "스택 오버플로우\n");
        exit(1);
    }
    s->top++;
    s->data[s->top].node = n;
    strncpy(s->data[s->top].prefix, prefix, MAX_PREFIX - 1);
    s->data[s->top].prefix[MAX_PREFIX - 1] = '\0';
    s->data[s->top].isLeft = isLeft;
    s->data[s->top].isRoot = isRoot;
}

static PrintFrame printStackPop(PrintStack *s) {
    return s->data[(s->top)--];
}

/* dst 뒤에 suffix 를 안전하게 이어붙인다 (MAX_PREFIX 범위 내로 자름) */
static void appendPrefix(char *dst, const char *suffix) {
    size_t len = strlen(dst);
    if (len < (size_t)MAX_PREFIX - 1) {
        strncat(dst, suffix, (size_t)MAX_PREFIX - 1 - len);
    }
}

/* 트리를 옆으로 눕힌 모양으로 출력한다: 오른쪽 서브트리가 위쪽, 왼쪽 서브트리가
   아래쪽에 오도록 하고, 부모-자식 관계를 "│", "┌──", "└──" 선으로 이어 보여준다.
   (오른쪽 전체 -> 노드 -> 왼쪽 전체 순서로 방문하는 것은 기존과 동일,
   각 단계마다 접두(prefix) 문자열을 함께 스택에 쌓아 재귀 없이 구현) */
static void printStructure(Node *root) {
    PrintStack s;
    Node *cur = root;
    char curPrefix[MAX_PREFIX];
    int curIsLeft = 1;
    int curIsRoot = 1;

    if (root == NULL) return;

    printStackInit(&s);
    curPrefix[0] = '\0';

    while (cur != NULL || !printStackIsEmpty(&s)) {
        while (cur != NULL) {
            char nextPrefix[MAX_PREFIX];

            printStackPush(&s, cur, curPrefix, curIsLeft, curIsRoot);

            strncpy(nextPrefix, curPrefix, MAX_PREFIX - 1);
            nextPrefix[MAX_PREFIX - 1] = '\0';
            if (curIsRoot) {
                appendPrefix(nextPrefix, "    ");
            } else if (curIsLeft) {
                appendPrefix(nextPrefix, "│   ");
            } else {
                appendPrefix(nextPrefix, "    ");
            }

            cur = cur->right;
            strncpy(curPrefix, nextPrefix, MAX_PREFIX - 1);
            curPrefix[MAX_PREFIX - 1] = '\0';
            curIsLeft = 0;
            curIsRoot = 0;
        }

        {
            PrintFrame f = printStackPop(&s);
            char nextPrefixLeft[MAX_PREFIX];

            if (f.isRoot) {
                printf("%s%c\n", f.prefix, f.node->data);
            } else if (f.isLeft) {
                printf("%s└── %c\n", f.prefix, f.node->data);
            } else {
                printf("%s┌── %c\n", f.prefix, f.node->data);
            }

            strncpy(nextPrefixLeft, f.prefix, MAX_PREFIX - 1);
            nextPrefixLeft[MAX_PREFIX - 1] = '\0';
            if (f.isRoot) {
                appendPrefix(nextPrefixLeft, "    ");
            } else if (f.isLeft) {
                appendPrefix(nextPrefixLeft, "    ");
            } else {
                appendPrefix(nextPrefixLeft, "│   ");
            }

            cur = f.node->left;
            strncpy(curPrefix, nextPrefixLeft, MAX_PREFIX - 1);
            curPrefix[MAX_PREFIX - 1] = '\0';
            curIsLeft = 1;
            curIsRoot = 0;
        }
    }
}

/* ---------------- main ---------------- */

int main(void) {
    char line[MAX_LINE];
    Node *root;
    int ok;

    printf("괄호 표기법으로 이진트리를 입력하세요.\n");
    printf("예) (A(B(D()())(E()()))(C()(F()())))\n> ");

    if (fgets(line, sizeof(line), stdin) == NULL) {
        fprintf(stderr, "입력을 읽을 수 없습니다.\n");
        return 1;
    }
    /* 개행 문자 제거 */
    line[strcspn(line, "\r\n")] = '\0';

    root = buildTree(line, &ok);
    if (!ok) {
        printf("\n오류: 올바르지 않은 괄호 표기법입니다. 입력을 확인하세요.\n");
        return 1;
    }
    if (root == NULL) {
        printf("\n빈 트리가 입력되었습니다.\n");
        return 0;
    }

    printf("\n[입력된 이진트리 구조]\n");
    printStructure(root);

    printf("\n[순회 결과]\n");
    printf("Preorder  : ");
    preorder(root);
    printf("Inorder   : ");
    inorder(root);
    printf("Postorder : ");
    postorder(root);

    freeTree(root);
    return 0;
}