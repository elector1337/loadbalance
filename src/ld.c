#include "ld.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// инстанс

// читаем задачу из файла: сначала m и n, потом каждое задание
Instance *instance_read(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen"); return NULL; }

    Instance *inst = calloc(1, sizeof(Instance));
    if (!inst) { fclose(f); return NULL; }

    // первая строка: количество машин и заданий
    if (fscanf(f, "%d %d", &inst->m, &inst->n) != 2 || inst->m <= 0 || inst->n < 0) {
        fprintf(stderr, "bad header\n");
        fclose(f); free(inst); return NULL;
    }

    inst->jobs = calloc((size_t)inst->n, sizeof(Job));
    if (!inst->jobs && inst->n > 0) { fclose(f); free(inst); return NULL; }

    for (int j = 0; j < inst->n; ++j) {
        Job *job = &inst->jobs[j];
        // читаем размер задания и сколько машин ему доступно
        if (fscanf(f, "%ld %d", &job->size, &job->allowed_count) != 2
            || job->size <= 0 || job->allowed_count <= 0) {
            fprintf(stderr, "bad job %d\n", j);
            instance_free(inst); fclose(f); return NULL;
        }
        job->allowed = malloc(sizeof(int) * (size_t)job->allowed_count);
        if (!job->allowed) { instance_free(inst); fclose(f); return NULL; }
        // читаем список допустимых машин для этого задания
        for (int k = 0; k < job->allowed_count; ++k) {
            if (fscanf(f, "%d", &job->allowed[k]) != 1
                || job->allowed[k] < 0 || job->allowed[k] >= inst->m) {
                fprintf(stderr, "bad machine in job %d\n", j);
                instance_free(inst); fclose(f); return NULL;
            }
        }
    }
    fclose(f);
    return inst;
}

// освобождаем всю память инстанса
void instance_free(Instance *inst) {
    if (!inst) return;
    if (inst->jobs) {
        for (int j = 0; j < inst->n; ++j) free(inst->jobs[j].allowed);
        free(inst->jobs);
    }
    free(inst);
}

// решение

// создаём пустое решение: assign[j] = -1, нагрузки нулевые
Solution *solution_new(const Instance *inst) {
    Solution *s = calloc(1, sizeof(Solution));
    if (!s) return NULL;
    s->assign = malloc(sizeof(int)  * (size_t)inst->n);
    s->load   = calloc((size_t)inst->m, sizeof(long));
    if ((!s->assign && inst->n > 0) || !s->load) { solution_free(s); return NULL; }
    for (int j = 0; j < inst->n; ++j) s->assign[j] = -1;
    s->ok = 0;
    s->makespan = 0;
    return s;
}

// освобождаем память решения
void solution_free(Solution *s) {
    if (!s) return;
    free(s->assign);
    free(s->load);
    free(s);
}

// пересчитываем нагрузки по машинам и находим makespan (максимум)
long solution_recompute(Solution *s, const Instance *inst) {
    for (int i = 0; i < inst->m; ++i) s->load[i] = 0;
    for (int j = 0; j < inst->n; ++j) {
        int i = s->assign[j];
        if (i < 0 || i >= inst->m) { s->ok = 0; s->makespan = LONG_MAX; return s->makespan; }
        s->load[i] += inst->jobs[j].size;
    }
    long mx = 0;
    for (int i = 0; i < inst->m; ++i) if (s->load[i] > mx) mx = s->load[i];
    s->makespan = mx;
    s->ok = 1;
    return mx;
}

// простой greedy

// каждое задание кидаем на самую незагруженную допустимую машину
int solve_greedy_simple(const Instance *inst, Solution *s) {
    for (int i = 0; i < inst->m; ++i) s->load[i] = 0;
    for (int j = 0; j < inst->n; ++j) {
        const Job *job = &inst->jobs[j];
        int best = -1; long best_load = 0;
        // выбираем самую незагруженную допустимую машину
        for (int k = 0; k < job->allowed_count; ++k) {
            int i = job->allowed[k];
            if (best == -1 || s->load[i] < best_load) { best = i; best_load = s->load[i]; }
        }
        if (best == -1) { s->ok = 0; return 0; }
        s->assign[j] = best;
        s->load[best] += job->size;
    }
    solution_recompute(s, inst);
    return s->ok;
}

