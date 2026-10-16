#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define V 100
#define SPARSE_E 100
#define DENSE_E 4000
#define TRIALS 1000

typedef struct { int u, v; } Edge;

/* ---------- 인접 행렬 ---------- */
typedef struct {
    int n;
    int edge_count;
    int m[V][V];
} MatGraph;

/* ---------- 인접 리스트 ---------- */
typedef struct Node {
    int vertex;
    struct Node *next;
} Node;

typedef struct {
    int n;
    int edge_count;
    int node_count;       /* 동적으로 할당된 노드 수 (메모리 계산용) */
    Node *head[V];
} ListGraph;

/* ---------- 실험 대상 연산 ---------- */
typedef struct {
    int ins_u, ins_v;     /* 삽입할 간선 (그래프에 없음) */
    int del_u, del_v;     /* 삭제할 간선 (그래프에 있음) */
    int con_u, con_v;     /* 연결 여부 확인할 정점 쌍 */
    int adj_w;            /* 인접 정점을 출력할 정점 */
} Ops;

typedef struct {
    size_t memory;
    long ins, del, con, adj;
    int connected;        /* 연결 확인 결과 (1: 연결됨, 0: 연결 안 됨) */
} Result;

/* ================= 인접 행렬 ================= */
static int mat_has(const MatGraph *g, int u, int v, long *cmp) {
    (*cmp)++;
    return g->m[u][v];
}

static int mat_insert(MatGraph *g, int u, int v, long *cmp) {
    if (mat_has(g, u, v, cmp)) return 0;    /* 이미 있으면 삽입 X */
    g->m[u][v] = 1;
    g->m[v][u] = 1;
    g->edge_count++;
    return 1;
}

static int mat_delete(MatGraph *g, int u, int v, long *cmp) {
    if (!mat_has(g, u, v, cmp)) return 0;   /* 없으면 삭제 X */
    g->m[u][v] = 0;
    g->m[v][u] = 0;
    g->edge_count--;
    return 1;
}

static void mat_adjacent(const MatGraph *g, int w, long *cmp, int print) {
    for (int j = 0; j < V; j++) {
        (*cmp)++;
        if (g->m[w][j] && print) printf("%d ", j);
    }
}

static size_t mat_memory(const MatGraph *g) {
    (void)g;
    return sizeof(MatGraph);
}

/* ================= 인접 리스트 ================= */
static int list_contains(const ListGraph *g, int u, int v, long *cmp) {
    for (Node *p = g->head[u]; p; p = p->next) {
        (*cmp)++;
        if (p->vertex == v) return 1;
    }
    return 0;
}

static void list_push_front(ListGraph *g, int u, int v) {
    Node *nd = (Node *)malloc(sizeof(Node));
    if (!nd) { fprintf(stderr, "malloc failed\n"); exit(1); }
    nd->vertex = v;
    nd->next = g->head[u];
    g->head[u] = nd;
    g->node_count++;
}

static int list_remove(ListGraph *g, int u, int v, long *cmp) {
    Node *prev = NULL;
    for (Node *p = g->head[u]; p; prev = p, p = p->next) {
        (*cmp)++;
        if (p->vertex == v) {
            if (prev) prev->next = p->next;
            else g->head[u] = p->next;
            free(p);
            g->node_count--;
            return 1;
        }
    }
    return 0;
}

/* 중복 확인은 u의 리스트에서만 수행 (무방향이라 v의 리스트에도 u가 있음) */
static int list_insert(ListGraph *g, int u, int v, long *cmp) {
    if (list_contains(g, u, v, cmp)) return 0;
    list_push_front(g, u, v);
    list_push_front(g, v, u);
    g->edge_count++;
    return 1;
}

/* 삭제는 u의 리스트와 v의 리스트 양쪽에서 노드를 찾아야 함 */
static int list_delete(ListGraph *g, int u, int v, long *cmp) {
    if (!list_remove(g, u, v, cmp)) return 0;
    list_remove(g, v, u, cmp);
    g->edge_count--;
    return 1;
}

static void list_adjacent(const ListGraph *g, int w, long *cmp, int print) {
    for (Node *p = g->head[w]; p; p = p->next) {
        (*cmp)++;
        if (print) printf("%d ", p->vertex);
    }
}

