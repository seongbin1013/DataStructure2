#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define V      10                 /* 정점 수 */
#define E      20                 /* 간선 수 */
#define MIN_W  1                  /* 최소 가중치 */
#define MAX_W  100                /* 최대 가중치 */
#define INF    1000000000         /* 경로 없음 */
#define NONE   (-1)               /* 다음 정점 없음 */
#define PAIRS  (V * (V - 1) / 2)  /* 서로 다른 정점 쌍 수 = 45 */

typedef struct {
    int u, v, w;                  /* u < v 로 저장 */
} Edge;

/* ---------- 그래프 (인접 행렬 + 간선 목록) ---------- */
static int  adj[V][V];            /* 0이면 간선 없음, 아니면 가중치 */
static Edge edges[E];
static int  edge_count = 0;

/* ---------- Floyd-Warshall 결과 ---------- */
static int dist[V][V];            /* 최단거리 */
static int next_v[V][V];          /* 경로 복원용 다음 정점 */

/* ---------- 난수: xorshift32 (OS와 관계없이 같은 시드면 같은 결과) ---------- */
static uint32_t rng_state = 2463534242u;

static void rng_seed(uint32_t seed)
{
    rng_state = (seed == 0) ? 2463534242u : seed;   /* 0이면 xorshift가 멈추므로 대체 */
}

static uint32_t xorshift32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

static int rand_range(int lo, int hi)               /* lo 이상 hi 이하 */
{
    return lo + (int)(xorshift32() % (uint32_t)(hi - lo + 1));
}

/* ---------- 그래프 생성 ---------- */
static void add_edge(int u, int v, int w)
{
    if (u > v) { int t = u; u = v; v = t; }
    adj[u][v] = w;
    adj[v][u] = w;
    edges[edge_count].u = u;
    edges[edge_count].v = v;
    edges[edge_count].w = w;
    edge_count++;
}

static int cmp_edge(const void *a, const void *b)
{
    const Edge *x = (const Edge *)a;
    const Edge *y = (const Edge *)b;
    if (x->u != y->u) return x->u - y->u;
    return x->v - y->v;
}

/*
 * 1) 정점을 무작위로 섞고, perm[i]를 perm[0..i-1] 중 하나와 연결 → 신장 트리 간선 9개 (항상 연결)
 * 2) 자기 자신 연결·중복이 아닌 간선을 무작위로 골라 20개가 될 때까지 추가
 */
static void generate_graph(void)
{
    int perm[V];
    int i;

    for (i = 0; i < V; i++) perm[i] = i;
    for (i = V - 1; i > 0; i--) {                   /* Fisher-Yates 섞기 */
        int j = rand_range(0, i);
        int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }

    for (i = 1; i < V; i++) {                       /* 신장 트리 */
        /* 함수 인자의 계산 순서는 C 표준에서 정해져 있지 않음 (gcc와 clang이 다름).
           난수를 인자 안에서 두 번 뽑으면 컴파일러마다 그래프가 달라지므로 따로 뽑음. */
        int j = rand_range(0, i - 1);
        int w = rand_range(MIN_W, MAX_W);
        add_edge(perm[i], perm[j], w);
    }

    while (edge_count < E) {                        /* 나머지 간선 */
        int u = rand_range(0, V - 1);
        int v = rand_range(0, V - 1);
        if (u == v || adj[u][v] != 0) continue;     /* 자기 자신 연결, 중복 간선 제외 */
        add_edge(u, v, rand_range(MIN_W, MAX_W));
    }

    qsort(edges, (size_t)edge_count, sizeof(Edge), cmp_edge);
}

