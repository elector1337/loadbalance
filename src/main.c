#include "ld.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// выводим результат: имя алгоритма, makespan и нагрузку каждой машины
static void print_solution(const char *name, const Instance *inst, Solution *s) {
    if (!s->ok) { printf("%-12s : INFEASIBLE\n", name); return; }
    printf("%-12s : makespan=%ld | loads:", name, s->makespan);
    for (int i = 0; i < inst->m; ++i) printf(" %ld", s->load[i]);
    printf("\n");
}

// запускаем один алгоритм и печатаем его результат
static int run_one(const Instance *inst, const char *algo) {
    Solution *s = solution_new(inst);
    if (!s) return 1;
    int ok = 0;
    // выбираем нужный алгоритм по имени
    if      (!strcmp(algo, "greedy"))   ok = solve_greedy_simple(inst, s);
    else if (!strcmp(algo, "lpt"))      ok = solve_greedy_lpt(inst, s);
    else if (!strcmp(algo, "multifit")) ok = solve_multifit(inst, s, 40);
    else if (!strcmp(algo, "lp"))       ok = solve_lp_style(inst, s);
    else if (!strcmp(algo, "local")) {
        if (!solve_greedy_lpt(inst, s)) { solution_free(s); return 1; }
        ok = solve_local_search(inst, s);
    } else {
        fprintf(stderr, "unknown algo: %s\n", algo);
        solution_free(s);
        return 1;
    }
    (void)ok;
    print_solution(algo, inst, s);
    solution_free(s);
    return 0;
}

// точка входа: читаем файл и запускаем нужный алгоритм (или все сразу)
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <input> [algo]\n  algo: all|greedy|lpt|multifit|lp|local\n", argv[0]);
        return 1;
    }
    Instance *inst = instance_read(argv[1]);
    if (!inst) return 1;

    // по умолчанию запускаем все алгоритмы
    const char *algo = (argc >= 3) ? argv[2] : "all";

    if (!strcmp(algo, "all")) {
        const char *names[] = {"greedy", "lpt", "multifit", "lp", "local"};
        for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) run_one(inst, names[i]);
    } else {
        run_one(inst, algo);
    }
    instance_free(inst);
    return 0;
}
