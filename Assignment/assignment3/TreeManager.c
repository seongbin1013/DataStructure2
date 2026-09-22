#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE       256
#define MAX_PATH_DEPTH 100
#define MAX_NODES      1000   /* create_btree(size)에 대응하는 기본 용량 */

typedef struct Node {
    char data;
    struct Node *left;
    struct Node *right;
} Node;

typedef struct {
    Node *root;
    int max_size;
    int count;
} BTree;

/* 경로 탐색 결과: 노드 + 그 부모 + 어느 쪽 자식인지 */
typedef struct {
    Node *node;
    Node *parent;
    char side;      /* 'L', 'R', 또는 부모가 없으면 'T'(root) */
} Locate;

/* ================================================================
 * 이진트리 ADT 연산
 * ================================================================ */

BTree *create_btree(int size) {
    if (size <= 0) return NULL;

    BTree *tree = (BTree *)malloc(sizeof(BTree));
    if (tree == NULL) return NULL;

    tree->root = NULL;
    tree->max_size = size;
    tree->count = 0;
    return tree;
}

static Node *make_node(char value) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (n == NULL) return NULL;

    n->data = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static int is_leaf(Node *n) {
    return n->left == NULL && n->right == NULL;
}

/* 빈 트리에만 루트 생성 가능 */
int insert_root(BTree *tree, char value) {
    if (tree == NULL || tree->root != NULL) return 0;
    if (tree->count >= tree->max_size) return 0;

    Node *new_node = make_node(value);
    if (new_node == NULL) return 0;

    tree->root = new_node;
    tree->count++;
    return 1;
}

/* parent 아래 child(L/R) 위치에 value 삽입
 * - parent는 자식이 2개 미만이어야 함 (자식이 이미 2개면 실패)
 * - 지정한 위치가 이미 차 있으면 실패
 * - 형제(반대쪽 자식)와 데이터가 같으면 실패 */
int insert_child(BTree *tree, Node *parent, char child, char value) {
    if (tree == NULL || parent == NULL) return 0;
    if (parent->left != NULL && parent->right != NULL) return 0;
    if (tree->count >= tree->max_size) return 0;

    Node *new_node = NULL;

    if (child == 'L') {
        if (parent->left != NULL) return 0;
        if (parent->right != NULL && parent->right->data == value) return 0;

        new_node = make_node(value);
        if (new_node == NULL) return 0;
        parent->left = new_node;
    } else if (child == 'R') {
        if (parent->right != NULL) return 0;
        if (parent->left != NULL && parent->left->data == value) return 0;

        new_node = make_node(value);
        if (new_node == NULL) return 0;
        parent->right = new_node;
    } else {
        return 0;
    }

    tree->count++;
    return 1;
}

/* 단말 노드만 삭제 가능 */
int delete_node(BTree *tree, Locate loc) {
    if (loc.node == NULL) return 0;
    if (!is_leaf(loc.node)) return 0;

    if (loc.parent == NULL) {
        free(loc.node);
        tree->root = NULL;
    } else if (loc.side == 'L') {
        free(loc.node);
        loc.parent->left = NULL;
    } else {
        free(loc.node);
        loc.parent->right = NULL;
    }
    tree->count--;
    return 1;
}

/* 노드 데이터 수정. 수정 후 형제와 데이터가 같아지면 실패 */
int update_value(Locate loc, char value) {
    if (loc.node == NULL) return 0;
    if (loc.parent != NULL) {
        Node *sibling = (loc.side == 'L') ? loc.parent->right : loc.parent->left;
        if (sibling != NULL && sibling->data == value) return 0;
    }
    loc.node->data = value;
    return 1;
}

/* parent의 좌/우 자식 정보 조회 */
void read_child(Node *parent, char *l_data, int *has_l, char *r_data, int *has_r) {
    *has_l = (parent->left != NULL);
    if (*has_l) *l_data = parent->left->data;
    *has_r = (parent->right != NULL);
    if (*has_r) *r_data = parent->right->data;
}

static void print_node(Node *node, int depth) {
    if (node == NULL) return;
    if (depth == 0) {
        printf("%c\n", node->data);
    } else {
        for (int i = 0; i < (depth - 1) * 4; i++) putchar(' ');
        printf("+---%c\n", node->data);
    }
    print_node(node->left, depth + 1);
    print_node(node->right, depth + 1);
}

void print_btree(BTree *tree) {
    if (tree->root == NULL) {
        printf("빈 트리입니다.\n");
        return;
    }
    print_node(tree->root, 0);
}

static void free_subtree(Node *node) {
    if (node == NULL) return;
    free_subtree(node->left);
    free_subtree(node->right);
    free(node);
}

void destroy_btree(BTree *tree) {
    if (tree == NULL) return;
    free_subtree(tree->root);
    tree->root = NULL;
    free(tree);
}