static size_t list_memory(const ListGraph *g) {
    return sizeof(ListGraph) + (size_t)g->node_count * sizeof(Node);
}

static void list_free(ListGraph *g) {
    for (int i = 0; i < V; i++) {
        Node *p = g->head[i];
        while (p) { Node *t = p->next; free(p); p = t; }
        g->head[i] = NULL;
    }
    g->node_count = 0;
}

/* ================= 그래프 생성 (실험용 임시 구조) ================= */
typedef struct {
    int used[V][V];       /* 간선 존재 여부 (선택용, 메모리 측정 제외) */
    Edge *edges;          /* 간선 목록   (선택용, 메모리 측정 제외) */
    int count;
} Truth;

/* 플랫폼(macOS/Linux)마다 rand() 구현이 달라 결과가 달라지므로,
 * 어디서 실행해도 같은 결과가 나오도록 직접 만든 xorshift32 난수 생성기를 사용 */
static unsigned rng_state = 1;
static unsigned next_rand(void) {
    unsigned x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}
static int rnd(int n) { return (int)(next_rand() % (unsigned)n); }

/* 연결/미연결 분리 측정 전용 난수 (기존 실험의 난수 흐름과 분리) */
static unsigned rng2_state = 1;
static int rnd2(int n) {
    unsigned x = rng2_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng2_state = x;
    return (int)(x % (unsigned)n);
}

static void generate(Truth *t, int e) {
    memset(t->used, 0, sizeof(t->used));
    t->edges = (Edge *)malloc(sizeof(Edge) * (size_t)(e + 2));
    t->count = 0;
    while (t->count < e) {
        int u = rnd(V), v = rnd(V);
        if (u == v || t->used[u][v]) continue;
        t->used[u][v] = t->used[v][u] = 1;
        t->edges[t->count].u = u;
        t->edges[t->count].v = v;
        t->count++;
    }
}

static void build(const Truth *t, MatGraph *mg, ListGraph *lg) {
    memset(mg, 0, sizeof(*mg));
    memset(lg, 0, sizeof(*lg));
    mg->n = lg->n = V;
    for (int i = 0; i < t->count; i++) {
        int u = t->edges[i].u, v = t->edges[i].v;
        mg->m[u][v] = mg->m[v][u] = 1;
        mg->edge_count++;
        list_push_front(lg, u, v);
        list_push_front(lg, v, u);
        lg->edge_count++;
    }
}

/* 두 표현이 같은 그래프인지 검사 */
static int verify(const MatGraph *mg, const ListGraph *lg) {
    int cnt[V][V];
    memset(cnt, 0, sizeof(cnt));
    long total = 0;
    for (int u = 0; u < V; u++) {
        for (Node *p = lg->head[u]; p; p = p->next) {
            cnt[u][p->vertex]++;
            total++;
            if (!mg->m[u][p->vertex]) return 0;
        }
    }
    long ones = 0;
    for (int u = 0; u < V; u++)
        for (int v = 0; v < V; v++) {
            if (mg->m[u][v] != mg->m[v][u]) return 0;
            if (mg->m[u][u]) return 0;
            if (mg->m[u][v] != cnt[u][v]) return 0;   /* 중복 노드도 걸러짐 */
            ones += mg->m[u][v];
        }
    return ones == total && ones == 2L * mg->edge_count &&
           mg->edge_count == lg->edge_count;
}

/* ================= 실험 ================= */
static void pick_ops(const Truth *t, Ops *o) {
    int u, v;
    do { u = rnd(V); v = rnd(V); } while (u == v || t->used[u][v]);
    o->ins_u = u; o->ins_v = v;

    int k = rnd(t->count);
    o->del_u = t->edges[k].u; o->del_v = t->edges[k].v;

    do { u = rnd(V); v = rnd(V); } while (u == v);
    o->con_u = u; o->con_v = v;

    o->adj_w = rnd(V);
}

static void apply_truth(Truth *t, const Ops *o) {
    t->used[o->ins_u][o->ins_v] = t->used[o->ins_v][o->ins_u] = 1;
    t->edges[t->count].u = o->ins_u;
    t->edges[t->count].v = o->ins_v;
    t->count++;
    for (int i = 0; i < t->count; i++) {
        if ((t->edges[i].u == o->del_u && t->edges[i].v == o->del_v)) {
            t->edges[i] = t->edges[t->count - 1];
            t->count--;
            break;
        }
    }
    t->used[o->del_u][o->del_v] = t->used[o->del_v][o->del_u] = 0;
}

