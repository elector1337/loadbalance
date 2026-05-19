#include "ld.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// бенчмарк: гоняет все 5 алгоритмов на одном инстансе, печатает таблицу
// usage: bench <input> [--csv]
// время мерится через clock_gettime (CLOCK_MONOTONIC)

typedef int (*algo_fn)(const Instance *, Solution *);

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int run_local(const Instance *inst, Solution *s) {
    // локальному поиску нужен стартовый план: берём LPT
    if (!solve_greedy_lpt(inst, s)) return 0;
    return solve_local_search(inst, s);
}

static int run_multifit(const Instance *inst, Solution *s) {
    return solve_multifit(inst, s, 40);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <input> [--csv]\n", argv[0]);
        return 1;
    }
    int csv = (argc >= 3 && strcmp(argv[2], "--csv") == 0);

    Instance *inst = instance_read(argv[1]);
    if (!inst) return 1;

    long lb = lower_bound_best(inst);

    struct { const char *name; algo_fn fn; } algos[] = {
        { "greedy",   solve_greedy_simple },
        { "lpt",      solve_greedy_lpt    },
        { "multifit", run_multifit        },
        { "lp",       solve_lp_style      },
        { "local",    run_local           },
    };
    int na = (int)(sizeof(algos) / sizeof(algos[0]));

    if (csv) {
        printf("algo,makespan,lower_bound,ratio,time_ms,legal\n");
    } else {
        printf("%-10s %12s %12s %8s %10s %6s\n", "algo", "makespan", "lower_bound", "ratio", "time_ms", "legal");
    }

    for (int k = 0; k < na; ++k) {
        Solution *s = solution_new(inst);
        if (!s) { instance_free(inst); return 1; }

        double t0 = now_ms();
        int ok = algos[k].fn(inst, s);
        double t1 = now_ms();

        long mk = ok ? s->makespan : -1;
        double ratio = (ok && lb > 0) ? (double)mk / (double)lb : 0.0;
        int legal = ok && solution_is_legal(s, inst);

        if (csv) {
            printf("%s,%ld,%ld,%.4f,%.3f,%d\n", algos[k].name, mk, lb, ratio, t1 - t0, legal);
        } else {
            printf("%-10s %12ld %12ld %8.4f %10.3f %6s\n",
                   algos[k].name, mk, lb, ratio, t1 - t0, legal ? "yes" : "NO");
        }
        solution_free(s);
    }
    instance_free(inst);
    return 0;
}
