# 과제 09: 모든 정점 쌍의 최단경로 탐색

정점 10개, 간선 20개의 가중치 무방향 연결 그래프를 무작위로 생성하고, Floyd-Warshall 알고리즘으로 서로 다른 정점 쌍 45개의 최단경로와 최단거리를 구해 출력함.

## 1. 빌드 및 실행

```bash
gcc -Wall -Wextra -std=c11 -o ShortestPath ShortestPath.c
./ShortestPath            # 시드를 현재 시각으로 사용 (실행할 때마다 다른 그래프)
./ShortestPath 20261017   # 시드 고정 (Output.txt와 같은 결과)
```

- 난수는 직접 구현한 xorshift32를 사용함. 같은 시드이면 OS나 컴파일러와 관계없이 같은 그래프가 나옴.
- `Output.txt`는 시드 `20261017`로 실행한 전체 결과임.

## 2. 구현 요약

**그래프 생성 (인접 행렬 + 간선 목록)**

- 정점을 무작위로 섞은 뒤 `perm[i]`를 `perm[0..i-1]` 중 하나와 연결해 신장 트리 간선 9개를 먼저 만듦. 이 단계만으로 항상 연결 그래프가 됨.
- 나머지 11개는 무작위로 고르되 자기 자신 연결(`u == v`)과 이미 있는 간선(`adj[u][v] != 0`)은 건너뜀.
- 가중치는 1~100의 임의 정수임. 간선 목록은 출력 전에 `(u, v)` 순으로 정렬함.

**Floyd-Warshall + 경로 복원**

- `dist[i][j]`: 최단거리, `next_v[i][j]`: i에서 j로 갈 때 i 바로 다음에 방문할 정점.
- 초기값: 직접 연결된 간선이면 `dist = 가중치`, `next_v = j`, 없으면 `INF` / `NONE`.
- 정점 k를 경유하는 것이 더 짧으면 `dist[i][j]`를 갱신하고 `next_v[i][j] = next_v[i][k]`로 바꿈. i→j의 첫 걸음이 i→k의 첫 걸음과 같아지기 때문임.
- 경로 복원은 `s`에서 시작해 `s = next_v[s][t]`를 t에 도달할 때까지 반복함.

```
예) 0 -> 2 (Output.txt)
next_v[0][2] = 9  →  next_v[9][2] = 7  →  next_v[7][2] = 2
Path : 0 -> 9 -> 7 -> 2,  Distance : 19 + 7 + 23 = 49
```

## 3. 출력 형식

과제 6번의 출력 형식을 따름. 다음 두 가지를 추가함.

- `Distance` 줄 뒤에 간선이 2개 이상인 경로의 가중치 합산 과정을 괄호로 표시함 (예: `Distance : 44 (24 + 20)`).
- 마지막에 `Check` 항목으로 과제 7번 확인 사항을 프로그램이 직접 검사한 결과를 출력함.

```
Check
Vertices          : 10 (OK)
Edges             : 20 (OK)
Weight 1 ~ 100    : 0 out of range (OK)
Self-loops        : 0 (OK)
Duplicate edges   : 0 (OK)
Connected         : Yes (OK)
Vertex pairs      : 45 (OK)
Path sum = Dist   : 45 / 45 (OK)
```

- `Connected`는 DFS로 정점 0에서 모든 정점에 도달하는지 확인함.
- `Path sum = Dist`는 복원한 경로를 따라 인접 행렬의 가중치를 다시 더한 값이 `dist[s][t]`와 같은 쌍의 수임.

## 4. 간선 수가 적다고 최단경로가 아님

`Output.txt`의 2 → 5는 직접 연결된 간선(가중치 56)이 있지만, 간선 3개를 거치는 경로가 더 짧음.

```
2 -> 5
Path     : 2 -> 7 -> 8 -> 5
Distance : 49 (23 + 6 + 20)      ← 직접 간선 (2, 5) = 56 보다 짧음
```

2 → 9도 같은 경우임. 직접 간선은 75이지만 `2 -> 7 -> 9`는 23 + 7 = 30임.

## 5. 검증

- gcc(`-std=c11`)와 clang(`-std=c11`)에서 `-Wall -Wextra -Wpedantic -Werror`로 경고 없이 빌드됨.
- 시드 2,002개(0~1999, 20261017, 4294967295)로 실행한 결과를 Python으로 작성한 별도 Dijkstra와 비교함. 45쌍 모두 경로가 실제 간선으로 이어지고, 가중치 합 = 출력된 거리 = Dijkstra 최단거리임을 확인함 (불일치 0건).
- gcc 빌드와 clang 빌드의 출력이 시드 2,003개에서 모두 한 글자까지 같음을 확인함.
- AddressSanitizer / UndefinedBehaviorSanitizer 빌드로 여러 시드를 실행해 메모리 오류가 없음을 확인함.

**컴파일러마다 그래프가 달라졌던 문제 (수정함)**

- 처음에는 신장 트리 간선을 `add_edge(perm[i], perm[rand_range(0, i - 1)], rand_range(MIN_W, MAX_W))`처럼 함수 인자 안에서 난수를 두 번 뽑았음.
- C 표준은 함수 인자의 계산 순서를 정하지 않음. Linux gcc는 가중치를 먼저, macOS의 `gcc`(실제로는 Apple clang)는 정점을 먼저 뽑아서 같은 시드에서도 그래프가 달랐음.
- 두 난수를 각각 별도의 문장(`int j = ...; int w = ...;`)으로 뽑도록 고쳐서 어떤 컴파일러에서든 같은 그래프가 나오게 함.
