#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// генератор случайных инстансов для задачи распределения нагрузки
// usage: gen <m> <n> <tmin> <tmax> <density> [seed]
// density — доля машин, попадающих в M_j (0.0 < d <= 1.0)
// результат печатается в формате который читает instance_read

// случайное целое число в диапазоне [lo, hi]
static int rand_int(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}

int main(int argc, char **argv) {
    if (argc < 6 || argc > 7) {
        fprintf(stderr, "usage: %s <m> <n> <tmin> <tmax> <density> [seed]\n", argv[0]);
        return 1;
    }
    // читаем параметры из командной строки
    int m = atoi(argv[1]);
    int n = atoi(argv[2]);
    int tmin = atoi(argv[3]);
    int tmax = atoi(argv[4]);
    double density = atof(argv[5]);
    // если seed не задан, используем текущее время
    unsigned seed = (argc == 7) ? (unsigned)atol(argv[6]) : (unsigned)time(NULL);

    if (m <= 0 || n < 0 || tmin <= 0 || tmax < tmin || density <= 0.0 || density > 1.0) {
        fprintf(stderr, "bad args\n");
        return 1;
    }

    srand(seed);

    // первая строка: m машин, n заданий
    printf("%d %d\n", m, n);
    int *allowed = malloc(sizeof(int) * (size_t)m);
    if (!allowed && m > 0) return 1;

    for (int j = 0; j < n; ++j) {
        int t = rand_int(tmin, tmax);
        int count = 0;
        // отбираем машины с вероятностью density
        for (int i = 0; i < m; ++i) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < density) allowed[count++] = i;
        }
        // гарантируем хотя бы одну допустимую машину
        if (count == 0) {
            allowed[count++] = rand_int(0, m - 1);
        }
        // выводим размер, количество допустимых машин и их список
        printf("%d %d", t, count);
        for (int k = 0; k < count; ++k) printf(" %d", allowed[k]);
        printf("\n");
    }
    free(allowed);
    return 0;
}
