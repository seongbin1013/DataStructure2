#define _POSIX_C_SOURCE 200809L /* macOS에서 snprintf, clock_gettime 선언을 위해 200112L 이상 필요 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define V_COUNT        50
#define SPARSE_EDGES   100
#define DENSE_EDGES    600
#define MAX_WEIGHT     1000
#define DEFAULT_REPEAT 10000
#define TRIALS         5      /* repeat회 평균을 TRIALS번 측정해 중앙값 사용 (노이즈 완화) */
#define INF            INT_MAX

/* ------------------------------------------------------------------ */
/* 자료형                                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    int u, v, w;
} Edge;

typedef struct AdjNode {
    int to;
    int weight;
    struct AdjNode *next;
} AdjNode;

typedef struct {
    int n;          /* 정점 수 */
    int e;          /* 간선 수 (무방향 간선 1개 = 1) */
    AdjNode **head; /* head[u] : u의 인접 리스트 첫 노드 */
    size_t mem;     /* 그래프 표현에 사용한 메모리(Bytes) */
} AdjList;

typedef struct {
    int n;
    int e;
    int *w;         /* w[u * n + v] : 가중치, 0이면 간선 없음 (가중치는 양의 정수) */
    size_t mem;
} AdjMatrix;

typedef struct {
    int    mst_edges;
    long   mst_cost;
    size_t graph_mem;
    size_t aux_mem;
    size_t result_mem;
    size_t total_mem;
    double time_ms;
    Edge   mst[V_COUNT - 1];
} CaseResult;

typedef int (*MstFn)(const void *graph, Edge *mst, size_t *aux_mem);

/* ------------------------------------------------------------------ */
/* 메모리 사용량을 세면서 할당하는 보조 함수                                   */
/* ------------------------------------------------------------------ */
static void *tracked_alloc(size_t size, size_t *counter)
{
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    *counter += size;
    return p;
}

