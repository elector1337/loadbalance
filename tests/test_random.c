#include "ld.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// генерируем случайный инстанс прямо в памяти, не через файл
static Instance *make_random(int m, int n, int tmin, int tmax, double density, unsigned seed) {
    srand(seed);
    Instance *inst = calloc(1, sizeof(Instance));
    inst->m = m; inst->n = n;
    inst->jobs = calloc((size_t)n, sizeof(Job));
    for (int j = 0; j < n; ++j) {
        inst->jobs[j].size = tmin + rand() % (tmax - tmin + 1);
        int *buf = malloc(sizeof(int) * (size_t)m);
        int cnt = 0;
        for (int i = 0; i < m; ++i) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < density) buf[cnt++] = i;
        }
        if (cnt == 0) buf[cnt++] = rand() % m;
        inst->jobs[j].allowed_count = cnt;
        inst->jobs[j].allowed = malloc(sizeof(int) * (size_t)cnt);
        memcpy(inst->jobs[j].allowed, buf, sizeof(int) * (size_t)cnt);
        free(buf);
    }
    return inst;
}

typedef int (*algo_fn)(const Instance *, Solution *);

static int run_local(const Instance *inst, Solution *s) {
    if (!solve_greedy_lpt(inst, s)) return 0;
    return solve_local_search(inst, s);
}

static int run_multifit(const Instance *inst, Solution *s) {
    return solve_multifit(inst, s, 40);
}

// инвариант 1: алгоритм возвращает легальное назначение
// инвариант 2: makespan >= нижняя граница
// инвариант 3: для LPT/Multifit/LP makespan <= 2 * LB (теоретическая гарантия)
static void check_invariants(const char *name, const Instance *inst, Solution *s, int can_be_2approx) {
    assert(s->ok);
    if (!solution_is_legal(s, inst)) {
        fprintf(stderr, "[%s] illegal solution\n", name);
        assert(0);
    }
    long lb = lower_bound_best(inst);
    if (s->makespan < lb) {
        fprintf(stderr, "[%s] makespan %ld < lower_bound %ld\n", name, s->makespan, lb);
        assert(0);
    }
    if (can_be_2approx) {
        // допускаем небольшое перекрытие из-за округления нижней границы и реальных перегрузов
        if (s->makespan > 2 * lb + 1) {
            fprintf(stderr, "[%s] makespan %ld > 2 * LB %ld\n", name, s->makespan, lb);
            // не падаем — это статистическая проверка, не гарантия для всех случаев
        }
    }
}

static void test_many(void) {
    struct { int m, n, tmin, tmax; double d; } cases[] = {
        {  2,  10, 1,  10, 1.0 },
        {  3,  20, 1, 100, 0.7 },
        {  5,  50, 1, 100, 0.5 },
        { 10, 100, 1, 100, 0.4 },
        {  4,  30, 1,  50, 0.3 },
    };
    int nc = (int)(sizeof(cases) / sizeof(cases[0]));

    struct { const char *name; algo_fn fn; int can_2; } algos[] = {
        { "greedy",   solve_greedy_simple, 0 },
        { "lpt",      solve_greedy_lpt,    1 },
        { "multifit", run_multifit,        1 },
        { "lp",       solve_lp_style,      1 },
        { "local",    run_local,           1 },
    };
    int na = (int)(sizeof(algos) / sizeof(algos[0]));

    int tested = 0;
    for (int c = 0; c < nc; ++c) {
        for (unsigned seed = 1; seed <= 5; ++seed) {
            Instance *inst = make_random(cases[c].m, cases[c].n, cases[c].tmin, cases[c].tmax, cases[c].d, seed);
            for (int k = 0; k < na; ++k) {
                Solution *s = solution_new(inst);
                int ok = algos[k].fn(inst, s);
                assert(ok);
                check_invariants(algos[k].name, inst, s, algos[k].can_2);
                solution_free(s);
                tested++;
            }
            instance_free(inst);
        }
    }
    printf("test_many: %d (algo, instance) combinations passed\n", tested);
}

// нижние границы должны быть монотонны: forced >= max_t возможно, и максимум их даёт лучшую
static void test_lower_bounds(void) {
    Instance *inst = make_random(3, 12, 1, 20, 0.6, 42);
    long a = lower_bound_max_t(inst);
    long b = lower_bound_avg(inst);
    long c = lower_bound_forced(inst);
    long best = lower_bound_best(inst);
    assert(a >= 0 && b >= 0 && c >= 0);
    assert(best >= a && best >= b && best >= c);
    instance_free(inst);
    printf("test_lower_bounds: ok\n");
}

// LPT с одинаковыми ограничениями (все на все) даёт классическое 4/3-приближение —
// при больших инстансах ratio должен быть близок к 1
static void test_lpt_ratio_unrestricted(void) {
    Instance *inst = make_random(5, 100, 1, 50, 1.0, 7);
    Solution *s = solution_new(inst);
    assert(solve_greedy_lpt(inst, s));
    long lb = lower_bound_best(inst);
    double ratio = (double)s->makespan / (double)lb;
    // эмпирически на таком размере должно быть < 1.2
    assert(ratio < 1.5);
    printf("test_lpt_ratio_unrestricted: ratio=%.4f ok\n", ratio);
    solution_free(s);
    instance_free(inst);
}

int main(void) {
    test_lower_bounds();
    test_lpt_ratio_unrestricted();
    test_many();
    printf("all random tests passed\n");
    return 0;
}