static Result run_mat(MatGraph *g, const Ops *o, int print_adj) {
    Result r = {0, 0, 0, 0, 0, 0};
    long dummy = 0;
    r.memory = mat_memory(g);          /* 연산 전 상태 기준 */
    mat_insert(g, o->ins_u, o->ins_v, &r.ins);
    mat_delete(g, o->del_u, o->del_v, &r.del);
    r.connected = mat_has(g, o->con_u, o->con_v, &r.con);
    if (print_adj) printf("  [Matrix] neighbors of %d: ", o->adj_w);
    mat_adjacent(g, o->adj_w, &r.adj, print_adj);
    if (print_adj) printf("\n");
    (void)dummy;
    return r;
}

static Result run_list(ListGraph *g, const Ops *o, int print_adj) {
    Result r = {0, 0, 0, 0, 0, 0};
    r.memory = list_memory(g);         /* 연산 전 상태 기준 */
    list_insert(g, o->ins_u, o->ins_v, &r.ins);
    list_delete(g, o->del_u, o->del_v, &r.del);
    r.connected = list_contains(g, o->con_u, o->con_v, &r.con);
    if (print_adj) printf("  [List]   neighbors of %d: ", o->adj_w);
    list_adjacent(g, o->adj_w, &r.adj, print_adj);
    if (print_adj) printf("\n");
    return r;
}

static void print_case(int no, const char *gname, const char *rep, const Result *r) {
    printf("Case %d : %s Graph - %s\n", no, gname, rep);
    printf("Memory                         : %zu Bytes\n", r->memory);
    printf("Edge insertion comparisons     : %ld\n", r->ins);
    printf("Edge deletion comparisons      : %ld\n", r->del);
    printf("Connectivity check comparisons : %ld\n", r->con);
    printf("Adjacent vertices comparisons  : %ld\n\n", r->adj);
}

/* 보충 실험: 같은 그래프에서 무작위 연산을 TRIALS번 반복한 평균
 * (삽입/삭제 후에는 원래 상태로 되돌려 매 시도가 같은 그래프에서 시작) */
static void average_trials(Truth *t, MatGraph *mg, ListGraph *lg,
                           double avg_m[4], double avg_l[4]) {
    long sm[4] = {0, 0, 0, 0}, sl[4] = {0, 0, 0, 0};
    for (int i = 0; i < TRIALS; i++) {
        long d = 0;
        int u, v;
        /* 삽입: 없는 간선을 넣고(측정) 바로 지움(미측정) */
        do { u = rnd(V); v = rnd(V); } while (u == v || t->used[u][v]);
        mat_insert(mg, u, v, &sm[0]);  mat_delete(mg, u, v, &d);
        list_insert(lg, u, v, &sl[0]); list_delete(lg, u, v, &d);
        /* 삭제: 있는 간선을 지우고(측정) 다시 넣음(미측정) */
        int k = rnd(t->count);
        u = t->edges[k].u; v = t->edges[k].v;
        mat_delete(mg, u, v, &sm[1]);  mat_insert(mg, u, v, &d);
        list_delete(lg, u, v, &sl[1]); list_insert(lg, u, v, &d);
        /* 연결 확인 */
        do { u = rnd(V); v = rnd(V); } while (u == v);
        mat_has(mg, u, v, &sm[2]);
        list_contains(lg, u, v, &sl[2]);
        /* 인접 정점 */
        int w = rnd(V);
        mat_adjacent(mg, w, &sm[3], 0);
        list_adjacent(lg, w, &sl[3], 0);
    }
    for (int i = 0; i < 4; i++) {
        avg_m[i] = (double)sm[i] / TRIALS;
        avg_l[i] = (double)sl[i] / TRIALS;
    }
}

/* 보충 실험 2: 연결 확인을 연결된 쌍 / 연결 안 된 쌍으로 나눠 TRIALS번씩 측정
 * (읽기만 하므로 그래프는 바뀌지 않음) */