// LPT: сортируем индексы по убыванию size, потом greedy

// глобальный массив размеров для компаратора qsort
static int *g_sizes_for_cmp;

// компаратор: сортировка по убыванию размера задания
static int cmp_desc_by_size(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    long da = g_sizes_for_cmp[ia], db = g_sizes_for_cmp[ib];
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}

// LPT: сначала сортируем задания по убыванию, потом жадно назначаем
int solve_greedy_lpt(const Instance *inst, Solution *s) {
    for (int i = 0; i < inst->m; ++i) s->load[i] = 0;
    int *order = malloc(sizeof(int) * (size_t)inst->n);
    int *sizes = malloc(sizeof(int) * (size_t)inst->n);
    if ((!order || !sizes) && inst->n > 0) { free(order); free(sizes); return 0; }
    for (int j = 0; j < inst->n; ++j) { order[j] = j; sizes[j] = (int)inst->jobs[j].size; }
    g_sizes_for_cmp = sizes;
    qsort(order, (size_t)inst->n, sizeof(int), cmp_desc_by_size);

    for (int idx = 0; idx < inst->n; ++idx) {
        int j = order[idx];
        const Job *job = &inst->jobs[j];
        int best = -1; long best_load = 0;
        for (int k = 0; k < job->allowed_count; ++k) {
            int i = job->allowed[k];
            if (best == -1 || s->load[i] < best_load) { best = i; best_load = s->load[i]; }
        }
        if (best == -1) { free(order); free(sizes); s->ok = 0; return 0; }
        s->assign[j] = best;
        s->load[best] += job->size;
    }
    free(order); free(sizes);
    solution_recompute(s, inst);
    return s->ok;
}

// проверка feasibility для multifit: можно ли уложить в бюджет C
// best-fit decreasing: задания по убыванию, каждое на самую *незагруженную*
// допустимую машину, в которую влезает. эмпирически на restricted работает
// лучше FFD (FFD может не найти укладку, которую видит best-fit)

// проверяем, можно ли назначить все задания не превысив бюджет C
static int feasible_at_C(const Instance *inst, long C, int *assign_out) {
    long *load = calloc((size_t)inst->m, sizeof(long));
    if (!load) return 0;
    int *order = malloc(sizeof(int) * (size_t)inst->n);
    int *sizes = malloc(sizeof(int) * (size_t)inst->n);
    if ((!order || !sizes) && inst->n > 0) { free(load); free(order); free(sizes); return 0; }
    for (int j = 0; j < inst->n; ++j) { order[j] = j; sizes[j] = (int)inst->jobs[j].size; }
    g_sizes_for_cmp = sizes;
    qsort(order, (size_t)inst->n, sizeof(int), cmp_desc_by_size);

    int ok = 1;
    for (int idx = 0; idx < inst->n && ok; ++idx) {
        int j = order[idx];
        const Job *job = &inst->jobs[j];
        int best = -1; long best_load = 0;
        for (int k = 0; k < job->allowed_count; ++k) {
            int i = job->allowed[k];
            // пропускаем машину если задание не влезет
            if (load[i] + job->size > C) continue;
            if (best == -1 || load[i] < best_load) { best = i; best_load = load[i]; }
        }
        if (best == -1) { ok = 0; break; }
        assign_out[j] = best;
        load[best] += job->size;
    }
    free(load); free(order); free(sizes);
    return ok;
}