/* ---------- Floyd-Warshall ---------- */
static void floyd_warshall(void)
{
    int i, j, k;

    for (i = 0; i < V; i++) {
        for (j = 0; j < V; j++) {
            if (i == j) {
                dist[i][j] = 0;
                next_v[i][j] = i;
            } else if (adj[i][j] != 0) {
                dist[i][j] = adj[i][j];
                next_v[i][j] = j;                   /* 직접 연결: 다음 정점은 j */
            } else {
                dist[i][j] = INF;
                next_v[i][j] = NONE;
            }
        }
    }

    /* k를 경유하는 것이 더 짧으면 갱신 */
    for (k = 0; k < V; k++) {
        for (i = 0; i < V; i++) {
            if (dist[i][k] == INF) continue;
            for (j = 0; j < V; j++) {
                if (dist[k][j] == INF) continue;
                if (dist[i][k] + dist[k][j] < dist[i][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next_v[i][j] = next_v[i][k];    /* i→j의 첫 걸음 = i→k의 첫 걸음 */
                }
            }
        }
    }
}

/* s에서 t까지 경로를 path[]에 복원하고 정점 수를 반환 (경로 없으면 0) */
static int build_path(int s, int t, int path[])
{
    int len = 0;

    if (next_v[s][t] == NONE) return 0;
    path[len++] = s;
    while (s != t) {
        s = next_v[s][t];
        if (s == NONE || len >= V) return 0;        /* 방어 코드: 정상이면 도달하지 않음 */
        path[len++] = s;
    }
    return len;
}

/* ---------- 연결성 확인 (DFS) ---------- */
static void dfs(int u, int visited[])
{
    int v;
    visited[u] = 1;
    for (v = 0; v < V; v++)
        if (adj[u][v] != 0 && !visited[v]) dfs(v, visited);
}

static int is_connected(void)
{
    int visited[V] = {0};
    int i;
    dfs(0, visited);
    for (i = 0; i < V; i++)
        if (!visited[i]) return 0;
    return 1;
}

/* ---------- 출력 ---------- */
static void print_graph(void)
{
    int i;

    printf("Graph\n");
    printf("Vertices : %d\n", V);
    printf("Edges    : %d\n\n", edge_count);

    printf("Edges\n");
    for (i = 0; i < edge_count; i++)
        printf("(%d, %d) Weight: %d\n", edges[i].u, edges[i].v, edges[i].w);
    printf("\n");
}

/* 45쌍을 출력하고, 경로의 간선 가중치 합이 최단거리와 일치한 쌍의 수를 반환 */
static int print_shortest_paths(int *pair_count)
{
    int path[V];
    int s, t, i, len, sum, matched = 0;

    *pair_count = 0;
    printf("Shortest Paths\n\n");

    for (s = 0; s < V; s++) {
        for (t = s + 1; t < V; t++) {               /* u < v 인 쌍만 */
            len = build_path(s, t, path);
            printf("%d -> %d\n", s, t);

            if (len == 0) {
                printf("Path     : (no path)\n");
                printf("Distance : INF\n\n");
                (*pair_count)++;
                continue;
            }

            printf("Path     : ");
            for (i = 0; i < len; i++)
                printf(i == 0 ? "%d" : " -> %d", path[i]);
            printf("\n");

            /* 경로를 따라 인접 행렬의 가중치를 다시 더해 최단거리와 비교 */
            sum = 0;
            for (i = 0; i + 1 < len; i++) sum += adj[path[i]][path[i + 1]];
            if (sum == dist[s][t]) matched++;

            printf("Distance : %d", dist[s][t]);
            if (len > 2) {                          /* 간선이 2개 이상이면 합산 과정 표시 */
                printf(" (");
                for (i = 0; i + 1 < len; i++)
                    printf(i == 0 ? "%d" : " + %d", adj[path[i]][path[i + 1]]);
                printf(")");
            }
            printf("\n\n");
            (*pair_count)++;
        }
    }

    printf("Total vertex pairs : %d\n\n", *pair_count);
    return matched;
}

static const char *ok(int cond) { return cond ? "OK" : "FAIL"; }

/* 과제의 '7. 확인 사항'을 실행 결과에서 바로 볼 수 있도록 출력 */
static void print_check(int pair_count, int matched)
{
    int i, j, self_loops = 0, dup = 0, bad_w = 0, matrix_edges = 0;

    for (i = 0; i < V; i++) {
        if (adj[i][i] != 0) self_loops++;
        for (j = i + 1; j < V; j++)
            if (adj[i][j] != 0) matrix_edges++;
    }
    for (i = 0; i < edge_count; i++) {
        if (edges[i].u == edges[i].v) self_loops++;
        if (edges[i].w < MIN_W || edges[i].w > MAX_W) bad_w++;
        if (i > 0 && edges[i].u == edges[i - 1].u && edges[i].v == edges[i - 1].v) dup++;  /* 정렬되어 있음 */
    }
    if (matrix_edges != edge_count) dup++;          /* 행렬과 목록의 간선 수가 다르면 중복이 있었던 것 */

    printf("Check\n");
    printf("Vertices          : %d (%s)\n", V, ok(V == 10));
    printf("Edges             : %d (%s)\n", edge_count, ok(edge_count == E && matrix_edges == E));
    printf("Weight %d ~ %-3d    : %d out of range (%s)\n", MIN_W, MAX_W, bad_w, ok(bad_w == 0));
    printf("Self-loops        : %d (%s)\n", self_loops, ok(self_loops == 0));
    printf("Duplicate edges   : %d (%s)\n", dup, ok(dup == 0));
    printf("Connected         : %s (%s)\n", is_connected() ? "Yes" : "No", ok(is_connected()));
    printf("Vertex pairs      : %d (%s)\n", pair_count, ok(pair_count == PAIRS));
    printf("Path sum = Dist   : %d / %d (%s)\n", matched, pair_count, ok(matched == PAIRS));
}

int main(int argc, char *argv[])
{
    uint32_t seed = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : (uint32_t)time(NULL);
    int pair_count, matched;

    rng_seed(seed);
    generate_graph();

    printf("Seed : %u\n\n", (unsigned)seed);
    print_graph();                                  /* 최단경로 계산 전에 그래프 출력 */
    floyd_warshall();

    matched = print_shortest_paths(&pair_count);
    print_check(pair_count, matched);

    return 0;
}