static void connect_split(const Truth *t, const MatGraph *mg, const ListGraph *lg,
                          double cm[2], double cl[2]) {
    long ym = 0, yl = 0, nm = 0, nl = 0;
    for (int i = 0; i < TRIALS; i++) {
        /* 연결된 쌍: 실제 간선 하나를 고르고 방향도 무작위 */
        int k = rnd2(t->count);
        int u = t->edges[k].u, v = t->edges[k].v;
        if (rnd2(2)) { int tmp = u; u = v; v = tmp; }
        mat_has(mg, u, v, &ym);
        list_contains(lg, u, v, &yl);
        /* 연결 안 된 쌍 */
        do { u = rnd2(V); v = rnd2(V); } while (u == v || t->used[u][v]);
        mat_has(mg, u, v, &nm);
        list_contains(lg, u, v, &nl);
    }
    cm[0] = (double)ym / TRIALS; cm[1] = (double)nm / TRIALS;
    cl[0] = (double)yl / TRIALS; cl[1] = (double)nl / TRIALS;
}

int main(int argc, char *argv[]) {
    unsigned seed = (argc > 1) ? (unsigned)strtoul(argv[1], NULL, 10) : 20261017u;
    rng_state = seed ? seed : 1;
    rng2_state = (seed ^ 0x9E3779B9u) ? (seed ^ 0x9E3779B9u) : 1;

    const char *names[2] = {"Sparse", "Dense"};
    int edges[2] = {SPARSE_E, DENSE_E};
    double avg_m[2][4], avg_l[2][4];
    double con_m[2][2], con_l[2][2];

    printf("seed = %u, vertices = %d\n\n", seed, V);

    for (int g = 0; g < 2; g++) {
        Truth t;
        MatGraph *mg = (MatGraph *)malloc(sizeof(MatGraph));
        ListGraph *lg = (ListGraph *)malloc(sizeof(ListGraph));
        generate(&t, edges[g]);
        build(&t, mg, lg);

        if (!verify(mg, lg)) { printf("ERROR: 두 표현이 다릅니다.\n"); return 1; }
        printf("== %s graph: V=%d, E=%d (두 표현 동일 확인 OK) ==\n", names[g], V, t.count);

        Ops o;
        pick_ops(&t, &o);
        printf("  insert (%d,%d), delete (%d,%d), connect? (%d,%d), adjacent of %d\n",
               o.ins_u, o.ins_v, o.del_u, o.del_v, o.con_u, o.con_v, o.adj_w);

        Result rm = run_mat(mg, &o, 1);
        Result rl = run_list(lg, &o, 1);
        printf("  connected? (%d,%d): Matrix=%s, List=%s\n", o.con_u, o.con_v,
               rm.connected ? "YES" : "NO", rl.connected ? "YES" : "NO");
        printf("\n");
        print_case(2 * g + 1, names[g], "Adjacency Matrix", &rm);
        print_case(2 * g + 2, names[g], "Adjacency List", &rl);

        apply_truth(&t, &o);
        if (!verify(mg, lg)) { printf("ERROR: 연산 후 두 표현이 다릅니다.\n"); return 1; }

        average_trials(&t, mg, lg, avg_m[g], avg_l[g]);
        connect_split(&t, mg, lg, con_m[g], con_l[g]);
        if (!verify(mg, lg)) { printf("ERROR: 보충 실험 후 두 표현이 다릅니다.\n"); return 1; }

        list_free(lg);
        free(lg); free(mg); free(t.edges);
    }

    printf("== 보충: 무작위 연산 %d회 평균 비교 횟수 ==\n", TRIALS);
    printf("%-8s %-8s %10s %10s %10s %10s %10s %10s\n", "Graph", "Repr", "Insert", "Delete",
           "Connect", "Conn(Y)", "Conn(N)", "Adjacent");
    for (int g = 0; g < 2; g++) {
        printf("%-8s %-8s %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n", names[g], "Matrix",
               avg_m[g][0], avg_m[g][1], avg_m[g][2], con_m[g][0], con_m[g][1], avg_m[g][3]);
        printf("%-8s %-8s %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n", names[g], "List",
               avg_l[g][0], avg_l[g][1], avg_l[g][2], con_l[g][0], con_l[g][1], avg_l[g][3]);
    }
    printf("  (Connect: 무작위 쌍, Conn(Y): 연결된 쌍만, Conn(N): 연결 안 된 쌍만)\n");
    return 0;
}