// multifit: бинарный поиск по C + best-fit decreasing
int solve_multifit(const Instance *inst, Solution *s, int iters) {
    if (iters <= 0) iters = 40;
    long sum = 0, mx = 0;
    for (int j = 0; j < inst->n; ++j) { sum += inst->jobs[j].size; if (inst->jobs[j].size > mx) mx = inst->jobs[j].size; }

    // нижняя граница для бинпоиска — лучшая из max(t), avg, forced
    long lo = lower_bound_best(inst);
    if (lo < mx) lo = mx; // на всякий случай
    // верхняя — LPT-результат: точно достижимо за полиномиальное время
    long hi = sum;
    {
        Solution *tmp = solution_new(inst);
        if (tmp && solve_greedy_lpt(inst, tmp)) hi = tmp->makespan;
        if (tmp) solution_free(tmp);
    }
    if (hi < lo) hi = lo;

    int *try_assign = malloc(sizeof(int) * (size_t)inst->n);
    int *best_assign = malloc(sizeof(int) * (size_t)inst->n);
    if ((!try_assign || !best_assign) && inst->n > 0) { free(try_assign); free(best_assign); return 0; }

    int have_best = 0;
    // проверим саму верхнюю границу (LPT-решение) как baseline
    if (feasible_at_C(inst, hi, try_assign)) {
        memcpy(best_assign, try_assign, sizeof(int) * (size_t)inst->n);
        have_best = 1;
    }

    // бинарный поиск: сужаем диапазон [lo, hi]
    for (int it = 0; it < iters && lo < hi; ++it) {
        long mid = lo + (hi - lo) / 2;
        if (feasible_at_C(inst, mid, try_assign)) {
            memcpy(best_assign, try_assign, sizeof(int) * (size_t)inst->n);
            have_best = 1;
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (!have_best) {
        if (!feasible_at_C(inst, hi, best_assign)) { free(try_assign); free(best_assign); s->ok = 0; return 0; }
    }
    memcpy(s->assign, best_assign, sizeof(int) * (size_t)inst->n);
    free(try_assign); free(best_assign);
    solution_recompute(s, inst);
    if (!s->ok) return 0;

    // финальный 1-exchange polish: часто срезает ещё 0.5-1% сверху
    solve_local_search(inst, s);
    return s->ok;
}

// LP-style (упрощённая Shmoys-Tardos): бинпоиск + раунд через паросочетание
// идея: при заданном C отбрасываем недопустимые ребра (t_j > C), и пытаемся
// назначить задания так чтобы суммарная нагрузка по машине <= 2C
// для restricted assignment жадно по убыванию + ограничение C

// пробуем уложить задания при makespan = C, допуская нагрузку до 2C
static int try_round_at_C(const Instance *inst, long C, int *out) {
    long *load = calloc((size_t)inst->m, sizeof(long));
    if (!load) return 0;
    int *order = malloc(sizeof(int) * (size_t)inst->n);
    int *sizes = malloc(sizeof(int) * (size_t)inst->n);
    if ((!order || !sizes) && inst->n > 0) { free(load); free(order); free(sizes); return 0; }
    for (int j = 0; j < inst->n; ++j) { order[j] = j; sizes[j] = (int)inst->jobs[j].size; }
    g_sizes_for_cmp = sizes;
    qsort(order, (size_t)inst->n, sizeof(int), cmp_desc_by_size);

    int ok = 1;
    for (int idx = 0; idx < inst->n; ++idx) {
        int j = order[idx];
        const Job *job = &inst->jobs[j];
        // одиночное задание больше C: вообще нельзя получить C
        if (job->size > C) { ok = 0; break; }
        int best = -1; long best_load = 0;
        // допускаем перегруз до 2C (теорема Shmoys-Tardos)
        for (int k = 0; k < job->allowed_count; ++k) {
            int i = job->allowed[k];
            if (load[i] + job->size > 2 * C) continue;
            if (best == -1 || load[i] < best_load) { best = i; best_load = load[i]; }
        }
        if (best == -1) { ok = 0; break; }
        out[j] = best;
        load[best] += job->size;
    }
    free(load); free(order); free(sizes);
    return ok;
}

// LP-style: бинарный поиск + округление по Shmoys-Tardos
int solve_lp_style(const Instance *inst, Solution *s) {
    long sum = 0, mx = 0;
    for (int j = 0; j < inst->n; ++j) { sum += inst->jobs[j].size; if (inst->jobs[j].size > mx) mx = inst->jobs[j].size; }
    long lower = mx;
    if (sum / (inst->m > 0 ? inst->m : 1) > lower) lower = sum / inst->m;
    long lo = mx, hi = sum;
    if (hi < lo) hi = lo;

    int *try_a = malloc(sizeof(int) * (size_t)inst->n);
    int *best_a = malloc(sizeof(int) * (size_t)inst->n);
    if ((!try_a || !best_a) && inst->n > 0) { free(try_a); free(best_a); return 0; }

    int have = 0;
    // ищем минимальный C для которого раунд проходит
    for (int it = 0; it < 60 && lo < hi; ++it) {
        long mid = lo + (hi - lo) / 2;
        if (try_round_at_C(inst, mid, try_a)) {
            memcpy(best_a, try_a, sizeof(int) * (size_t)inst->n);
            have = 1;
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (!have) {
        if (!try_round_at_C(inst, hi, best_a)) { free(try_a); free(best_a); s->ok = 0; return 0; }
    }
    memcpy(s->assign, best_a, sizeof(int) * (size_t)inst->n);
    free(try_a); free(best_a);
    solution_recompute(s, inst);
    return s->ok;
}

// нижние границы

// нижняя граница: makespan >= самого большого задания
long lower_bound_max_t(const Instance *inst) {
    long mx = 0;
    for (int j = 0; j < inst->n; ++j) if (inst->jobs[j].size > mx) mx = inst->jobs[j].size;
    return mx;
}

// нижняя граница: makespan >= среднее (округление вверх)
long lower_bound_avg(const Instance *inst) {
    long sum = 0;
    for (int j = 0; j < inst->n; ++j) sum += inst->jobs[j].size;
    if (inst->m <= 0) return sum;
    // округление вверх
    return (sum + inst->m - 1) / inst->m;
}

// нижняя граница: задания с одной машиной обязаны идти на неё
long lower_bound_forced(const Instance *inst) {
    // задание у которого ровно одна допустимая машина — оно обязано пойти на неё
    long *load = calloc((size_t)inst->m, sizeof(long));
    if (!load) return 0;
    for (int j = 0; j < inst->n; ++j) {
        if (inst->jobs[j].allowed_count == 1) {
            int i = inst->jobs[j].allowed[0];
            load[i] += inst->jobs[j].size;
        }
    }
    long mx = 0;
    for (int i = 0; i < inst->m; ++i) if (load[i] > mx) mx = load[i];
    free(load);
    return mx;
}

// берём максимум из трёх нижних границ
long lower_bound_best(const Instance *inst) {
    long a = lower_bound_max_t(inst);
    long b = lower_bound_avg(inst);
    long c = lower_bound_forced(inst);
    long r = a;
    if (b > r) r = b;
    if (c > r) r = c;
    return r;
}

// проверка корректности решения

// проверяем что каждое задание назначено на допустимую машину
int solution_is_legal(const Solution *s, const Instance *inst) {
    if (!s || !s->assign) return 0;
    for (int j = 0; j < inst->n; ++j) {
        int i = s->assign[j];
        if (i < 0 || i >= inst->m) return 0;
        const Job *job = &inst->jobs[j];
        int ok = 0;
        for (int k = 0; k < job->allowed_count; ++k) {
            if (job->allowed[k] == i) { ok = 1; break; }
        }
        if (!ok) return 0;
    }
    return 1;
}

// локальный поиск: 1-exchange

// пробуем переставить одно задание на другую машину, если это уменьшает makespan
int solve_local_search(const Instance *inst, Solution *s) {
    if (!s->ok) return 0;
    int improved = 1;
    int safety = 0;
    // повторяем пока есть улучшения (safety чтобы не зависнуть)
    while (improved && safety < 100000) {
        improved = 0;
        safety++;
        for (int j = 0; j < inst->n; ++j) {
            const Job *job = &inst->jobs[j];
            int cur = s->assign[j];
            long cur_max = s->makespan;
            int best_i = cur;
            long best_max = cur_max;
            for (int k = 0; k < job->allowed_count; ++k) {
                int i2 = job->allowed[k];
                if (i2 == cur) continue;
                long new_cur = s->load[cur] - job->size;
                long new_i2  = s->load[i2]  + job->size;
                // считаем новый makespan быстро
                long new_max = 0;
                for (int x = 0; x < inst->m; ++x) {
                    long lx = (x == cur) ? new_cur : (x == i2 ? new_i2 : s->load[x]);
                    if (lx > new_max) new_max = lx;
                }
                if (new_max < best_max) { best_max = new_max; best_i = i2; }
            }
            // применяем перестановку если нашли лучший вариант
            if (best_i != cur) {
                s->load[cur]    -= job->size;
                s->load[best_i] += job->size;
                s->assign[j] = best_i;
                s->makespan = best_max;
                improved = 1;
            }
        }
    }
    return 1;
}

// local search с рестартами от случайных стартов

// случайное начальное назначение: каждому заданию берём случайную допустимую машину
static int random_legal_start(const Instance *inst, Solution *s) {
    // случайное допустимое назначение: каждому заданию выбираем случайную машину из M_j
    for (int j = 0; j < inst->n; ++j) {
        const Job *job = &inst->jobs[j];
        if (job->allowed_count <= 0) { s->ok = 0; return 0; }
        int k = rand() % job->allowed_count;
        s->assign[j] = job->allowed[k];
    }
    solution_recompute(s, inst);
    return s->ok;
}

// запускаем local search с нескольких стартов, берём лучший результат
int solve_local_restart(const Instance *inst, Solution *s, int restarts, unsigned seed) {
    if (restarts <= 0) restarts = 10;
    srand(seed);

    // первый старт — от LPT, как самый разумный
    if (!solve_greedy_lpt(inst, s)) return 0;
    if (!solve_local_search(inst, s)) return 0;
    long best = s->makespan;

    int *best_assign = malloc(sizeof(int) * (size_t)inst->n);
    if (!best_assign && inst->n > 0) return 0;
    memcpy(best_assign, s->assign, sizeof(int) * (size_t)inst->n);

    // остальные старты — случайные
    for (int r = 1; r < restarts; ++r) {
        if (!random_legal_start(inst, s)) continue;
        if (!solve_local_search(inst, s)) continue;
        // сохраняем лучший найденный результат
        if (s->makespan < best) {
            best = s->makespan;
            memcpy(best_assign, s->assign, sizeof(int) * (size_t)inst->n);
        }
    }
    memcpy(s->assign, best_assign, sizeof(int) * (size_t)inst->n);
    free(best_assign);
    solution_recompute(s, inst);
    return s->ok;
}

// brute force: рекурсивный перебор всех допустимых назначений
// предназначен для маленьких инстансов (n до ~12), даёт точный оптимум
// для верификации приближённых алгоритмов

typedef struct {
    const Instance *inst;
    long *load;
    int  *assign;
    int  *best_assign;
    long  best_makespan;
} BruteCtx;

// рекурсивно перебираем все назначения, отсекаем если makespan уже хуже лучшего
static void brute_recurse(BruteCtx *ctx, int j, long cur_max) {
    // отсечение: текущий максимум уже >= лучшего найденного
    if (cur_max >= ctx->best_makespan) return;

    if (j == ctx->inst->n) {
        // все назначены, обновляем лучшее решение
        ctx->best_makespan = cur_max;
        memcpy(ctx->best_assign, ctx->assign, sizeof(int) * (size_t)ctx->inst->n);
        return;
    }
    const Job *job = &ctx->inst->jobs[j];
    for (int k = 0; k < job->allowed_count; ++k) {
        int i = job->allowed[k];
        ctx->load[i] += job->size;
        long new_max = ctx->load[i] > cur_max ? ctx->load[i] : cur_max;
        ctx->assign[j] = i;
        brute_recurse(ctx, j + 1, new_max);
        ctx->load[i] -= job->size;
    }
}

// полный перебор, работает только для маленьких n
int solve_brute_force(const Instance *inst, Solution *s) {
    // защита от взрыва: жёсткое ограничение
    if (inst->n > 14) {
        fprintf(stderr, "brute_force: n=%d слишком велико (>14)\n", inst->n);
        s->ok = 0;
        return 0;
    }
    BruteCtx ctx;
    ctx.inst = inst;
    ctx.best_makespan = LONG_MAX;
    ctx.load = calloc((size_t)inst->m, sizeof(long));
    ctx.assign = malloc(sizeof(int) * (size_t)inst->n);
    ctx.best_assign = malloc(sizeof(int) * (size_t)inst->n);
    if (!ctx.load || (!ctx.assign && inst->n > 0) || (!ctx.best_assign && inst->n > 0)) {
        free(ctx.load); free(ctx.assign); free(ctx.best_assign);
        s->ok = 0; return 0;
    }
    for (int j = 0; j < inst->n; ++j) { ctx.assign[j] = -1; ctx.best_assign[j] = -1; }

    brute_recurse(&ctx, 0, 0);

    if (ctx.best_makespan == LONG_MAX) {
        free(ctx.load); free(ctx.assign); free(ctx.best_assign);
        s->ok = 0; return 0;
    }
    memcpy(s->assign, ctx.best_assign, sizeof(int) * (size_t)inst->n);
    free(ctx.load); free(ctx.assign); free(ctx.best_assign);
    solution_recompute(s, inst);
    return s->ok;
}
