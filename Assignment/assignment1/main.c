#include <stdio.h>
#include <string.h>

#define MAX_NODES 26
#define MAX_INPUT 1024

char input[MAX_INPUT];
int pos = 0;
int len = 0;

/* 노드 스택 */
char nodeStack[MAX_NODES];

/* 각 노드의 자식 수를 저장하는 카운터 스택 */
int childCntStack[MAX_NODES];

int top = -1;

/* 입력 검사용 */
int seen[MAX_NODES];

/* 계층 구조 출력용 */
char preLabel[MAX_NODES];
int preDepth[MAX_NODES];
int preCount = 0;

/* 트리 정보 */
int totalNodes = 0;
int leafCount = 0;
int nonLeafCount = 0;
int maxDepth = 0;
int maxDegree = 0;

/* C의 정보 */
char parentOfC = 0;
char childrenOfC[MAX_NODES];
int childrenOfCCount = 0;


/* 트리 파싱 */
int parseTree(void)
{
    /* 노드 이름 검사 */
    if (pos >= len || input[pos] < 'A' || input[pos] > 'Z') {
        printf("오류: %d번째 위치에 노드 이름이 필요합니다.\n", pos + 1);
        return 0;
    }

    char label = input[pos];
    int idx = label - 'A';

    /* 중복 노드 검사 */
    if (seen[idx]) {
        printf("오류: 노드 '%c'가 중복되었습니다.\n", label);
        return 0;
    }

    seen[idx] = 1;

    /* 전체 노드 수 */
    totalNodes++;

    /*
     * 처음에는 단말 노드라고 가정한다.
     * 뒤에 '('가 나오면 비단말 노드로 변경한다.
     */
    leafCount++;

    /* 현재 깊이 */
    int depth = top + 1;

    if (depth > maxDepth)
        maxDepth = depth;

    /*
     * 부모 노드가 있는 경우
     */
    if (top >= 0) {

        /* 부모의 자식 수 증가 */
        childCntStack[top]++;

        /*
         * 현재 노드가 C라면
         * 노드 스택으로 부모를 구한다.
         */
        if (label == 'C')
            parentOfC = nodeStack[top];

        /*
         * 부모가 C라면
         * 카운터 스택을 이용하여 C의 자식을 저장한다.
         */
        if (nodeStack[top] == 'C') {
            int childIndex = childCntStack[top] - 1;

            childrenOfC[childIndex] = label;
            childrenOfCCount = childCntStack[top];
        }
    }

    /* 계층 구조 출력용 정보 저장 */
    preLabel[preCount] = label;
    preDepth[preCount] = depth;
    preCount++;

    pos++;

    /*
     * '('가 나오면 현재 노드는 자식을 가진 비단말 노드
     */
    if (pos < len && input[pos] == '(') {

        leafCount--;
        nonLeafCount++;

        pos++;  /* '(' 소비 */

        /* 빈 괄호 검사 */
        if (pos >= len || input[pos] == ')') {
            printf("오류: 노드 '%c'의 자식이 없습니다.\n", label);
            return 0;
        }

        /* 현재 노드를 스택에 push */
        top++;

        if (top >= MAX_NODES) {
            printf("오류: 트리의 깊이가 너무 큽니다.\n");
            return 0;
        }

        nodeStack[top] = label;
        childCntStack[top] = 0;

        while (1) {

            /* 자식 또는 서브트리 파싱 */
            if (!parseTree())
                return 0;

            if (pos >= len) {
                printf("오류: ')'가 필요합니다.\n");
                return 0;
            }

            if (input[pos] == ',') {
                pos++;

                /* A(B,) 같은 형태 검사 */
                if (pos >= len || input[pos] == ')') {
                    printf("오류: ',' 뒤에 노드가 필요합니다.\n");
                    return 0;
                }
            }
            else if (input[pos] == ')') {
                break;
            }
            else {
                printf("오류: %d번째 위치에 ',' 또는 ')'가 필요합니다.\n",
                       pos + 1);
                return 0;
            }
        }

        /*
         * 현재 노드의 차수와
         * 전체 트리의 최대 차수를 비교
         */
        if (childCntStack[top] > maxDegree)
            maxDegree = childCntStack[top];

        pos++;  /* ')' 소비 */

        top--;  /* pop */
    }

    return 1;
}