/* ================================================================
 * 경로(path) 파싱 및 탐색
 * ================================================================ */

/* "/A/B/C" -> letters = {'A','B','C'}, len = 3
 * 형식이 잘못되면(선행 '/' 없음, 빈 세그먼트, 대문자 1글자가 아닌 세그먼트) 0 반환 */
int parse_path(const char *path, char letters[], int *len) {
    if (path == NULL || letters == NULL || len == NULL) return 0;
    if (path[0] != '/') return 0;

    const char *p = path + 1;
    int n = 0;

    /* "/"만 있거나 마지막이 '/'인 경로는 허용하지 않음 */
    if (*p == '\0') return 0;

    while (*p != '\0') {
        /* 각 경로 세그먼트는 반드시 영문 대문자 한 글자 */
        if (!isupper((unsigned char)*p)) return 0;
        if (n >= MAX_PATH_DEPTH) return 0;
        letters[n++] = *p;
        p++;

        if (*p == '\0') break;

        /* 다음 글자는 반드시 '/'이어야 함 */
        if (*p != '/') return 0;
        p++;

        /* 연속된 '/' 또는 마지막 '/'는 빈 세그먼트이므로 오류 */
        if (*p == '\0' || *p == '/') return 0;
    }

    *len = n;
    return 1;
}

/* letters/len 이 나타내는 경로의 노드를 찾고, 그 부모/방향까지 함께 반환 */
Locate locate_by_path(BTree *tree, char letters[], int len) {
    Locate loc = { NULL, NULL, ' ' };
    if (tree->root == NULL || tree->root->data != letters[0]) return loc;

    Node *cur = tree->root;
    Node *parent = NULL;
    char side = 'T';

    for (int i = 1; i < len; i++) {
        if (cur->left != NULL && cur->left->data == letters[i]) {
            parent = cur; cur = cur->left; side = 'L';
        } else if (cur->right != NULL && cur->right->data == letters[i]) {
            parent = cur; cur = cur->right; side = 'R';
        } else {
            return loc; /* 경로 상 노드를 찾지 못함 -> node는 NULL 상태 유지 */
        }
    }
    loc.node = cur;
    loc.parent = parent;
    loc.side = side;
    return loc;
}

/* ================================================================
 * 명령어 처리
 * ================================================================ */

static int is_upper_letter_token(const char *s) {
    return s != NULL && strlen(s) == 1 && isupper((unsigned char)s[0]);
}

static int parse_child_token(const char *s, char *child) {
    if (strcmp(s, "L") == 0 || strcmp(s, "Left") == 0) {
        *child = 'L';
        return 1;
    }
    if (strcmp(s, "R") == 0 || strcmp(s, "Right") == 0) {
        *child = 'R';
        return 1;
    }
    return 0;
}

void cmd_insert(BTree *tree, char *tokens[], int n) {
    int nargs = n - 1;

    /* Insert / A  : 루트 생성 */
    if (nargs == 2 && strcmp(tokens[1], "/") == 0) {
        if (!is_upper_letter_token(tokens[2])) {
            printf("오류: 데이터는 영문 대문자 한 글자여야 함.\n");
            return;
        }
        if (!insert_root(tree, tokens[2][0])) {
            printf("오류: 루트 노드를 생성할 수 없음 (이미 트리가 존재함).\n");
            return;
        }
        printf("루트 노드 %c 생성 완료.\n", tokens[2][0]);
        return;
    }

    /* Insert parent-node child new-data */
    if (nargs != 3) {
        printf("오류: Insert 명령의 인자 개수가 올바르지 않음.\n");
        return;
    }

    char letters[MAX_PATH_DEPTH];
    int len;
    if (!parse_path(tokens[1], letters, &len)) {
        printf("오류: 잘못된 경로 형식임.\n");
        return;
    }

    Locate loc = locate_by_path(tree, letters, len);
    if (loc.node == NULL) {
        printf("오류: 지정한 부모 노드가 존재하지 않음.\n");
        return;
    }

    char child;
    if (!parse_child_token(tokens[2], &child)) {
        printf("오류: child는 L/Left 또는 R/Right이어야 함.\n");
        return;
    }

    if (!is_upper_letter_token(tokens[3])) {
        printf("오류: 데이터는 영문 대문자 한 글자여야 함.\n");
        return;
    }
    char value = tokens[3][0];

    if (loc.node->left != NULL && loc.node->right != NULL) {
        printf("오류: 부모 노드에 이미 자식이 2개 있음.\n");
        return;
    }

    if (!insert_child(tree, loc.node, child, value)) {
        printf("오류: 노드를 추가할 수 없음 (자리 중복 또는 형제와 동일 데이터).\n");
        return;
    }

    printf("%s 아래 %c(%c) 추가 완료.\n", tokens[1], value, child);
}

