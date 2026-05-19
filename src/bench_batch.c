#include "ld.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// пакетный бенчмарк: гоняет 5 алгоритмов на N случайных инстансах с разными сидами
// и печатает агрегированную статистику (среднее, мин, макс отношения makespan/LB)
//
// usage: bench_batch <m> <n> <tmin> <tmax> <density> <runs>
// пример: bench_batch 5 50 1 100 0.6 30

typedef int (*algo_fn)(const Instance *, Solution *);

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static Instance *generate(int m, int n, int tmin, int tmax, double density) {
    Instance *inst = calloc(1, sizeof(Instance));
    inst->m = m; inst->n = n;
    inst->jobs = calloc((size_t)n, sizeof(Job));
    int *buf = malloc(sizeof(int) * (size_t)m);
    for (int j = 0; j < n; ++j) {
        inst->jobs[j].size = tmin + rand() % (tmax - tmin + 1);
        int cnt = 0;
        for (int i = 0; i < m; ++i) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < density) buf[cnt++] = i;
        }
        if (cnt == 0) buf[cnt++] = rand() % m;
        inst->jobs[j].allowed_count = cnt;
        inst->jobs[j].allowed = malloc(sizeof(int) * (size_t)cnt);
        memcpy(inst->jobs[j].allowed, buf, sizeof(int) * (size_t)cnt);
    }
    free(buf);
    return inst;
}

static int run_local(const Instance *inst, Solution *s) {
    if (!solve_greedy_lpt(inst, s)) return 0;
    return solve_local_search(inst, s);
}
static int run_multifit(const Instance *inst, Solution *s) { return solve_multifit(inst, s, 40); }
static int run_restart(const Instance *inst, Solution *s)  { return solve_local_restart(inst, s, 8, 1); }

typedef struct {
    const char *name;
    algo_fn fn;
    double ratio_sum, ratio_min, ratio_max;
    double time_sum;
    int wins, runs;
} Stats;

int main(int argc, char **argv) {
    if (argc != 7 && argc != 8) {
        fprintf(stderr, "usage: %s <m> <n> <tmin> <tmax> <density> <runs> [--csv]\n", argv[0]);
        return 1;
    }
    int csv = (argc == 8 && strcmp(argv[7], "--csv") == 0);
    int m = atoi(argv[1]);
    int n = atoi(argv[2]);
    int tmin = atoi(argv[3]);
    int tmax = atoi(argv[4]);
    double density = atof(argv[5]);
    int runs = atoi(argv[6]);
    if (m <= 0 || n <= 0 || tmin <= 0 || tmax < tmin || density <= 0 || runs <= 0) {
        fprintf(stderr, "bad args\n");
        return 1;
    }

    Stats S[] = {
        { "greedy",   solve_greedy_simple, 0, 1e9, 0, 0, 0, 0 },
        { "lpt",      solve_greedy_lpt,    0, 1e9, 0, 0, 0, 0 },
        { "multifit", run_multifit,        0, 1e9, 0, 0, 0, 0 },
        { "lp",       solve_lp_style,      0, 1e9, 0, 0, 0, 0 },
        { "local",    run_local,           0, 1e9, 0, 0, 0, 0 },
        { "restart",  run_restart,         0, 1e9, 0, 0, 0, 0 },
    };
    int na = (int)(sizeof(S) / sizeof(S[0]));

    if (!csv) {
        printf("running %d instances of m=%d n=%d t=[%d,%d] density=%.2f ...\n", runs, m, n, tmin, tmax, density);
    }

    for (int r = 0; r < runs; ++r) {
        srand((unsigned)(r * 1234567u + 1));
        Instance *inst = generate(m, n, tmin, tmax, density);
        long lb = lower_bound_best(inst);

        long best_make = LONG_MAX;
        long mks[16] = {0};
        for (int k = 0; k < na; ++k) {
            Solution *s = solution_new(inst);
            double t0 = now_ms();
            int ok = S[k].fn(inst, s);
            double t1 = now_ms();
            if (ok && solution_is_legal(s, inst)) {
                double ratio = lb > 0 ? (double)s->makespan / (double)lb : 0.0;
                S[k].ratio_sum += ratio;
                if (ratio < S[k].ratio_min) S[k].ratio_min = ratio;
                if (ratio > S[k].ratio_max) S[k].ratio_max = ratio;
                S[k].time_sum += t1 - t0;
                S[k].runs++;
                mks[k] = s->makespan;
                if (s->makespan < best_make) best_make = s->makespan;
            } else {
                mks[k] = LONG_MAX;
            }
            solution_free(s);
        }
        // победителем считаем алгоритм с минимальным makespan на этом инстансе
        for (int k = 0; k < na; ++k) {
            if (mks[k] == best_make) S[k].wins++;
        }
        instance_free(inst);
    }

    if (csv) {
        printf("algo,avg_ratio,min_ratio,max_ratio,avg_ms,wins,runs\n");
        for (int k = 0; k < na; ++k) {
            double avg_r = S[k].runs > 0 ? S[k].ratio_sum / S[k].runs : 0.0;
            double avg_t = S[k].runs > 0 ? S[k].time_sum  / S[k].runs : 0.0;
            printf("%s,%.6f,%.6f,%.6f,%.6f,%d,%d\n",
                   S[k].name, avg_r, S[k].ratio_min, S[k].ratio_max, avg_t, S[k].wins, S[k].runs);
        }
    } else {
        printf("\n%-10s %8s %8s %8s %10s %8s\n", "algo", "avg_r", "min_r", "max_r", "avg_ms", "wins");
        for (int k = 0; k < na; ++k) {
            double avg_r = S[k].runs > 0 ? S[k].ratio_sum / S[k].runs : 0.0;
            double avg_t = S[k].runs > 0 ? S[k].time_sum  / S[k].runs : 0.0;
            printf("%-10s %8.4f %8.4f %8.4f %10.3f %8d\n",
                   S[k].name, avg_r, S[k].ratio_min, S[k].ratio_max, avg_t, S[k].wins);
        }
    }
    return 0;
}