static void *tracked_calloc(size_t count, size_t size, size_t *counter)
{
    void *p = calloc(count, size);
    if (p == NULL) {
        fprintf(stderr, "memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    *counter += count * size;
    return p;
}

/* ------------------------------------------------------------------ */
/* 난수 (xorshift32) : OS와 무관하게 같은 seed -> 같은 그래프                    */
/* ------------------------------------------------------------------ */
static unsigned int rng_state = 1;

static unsigned int rng_next(void)
{
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static int rng_below(int n)
{
    return (int)(rng_next() % (unsigned int)n);
}

/* ------------------------------------------------------------------ */
/* 1. 그래프 생성                                                        */
/* ------------------------------------------------------------------ */

/* 연결성 검사 (DFS, 임시 메모리 사용 -> 측정 대상 아님) */
static int is_connected(const Edge *edges, int e)
{
    static unsigned char adj[V_COUNT][V_COUNT];
    unsigned char visited[V_COUNT] = {0};
    int stack[V_COUNT];
    int top = 0, count = 0;

    memset(adj, 0, sizeof(adj));
    for (int i = 0; i < e; i++) {
        adj[edges[i].u][edges[i].v] = 1;
        adj[edges[i].v][edges[i].u] = 1;
    }

    stack[top++] = 0;
    visited[0] = 1;
    while (top > 0) {
        int x = stack[--top];
        count++;
        for (int y = 0; y < V_COUNT; y++) {
            if (adj[x][y] && !visited[y]) {
                visited[y] = 1;
                stack[top++] = y;
            }
        }
    }
    return count == V_COUNT;
}

/*
 * 정점 V_COUNT개, 간선 e개인 연결 단순 무방향 그래프를 랜덤 생성한다.
 *  1) 랜덤 순서로 섞은 정점을 이미 연결된 정점 하나에 붙여 신장 트리(V-1개 간선)를 먼저 만든다.
 *  2) 남은 간선은 자기 자신 연결, 중복 간선이 아닌 임의의 두 정점 쌍으로 채운다.
 *  3) 마지막으로 연결 그래프인지 다시 확인하고, 아니면 다시 생성한다.
 */
static void generate_graph(Edge *edges, int e)
{
    unsigned char used[V_COUNT][V_COUNT];
    int perm[V_COUNT];

    if (e < V_COUNT - 1 || e > V_COUNT * (V_COUNT - 1) / 2) {
        fprintf(stderr, "invalid edge count: %d\n", e);
        exit(EXIT_FAILURE);
    }

    do {
        int cnt = 0;
        memset(used, 0, sizeof(used));

        for (int i = 0; i < V_COUNT; i++)
            perm[i] = i;
        for (int i = V_COUNT - 1; i > 0; i--) {
            int j = rng_below(i + 1);
            int t = perm[i];
            perm[i] = perm[j];
            perm[j] = t;
        }

        for (int i = 1; i < V_COUNT; i++) {
            int a = perm[i];
            int b = perm[rng_below(i)];
            used[a][b] = used[b][a] = 1;
            edges[cnt].u = a;
            edges[cnt].v = b;
            edges[cnt].w = 1 + rng_below(MAX_WEIGHT);
            cnt++;
        }

        while (cnt < e) {
            int a = rng_below(V_COUNT);
            int b = rng_below(V_COUNT);
            if (a == b || used[a][b])
                continue;
            used[a][b] = used[b][a] = 1;
            edges[cnt].u = a;
            edges[cnt].v = b;
            edges[cnt].w = 1 + rng_below(MAX_WEIGHT);
            cnt++;
        }
    } while (!is_connected(edges, e));
}

/* ------------------------------------------------------------------ */
/* 그래프 표현                                                           */
/* ------------------------------------------------------------------ */
static AdjList *adjlist_build(const Edge *edges, int n, int e)
{
    AdjList *g = malloc(sizeof(AdjList));
    if (g == NULL)
        exit(EXIT_FAILURE);
    g->n = n;
    g->e = e;
    g->mem = 0;
    g->head = tracked_calloc((size_t)n, sizeof(AdjNode *), &g->mem);

    for (int i = 0; i < e; i++) {
        AdjNode *a = tracked_alloc(sizeof(AdjNode), &g->mem);
        a->to = edges[i].v;
        a->weight = edges[i].w;
        a->next = g->head[edges[i].u];
        g->head[edges[i].u] = a;

        AdjNode *b = tracked_alloc(sizeof(AdjNode), &g->mem);
        b->to = edges[i].u;
        b->weight = edges[i].w;
        b->next = g->head[edges[i].v];
        g->head[edges[i].v] = b;
    }
    return g;
}

static void adjlist_free(AdjList *g)
{
    for (int u = 0; u < g->n; u++) {
        AdjNode *p = g->head[u];
        while (p != NULL) {
            AdjNode *next = p->next;
            free(p);
            p = next;
        }
    }
    free(g->head);
    free(g);
}

static AdjMatrix *adjmatrix_build(const Edge *edges, int n, int e)
{
    AdjMatrix *g = malloc(sizeof(AdjMatrix));
    if (g == NULL)
        exit(EXIT_FAILURE);
    g->n = n;
    g->e = e;
    g->mem = 0;
    g->w = tracked_calloc((size_t)n * (size_t)n, sizeof(int), &g->mem);

    for (int i = 0; i < e; i++) {
        g->w[edges[i].u * n + edges[i].v] = edges[i].w;
        g->w[edges[i].v * n + edges[i].u] = edges[i].w;
    }
    return g;
}

static void adjmatrix_free(AdjMatrix *g)
{
    free(g->w);
    free(g);
}

/* ------------------------------------------------------------------ */
/* 3. Kruskal 알고리즘 (인접 리스트)                                         */
/* ------------------------------------------------------------------ */

/* 간선 정렬용 힙 정렬 : 가중치 오름차순, 추가 메모리 없이 제자리 정렬 */
static void sift_down(Edge *a, int root, int end)
{
    while (2 * root + 1 <= end) {
        int child = 2 * root + 1;
        if (child + 1 <= end && a[child].w < a[child + 1].w)
            child++;
        if (a[root].w < a[child].w) {
            Edge t = a[root];
            a[root] = a[child];
            a[child] = t;
            root = child;
        } else {
            return;
        }
    }
}

static void heap_sort_edges(Edge *a, int n)
{
    for (int start = (n - 2) / 2; start >= 0; start--)
        sift_down(a, start, n - 1);
    for (int end = n - 1; end > 0; end--) {
        Edge t = a[0];
        a[0] = a[end];
        a[end] = t;
        sift_down(a, 0, end - 1);
    }
}

/* Union-Find (경로 압축 + 랭크 기반 합치기) */
static int ds_find(int *parent, int x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

static int ds_union(int *parent, int *rank, int a, int b)
{
    a = ds_find(parent, a);
    b = ds_find(parent, b);
    if (a == b)
        return 0; /* 같은 집합 -> 사이클 발생 */
    if (rank[a] < rank[b]) {
        int t = a;
        a = b;
        b = t;
    }
    parent[b] = a;
    if (rank[a] == rank[b])
        rank[a]++;
    return 1;
}

static int kruskal(const AdjList *g, Edge *mst, size_t *aux_mem)
{
    int n = g->n;
    int picked = 0, m = 0;

    *aux_mem = 0;
    Edge *edges = tracked_alloc((size_t)g->e * sizeof(Edge), aux_mem);
    int *parent = tracked_alloc((size_t)n * sizeof(int), aux_mem);
    int *rank = tracked_alloc((size_t)n * sizeof(int), aux_mem);

    /* 인접 리스트에서 간선 수집 (무방향이라 u < v 인 쪽만 사용) */
    for (int u = 0; u < n; u++) {
        for (const AdjNode *p = g->head[u]; p != NULL; p = p->next) {
            if (u < p->to) {
                edges[m].u = u;
                edges[m].v = p->to;
                edges[m].w = p->weight;
                m++;
            }
        }
    }

    heap_sort_edges(edges, m);

    for (int i = 0; i < n; i++) {
        parent[i] = i;
        rank[i] = 0;
    }

    for (int i = 0; i < m && picked < n - 1; i++) {
        if (ds_union(parent, rank, edges[i].u, edges[i].v))
            mst[picked++] = edges[i];
    }

    free(edges);
    free(parent);
    free(rank);
    return picked;
}

/* ------------------------------------------------------------------ */
/* 4. Prim 알고리즘 (인접 행렬, O(V^2))                                     */
/* ------------------------------------------------------------------ */
static int prim(const AdjMatrix *g, Edge *mst, size_t *aux_mem)
{
    int n = g->n;
    int picked = 0;

    *aux_mem = 0;
    int *key = tracked_alloc((size_t)n * sizeof(int), aux_mem);
    int *parent = tracked_alloc((size_t)n * sizeof(int), aux_mem);
    unsigned char *visited = tracked_alloc((size_t)n * sizeof(unsigned char), aux_mem);

    for (int i = 0; i < n; i++) {
        key[i] = INF;
        parent[i] = -1;
        visited[i] = 0;
    }
    key[0] = 0; /* 시작 정점 */

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        for (int i = 0; i < n; i++) {
            if (!visited[i] && (u == -1 || key[i] < key[u]))
                u = i;
        }
        if (u == -1 || key[u] == INF)
            break; /* 연결되지 않은 그래프 */

        visited[u] = 1;
        if (parent[u] != -1) {
            mst[picked].u = parent[u];
            mst[picked].v = u;
            mst[picked].w = key[u];
            picked++;
        }

        const int *row = &g->w[u * n];
        for (int v = 0; v < n; v++) {
            if (row[v] != 0 && !visited[v] && row[v] < key[v]) {
                key[v] = row[v];
                parent[v] = u;
            }
        }
    }

    free(key);
    free(parent);
    free(visited);
    return picked;
}

/* MstFn 형태로 맞추기 위한 래퍼 */
static int kruskal_fn(const void *g, Edge *mst, size_t *aux)
{
    return kruskal((const AdjList *)g, mst, aux);
}

static int prim_fn(const void *g, Edge *mst, size_t *aux)
{
    return prim((const AdjMatrix *)g, mst, aux);
}

/* ------------------------------------------------------------------ */
/* 6. 성능 측정                                                          */
/* ------------------------------------------------------------------ */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void run_case(MstFn fn, const void *graph, size_t graph_mem, int repeat, CaseResult *r)
{
    size_t aux = 0;
    volatile long sink = 0;
    Edge tmp[V_COUNT - 1];

    /* 1회 실행: 결과(간선, 비용)와 메모리 확보, 캐시 예열 */
    r->mst_edges = fn(graph, r->mst, &aux);
    r->mst_cost = 0;
    for (int i = 0; i < r->mst_edges; i++)
        r->mst_cost += r->mst[i].w;

    r->graph_mem = graph_mem;
    r->aux_mem = aux;
    r->result_mem = sizeof(r->mst);
    r->total_mem = r->graph_mem + r->aux_mem + r->result_mem;

    /*
     * MST 계산만 repeat번 반복해서 1회 평균 시간(ms)을 구한다 (그래프 생성/출력 제외).
     * 이 측정을 TRIALS번 하고 중앙값을 사용 -> 다른 프로세스 때문에 한 번 튀는 값의 영향을 줄임
     */
    double trial[TRIALS];
    for (int t = 0; t < TRIALS; t++) {
        double t0 = now_ms();
        for (int i = 0; i < repeat; i++) {
            size_t a = 0;
            int c = fn(graph, tmp, &a);
            sink += c + tmp[0].w;
        }
        double t1 = now_ms();
        trial[t] = (t1 - t0) / repeat;
    }
    /* 삽입 정렬 후 가운데 값 */
    for (int i = 1; i < TRIALS; i++) {
        double key = trial[i];
        int j = i - 1;
        while (j >= 0 && trial[j] > key) {
            trial[j + 1] = trial[j];
            j--;
        }
        trial[j + 1] = key;
    }
    r->time_ms = trial[TRIALS / 2];
    (void)sink;
}

/* ------------------------------------------------------------------ */
/* 5. 결과 출력 / 검증                                                    */
/* ------------------------------------------------------------------ */
static void print_case(int no, const char *name, int v, int e, const CaseResult *r)
{
    printf("Case %d : %s\n", no, name);
    printf("Vertices      : %d\n", v);
    printf("Edges         : %d\n", e);
    printf("MST Edges     : %d\n", r->mst_edges);
    printf("MST Cost      : %ld\n", r->mst_cost);
    printf("Memory        : %zu Bytes\n", r->total_mem);
    printf("Execution Time: %.6f ms\n", r->time_ms);
    printf("MST Edge List (u - v : weight)\n");
    for (int i = 0; i < r->mst_edges; i++) {
        printf("  %2d - %2d : %4d%s", r->mst[i].u, r->mst[i].v, r->mst[i].w,
               (i % 5 == 4 || i == r->mst_edges - 1) ? "\n" : "  |");
    }
    printf("\n");
}

/* MST 검증: 간선 수가 V-1이고, 사이클 없이 모든 정점을 연결하는가 */
static int verify_tree(const CaseResult *r, int n)
{
    int parent[V_COUNT], rank[V_COUNT];

    if (r->mst_edges != n - 1)
        return 0;
    for (int i = 0; i < n; i++) {
        parent[i] = i;
        rank[i] = 0;
    }
    for (int i = 0; i < r->mst_edges; i++) {
        if (!ds_union(parent, rank, r->mst[i].u, r->mst[i].v))
            return 0; /* 사이클 */
    }
    return 1;
}

static void print_verify(const char *msg, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", msg);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    unsigned int seed = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10)
                                   : (unsigned int)time(NULL);
    int repeat = (argc > 2) ? atoi(argv[2]) : DEFAULT_REPEAT;
    if (repeat < 1)
        repeat = DEFAULT_REPEAT;
    rng_state = (seed == 0) ? 1 : seed;

    Edge sparse[SPARSE_EDGES];
    Edge dense[DENSE_EDGES];
    generate_graph(sparse, SPARSE_EDGES);
    generate_graph(dense, DENSE_EDGES);

    /* 같은 그래프(간선/가중치)를 두 가지 표현으로 만든다 */
    AdjList   *sparse_list = adjlist_build(sparse, V_COUNT, SPARSE_EDGES);
    AdjList   *dense_list = adjlist_build(dense, V_COUNT, DENSE_EDGES);
    AdjMatrix *sparse_mat = adjmatrix_build(sparse, V_COUNT, SPARSE_EDGES);
    AdjMatrix *dense_mat = adjmatrix_build(dense, V_COUNT, DENSE_EDGES);

    CaseResult res[4];
    run_case(kruskal_fn, sparse_list, sparse_list->mem, repeat, &res[0]);
    run_case(kruskal_fn, dense_list, dense_list->mem, repeat, &res[1]);
    run_case(prim_fn, sparse_mat, sparse_mat->mem, repeat, &res[2]);
    run_case(prim_fn, dense_mat, dense_mat->mem, repeat, &res[3]);

    printf("=== MST Comparison (seed = %u, repeat = %d) ===\n", seed, repeat);
    printf("Execution Time = MST 계산 %d회 반복 평균을 %d번 측정한 중앙값 (그래프 생성/출력 제외)\n",
           repeat, TRIALS);
    printf("Memory = 그래프 표현 + 알고리즘 보조 자료구조 + MST 결과 배열\n\n");

    print_case(1, "Sparse Graph - Adjacency List - Kruskal", V_COUNT, SPARSE_EDGES, &res[0]);
    print_case(2, "Dense Graph - Adjacency List - Kruskal", V_COUNT, DENSE_EDGES, &res[1]);
    print_case(3, "Sparse Graph - Adjacency Matrix - Prim", V_COUNT, SPARSE_EDGES, &res[2]);
    print_case(4, "Dense Graph - Adjacency Matrix - Prim", V_COUNT, DENSE_EDGES, &res[3]);

    printf("[Memory Breakdown] (Bytes)\n");
    printf("%-6s %12s %14s %12s %8s\n", "Case", "Graph", "Algo aux", "MST result", "Total");
    for (int i = 0; i < 4; i++) {
        printf("Case %d %12zu %14zu %12zu %8zu\n", i + 1, res[i].graph_mem, res[i].aux_mem,
               res[i].result_mem, res[i].total_mem);
    }
    printf("\n");

    printf("[Verification]\n");
    int all_ok = 1, ok;
    ok = is_connected(sparse, SPARSE_EDGES) && is_connected(dense, DENSE_EDGES);
    print_verify("Sparse/Dense graph are connected", ok);
    all_ok &= ok;
    for (int i = 0; i < 4; i++) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Case %d MST is a spanning tree (%d edges, no cycle)", i + 1,
                 V_COUNT - 1);
        ok = verify_tree(&res[i], V_COUNT);
        print_verify(msg, ok);
        all_ok &= ok;
    }
    ok = (res[0].mst_cost == res[2].mst_cost);
    print_verify("Sparse: Kruskal cost == Prim cost", ok);
    all_ok &= ok;
    ok = (res[1].mst_cost == res[3].mst_cost);
    print_verify("Dense : Kruskal cost == Prim cost", ok);
    all_ok &= ok;

    adjlist_free(sparse_list);
    adjlist_free(dense_list);
    adjmatrix_free(sparse_mat);
    adjmatrix_free(dense_mat);
    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}