/*
 * 노드 이름이 레벨 순서로
 * A, B, C, D ... 사용되었는지 검사
 */
int validateLevelOrder(void)
{
    char expected = 'A';

    for (int depth = 0; depth <= maxDepth; depth++) {

        for (int i = 0; i < preCount; i++) {

            if (preDepth[i] == depth) {

                if (preLabel[i] != expected) {
                    printf("오류: 노드 이름은 A부터 알파벳 순서대로 사용해야 합니다.\n");
                    return 0;
                }

                expected++;
            }
        }
    }

    return 1;
}


/* 트리를 왼쪽으로 눕힌 형태로 출력 */
void printTree(void)
{
    for (int i = 0; i < preCount; i++) {

        int depth = preDepth[i];

        if (depth == 0) {
            printf("%c\n", preLabel[i]);
        }
        else {
            for (int j = 0; j < depth - 1; j++)
                printf("    ");

            printf("+---%c\n", preLabel[i]);
        }
    }
}


int main(void)
{
    printf("트리를 괄호 표기법으로 입력하세요: ");

    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("오류: 입력이 없습니다.\n");
        return 1;
    }

    /* 개행 제거 */
    len = (int)strlen(input);

    while (len > 0 &&
          (input[len - 1] == '\n' || input[len - 1] == '\r')) {
        input[--len] = '\0';
    }

    if (len == 0) {
        printf("오류: 입력이 비어 있습니다.\n");
        return 1;
    }

    /* 루트는 반드시 A */
    if (input[0] != 'A') {
        printf("오류: 루트 노드는 반드시 A여야 합니다.\n");
        return 1;
    }

    /* 허용 문자 검사 */
    for (int i = 0; i < len; i++) {

        char c = input[i];

        if (!((c >= 'A' && c <= 'Z') ||
              c == '(' ||
              c == ')' ||
              c == ',')) {

            printf("오류: 허용되지 않은 문자 '%c'가 있습니다.\n", c);
            return 1;
        }
    }

    /* 트리 파싱 */
    if (!parseTree())
        return 1;

    /* 파싱 이후 문자가 남아 있으면 오류 */
    if (pos != len) {
        printf("오류: %d번째 위치부터 불필요한 문자가 있습니다.\n",
               pos + 1);
        return 1;
    }

    /* A부터 연속된 알파벳 사용 여부 검사 */
    for (int i = 0; i < totalNodes; i++) {

        if (!seen[i]) {
            printf("오류: 노드 '%c'가 누락되었습니다.\n", 'A' + i);
            return 1;
        }
    }

    /* 레벨 순서 기준 알파벳 순서 검사 */
    if (!validateLevelOrder())
        return 1;

    /*
     * 루트 깊이를 0으로 계산했으므로
     * 높이 = 최대 깊이 + 1
     */
    int height = maxDepth + 1;

    printf("\n[트리 정보]\n");
    printf("전체 노드의 수   : %d\n", totalNodes);
    printf("단말 노드의 수   : %d\n", leafCount);
    printf("비단말 노드의 수 : %d\n", nonLeafCount);
    printf("트리의 높이      : %d\n", height);
    printf("트리의 차수      : %d\n", maxDegree);

    /* C의 정보 */
    if (!seen['C' - 'A']) {
        printf("노드 C           : 존재하지 않음\n");
    }
    else {
        printf("C의 부모 노드    : ");

        if (parentOfC == 0)
            printf("없음\n");
        else
            printf("%c\n", parentOfC);

        printf("C의 자식 노드    : ");

        if (childrenOfCCount == 0) {
            printf("없음\n");
        }
        else {
            for (int i = 0; i < childrenOfCCount; i++) {

                printf("%c", childrenOfC[i]);

                if (i < childrenOfCCount - 1)
                    printf(", ");
            }

            printf("\n");
        }
    }

    printf("\n[계층 구조]\n");
    printTree();

    return 0;
}