void cmd_delete(BTree *tree, char *tokens[], int n) {
    if (n - 1 != 1) {
        printf("오류: Delete 명령의 인자 개수가 올바르지 않음.\n");
        return;
    }

    char letters[MAX_PATH_DEPTH];
    int len;
    if (!parse_path(tokens[1], letters, &len)) {
        printf("오류: 잘못된 경로 형식임.\n");
        return;
    }

    Locate loc = locate_by_path(tree, letters, len);
    if (loc.node == NULL) {
        printf("오류: 지정한 노드가 존재하지 않음.\n");
        return;
    }
    if (!is_leaf(loc.node)) {
        printf("오류: 단말 노드만 삭제할 수 있음.\n");
        return;
    }

    char deleted = loc.node->data;
    delete_node(tree, loc);
    printf("%c 노드 삭제 완료.\n", deleted);
}

void cmd_update(BTree *tree, char *tokens[], int n) {
    if (n - 1 != 2) {
        printf("오류: Update 명령의 인자 개수가 올바르지 않음.\n");
        return;
    }

    char letters[MAX_PATH_DEPTH];
    int len;
    if (!parse_path(tokens[1], letters, &len)) {
        printf("오류: 잘못된 경로 형식임.\n");
        return;
    }

    Locate loc = locate_by_path(tree, letters, len);
    if (loc.node == NULL) {
        printf("오류: 지정한 노드가 존재하지 않음.\n");
        return;
    }

    if (!is_upper_letter_token(tokens[2])) {
        printf("오류: 데이터는 영문 대문자 한 글자여야 함.\n");
        return;
    }
    char value = tokens[2][0];
    char old = letters[len - 1];

    if (!update_value(loc, value)) {
        printf("오류: 형제 노드와 동일한 데이터를 가질 수 없음.\n");
        return;
    }
    printf("%c -> %c 수정 완료.\n", old, value);
}

void cmd_read(BTree *tree, char *tokens[], int n) {
    if (n - 1 != 1) {
        printf("오류: Read 명령의 인자 개수가 올바르지 않음.\n");
        return;
    }

    char letters[MAX_PATH_DEPTH];
    int len;
    if (!parse_path(tokens[1], letters, &len)) {
        printf("오류: 잘못된 경로 형식임.\n");
        return;
    }

    Locate loc = locate_by_path(tree, letters, len);
    if (loc.node == NULL) {
        printf("오류: 지정한 노드가 존재하지 않음.\n");
        return;
    }

    char l_data = 0, r_data = 0;
    int has_l = 0, has_r = 0;
    read_child(loc.node, &l_data, &has_l, &r_data, &has_r);

    if (!has_l && !has_r) {
        printf("자식 노드가 없음.\n");
    } else if (has_l && has_r) {
        printf("%c(L), %c(R)\n", l_data, r_data);
    } else if (has_l) {
        printf("%c(L)\n", l_data);
    } else {
        printf("%c(R)\n", r_data);
    }
}

void cmd_print(BTree *tree, char *tokens[], int n) {
    (void)tokens;
    if (n - 1 != 0) {
        printf("오류: Print 명령은 추가 인자를 받지 않음.\n");
        return;
    }
    print_btree(tree);
}

void process_command(BTree *tree, char *line) {
    char buf[MAX_LINE];
    strncpy(buf, line, MAX_LINE - 1);
    buf[MAX_LINE - 1] = '\0';

    char *tokens[10];
    int n = 0;
    char *tok = strtok(buf, " \t");

    while (tok != NULL) {
        if (n >= 10) {
            printf("오류: 명령의 인자가 너무 많음.\n");
            return;
        }
        tokens[n++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (n == 0) return;

    if (strcmp(tokens[0], "I") == 0 || strcmp(tokens[0], "Insert") == 0) {
        cmd_insert(tree, tokens, n);
    } else if (strcmp(tokens[0], "D") == 0 || strcmp(tokens[0], "Delete") == 0) {
        cmd_delete(tree, tokens, n);
    } else if (strcmp(tokens[0], "U") == 0 || strcmp(tokens[0], "Update") == 0) {
        cmd_update(tree, tokens, n);
    } else if (strcmp(tokens[0], "R") == 0 || strcmp(tokens[0], "Read") == 0) {
        cmd_read(tree, tokens, n);
    } else if (strcmp(tokens[0], "P") == 0 || strcmp(tokens[0], "Print") == 0) {
        cmd_print(tree, tokens, n);
    } else {
        printf("오류: 알 수 없는 명령어임.\n");
    }
}

int main(void) {
    BTree *tree = create_btree(MAX_NODES);
    if (tree == NULL) {
        fprintf(stderr, "오류: 트리 생성에 실패하여 프로그램을 종료함.\n");
        return 1;
    }

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), stdin)) {
        /* '\n' 뿐 아니라 Windows 스타일 '\r\n'의 '\r'도 함께 제거 */
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
        process_command(tree, line);
    }

    destroy_btree(tree);
    return 0;
}