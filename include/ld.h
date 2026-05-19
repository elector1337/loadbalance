#ifndef LD_H
#define LD_H

#include <stddef.h>

// одна задача: размер и список допустимых машин
typedef struct {
    long size;          // время выполнения задания
    int  allowed_count; // сколько машин допустимо для этого задания
    int *allowed;       // массив индексов допустимых машин
} Job;

// входная задача целиком
typedef struct {
    int  m;    // количество машин
    int  n;    // количество заданий
    Job *jobs; // массив заданий
} Instance;

// результат работы алгоритма
typedef struct {
    int  *assign;   // assign[j] — машина для j-го задания
    long *load;     // load[i] — суммарная нагрузка машины i
    long  makespan; // максимальная нагрузка (это и надо минимизировать)
    int   ok;       // 1 если решение найдено, 0 если нет
} Solution;

Instance *instance_read(const char *path);
void      instance_free(Instance *inst);

Solution *solution_new(const Instance *inst);
void      solution_free(Solution *s);
long      solution_recompute(Solution *s, const Instance *inst);

int solve_greedy_simple(const Instance *inst, Solution *s);
int solve_greedy_lpt   (const Instance *inst, Solution *s);
int solve_multifit     (const Instance *inst, Solution *s, int iters);
int solve_lp_style     (const Instance *inst, Solution *s);
int solve_local_search (const Instance *inst, Solution *s);
int solve_local_restart(const Instance *inst, Solution *s, int restarts, unsigned seed);
int solve_brute_force  (const Instance *inst, Solution *s);

// нижние границы для оптимума makespan
long lower_bound_max_t(const Instance *inst);
long lower_bound_avg  (const Instance *inst);
long lower_bound_forced(const Instance *inst);
long lower_bound_best (const Instance *inst);

// проверка что решение легально (assign[j] in M_j, все назначены)
int  solution_is_legal(const Solution *s, const Instance *inst);

#endif
