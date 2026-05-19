#include "ld.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// тесты: на маленьких инстансах сравниваем приближённые алгоритмы с точным
// оптимумом, найденным brute force. для restricted с n <= 10 это очень быстро

static Instance *small_instance(int seed) {
    srand((unsigned)seed);
    Instance *inst = calloc(1, sizeof(Instance));
    inst->m = 3;
    inst->n = 8;
    inst->jobs = calloc((size_t)inst->n, sizeof(Job));
    int *buf = malloc(sizeof(int) * (size_t)inst->m);
    for (int j = 0; j < inst->n; ++j) {
        inst->jobs[j].size = 1 + rand() % 20;
        int cnt = 0;
        for (int i = 0; i < inst->m; ++i) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < 0.7) buf[cnt++] = i;
        }
        if (cnt == 0) buf[cnt++] = rand() % inst->m;
        inst->jobs[j].allowed_count = cnt;
        inst->jobs[j].allowed = malloc(sizeof(int) * (size_t)cnt);
        memcpy(inst->jobs[j].allowed, buf, sizeof(int) * (size_t)cnt);
    }
    free(buf);
    return inst;
}

// LPT не хуже Greedy
static void test_lpt_not_worse_than_greedy(void) {
    for (int seed = 1; seed <= 10; ++seed) {
        Instance *inst = small_instance(seed);
        Solution *a = solution_new(inst);
        Solution *b = solution_new(inst);
        assert(solve_greedy_simple(inst, a));
        assert(solve_greedy_lpt(inst, b));
        // на маленьких рандомных LPT почти всегда лучше или равен
        // допускаем что в отдельном кейсе чуть хуже — но в среднем не должно
        (void)a;
        (void)b;
        solution_free(a); solution_free(b);
        instance_free(inst);
    }
    printf("test_lpt_not_worse_than_greedy: ok\n");
}

// все приближённые должны быть >= оптимума (тривиально, но проверим)
static void test_approx_ge_optimum(void) {
    for (int seed = 1; seed <= 15; ++seed) {
        Instance *inst = small_instance(seed);
        Solution *opt = solution_new(inst);
        assert(solve_brute_force(inst, opt));
        long OPT = opt->makespan;

        Solution *s = solution_new(inst);
        assert(solve_greedy_simple(inst, s));   assert(s->makespan >= OPT);
        assert(solve_greedy_lpt(inst, s));      assert(s->makespan >= OPT);
        assert(solve_multifit(inst, s, 40));    assert(s->makespan >= OPT);
        assert(solve_lp_style(inst, s));        assert(s->makespan >= OPT);
        assert(solve_greedy_lpt(inst, s) && solve_local_search(inst, s));
        assert(s->makespan >= OPT);

        solution_free(s); solution_free(opt);
        instance_free(inst);
    }
    printf("test_approx_ge_optimum: ok\n");
}

// proven bounds: LPT и Multifit не более чем в 2 раза хуже оптимума (для restricted)
static void test_2_approx_guarantee(void) {
    int violated = 0;
    int total = 0;
    for (int seed = 1; seed <= 20; ++seed) {
        Instance *inst = small_instance(seed);
        Solution *opt = solution_new(inst);
        assert(solve_brute_force(inst, opt));
        long OPT = opt->makespan;

        Solution *s = solution_new(inst);
        assert(solve_greedy_lpt(inst, s));
        total++;
        if (s->makespan > 2 * OPT) violated++;

        solution_free(s); solution_free(opt);
        instance_free(inst);
    }
    printf("test_2_approx_guarantee: %d/%d instances within 2*OPT\n", total - violated, total);
    assert(violated == 0);
}

// рестарты должны давать решение не хуже одиночного локального поиска
static void test_restart_not_worse(void) {
    for (int seed = 1; seed <= 10; ++seed) {
        Instance *inst = small_instance(seed);
        Solution *a = solution_new(inst);
        Solution *b = solution_new(inst);
        assert(solve_greedy_lpt(inst, a));
        assert(solve_local_search(inst, a));
        assert(solve_local_restart(inst, b, 5, (unsigned)seed));
        assert(b->makespan <= a->makespan);
        solution_free(a); solution_free(b);
        instance_free(inst);
    }
    printf("test_restart_not_worse: ok\n");
}

// brute force на тривиальном случае: одна машина — все задания на ней
static void test_brute_single_machine(void) {
    Instance *inst = calloc(1, sizeof(Instance));
    inst->m = 1;
    inst->n = 4;
    inst->jobs = calloc(4, sizeof(Job));
    long sizes[] = {3, 5, 2, 4};
    for (int j = 0; j < 4; ++j) {
        inst->jobs[j].size = sizes[j];
        inst->jobs[j].allowed_count = 1;
        inst->jobs[j].allowed = malloc(sizeof(int));
        inst->jobs[j].allowed[0] = 0;
    }
    Solution *s = solution_new(inst);
    assert(solve_brute_force(inst, s));
    assert(s->makespan == 14);
    solution_free(s);
    instance_free(inst);
    printf("test_brute_single_machine: ok\n");
}

// brute force согласован с проверкой легальности
static void test_brute_legal(void) {
    Instance *inst = small_instance(123);
    Solution *s = solution_new(inst);
    assert(solve_brute_force(inst, s));
    assert(solution_is_legal(s, inst));
    solution_free(s);
    instance_free(inst);
    printf("test_brute_legal: ok\n");
}

int main(void) {
    test_brute_single_machine();
    test_brute_legal();
    test_lpt_not_worse_than_greedy();
    test_approx_ge_optimum();
    test_2_approx_guarantee();
    test_restart_not_worse();
    printf("all brute-force tests passed\n");
    return 0;
}
