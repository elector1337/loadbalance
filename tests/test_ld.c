#include "ld.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// маленькая утилита: построить инстанс в памяти
static Instance *make_instance(int m, int n) {
    Instance *inst = calloc(1, sizeof(Instance));
    inst->m = m; inst->n = n;
    inst->jobs = calloc((size_t)n, sizeof(Job));
    return inst;
}

static void set_job(Instance *inst, int j, long size, int count, const int *allowed) {
    inst->jobs[j].size = size;
    inst->jobs[j].allowed_count = count;
    inst->jobs[j].allowed = malloc(sizeof(int) * (size_t)count);
    memcpy(inst->jobs[j].allowed, allowed, sizeof(int) * (size_t)count);
}

// одна машина: все задания идут на неё
static void test_single_machine(void) {
    Instance *inst = make_instance(1, 3);
    int a[] = {0};
    set_job(inst, 0, 3, 1, a);
    set_job(inst, 1, 5, 1, a);
    set_job(inst, 2, 2, 1, a);

    Solution *s = solution_new(inst);
    assert(solve_greedy_lpt(inst, s));
    assert(s->makespan == 10);

    solution_free(s);
    instance_free(inst);
    printf("test_single_machine: ok\n");
}

// две машины, симметрия: оптимум 5
static void test_two_machines_balanced(void) {
    Instance *inst = make_instance(2, 4);
    int both[] = {0, 1};
    set_job(inst, 0, 4, 2, both);
    set_job(inst, 1, 3, 2, both);
    set_job(inst, 2, 2, 2, both);
    set_job(inst, 3, 1, 2, both);

    Solution *s = solution_new(inst);
    assert(solve_greedy_lpt(inst, s));
    // LPT даёт оптимум 5 на этом примере
    assert(s->makespan == 5);

    solution_free(s);
    instance_free(inst);
    printf("test_two_machines_balanced: ok\n");
}

// ограничение: задание 0 может только на машину 1, проверяем что greedy уважает M_j
static void test_restricted(void) {
    Instance *inst = make_instance(2, 2);
    int only1[] = {1};
    int both[]  = {0, 1};
    set_job(inst, 0, 10, 1, only1);
    set_job(inst, 1, 5,  2, both);

    Solution *s = solution_new(inst);
    assert(solve_greedy_simple(inst, s));
    assert(s->assign[0] == 1);
    // задание 1 пойдёт на машину 0 (она пустая), makespan = 10
    assert(s->makespan == 10);

    solution_free(s);
    instance_free(inst);
    printf("test_restricted: ok\n");
}

// нет допустимой машины — задание не должно назначиться
static void test_infeasible(void) {
    Instance *inst = make_instance(2, 1);
    int none[] = {0};
    set_job(inst, 0, 1, 1, none);
    inst->jobs[0].allowed_count = 0;
    free(inst->jobs[0].allowed);
    inst->jobs[0].allowed = NULL;

    Solution *s = solution_new(inst);
    int ok = solve_greedy_simple(inst, s);
    assert(!ok);

    solution_free(s);
    instance_free(inst);
    printf("test_infeasible: ok\n");
}

// локальный поиск не должен ухудшать решение
static void test_local_search_monotone(void) {
    Instance *inst = make_instance(3, 6);
    int all3[] = {0, 1, 2};
    long sizes[] = {7, 6, 5, 4, 3, 2};
    for (int j = 0; j < 6; ++j) set_job(inst, j, sizes[j], 3, all3);

    Solution *s = solution_new(inst);
    assert(solve_greedy_simple(inst, s));
    long before = s->makespan;
    assert(solve_local_search(inst, s));
    assert(s->makespan <= before);

    solution_free(s);
    instance_free(inst);
    printf("test_local_search_monotone: ok\n");
}

// multifit должен быть не хуже LPT на одном инстансе
static void test_multifit_not_worse(void) {
    Instance *inst = make_instance(3, 7);
    int all3[] = {0, 1, 2};
    long sizes[] = {8, 7, 6, 5, 4, 3, 2};
    for (int j = 0; j < 7; ++j) set_job(inst, j, sizes[j], 3, all3);

    Solution *a = solution_new(inst);
    Solution *b = solution_new(inst);
    assert(solve_greedy_lpt(inst, a));
    assert(solve_multifit(inst, b, 40));
    assert(b->makespan <= a->makespan);

    solution_free(a); solution_free(b);
    instance_free(inst);
    printf("test_multifit_not_worse: ok\n");
}

int main(void) {
    test_single_machine();
    test_two_machines_balanced();
    test_restricted();
    test_infeasible();
    test_local_search_monotone();
    test_multifit_not_worse();
    printf("all tests passed\n");
    return 0;
}
