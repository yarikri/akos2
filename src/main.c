// main.c - паромная переправа (вариант 28), многопоточная версия
// запуск: ./ferry [-c файл.cfg] [параметр=значение ...]
// потоки: главный (создает машины, проверки), паром, диспетчер и по потоку на каждую машину
#include "world.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_interrupted = 0;
static void on_signal(int sig) { (void)sig; g_interrupted = 1; }

static long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

// паром и машины, которые стоят в очереди с самого начала
static void world_init(World *w) {
    const Config *g = &w->cfg;
    w->total = (g->cars[LEFT] == UNLIMITED || g->cars[RIGHT] == UNLIMITED)
                   ? UNLIMITED : g->cars[LEFT] + g->cars[RIGHT];
    ferry_init(w);
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < g->initial[b]; i++) car_arrive(w, b);
        w->next_arr[b] = g->arr_min + rand() % (g->arr_max - g->arr_min + 1);
    }
    if (w->total == 0) w->stop = "машин нет";
}

// будим все потоки и ждем, пока они закончат
static void stop_threads(World *w, pthread_t ferry, pthread_t disp) {
    pthread_mutex_lock(&w->m);
    w->quit = true;
    pthread_mutex_unlock(&w->m);
    sem_post(&w->ferry.depart);
    sem_post(&w->ferry.unloaded);
    for (int i = 0; i < w->nall; i++) sem_post(&w->all[i]->go);
    pthread_join(ferry, NULL);
    pthread_join(disp, NULL);
    for (int i = 0; i < w->nall; i++)
        if (w->all[i]->started) pthread_join(w->all[i]->th, NULL);
}

int main(int argc, char **argv) {
    World w;
    memset(&w, 0, sizeof w);
    config_defaults(&w.cfg);

    // аргументы: -c файл.cfg и параметры key=value (пишите -c первым)
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            if (config_load_file(&w.cfg, argv[++i])) return 2;
        } else if (eq) {
            *eq = 0;
            if (config_set(&w.cfg, argv[i], eq + 1)) return 2;
        } else {
            dprintf(STDERR_FILENO, "Использование: %s [-c файл.cfg] [параметр=значение ...]\n", argv[0]);
            return 2;
        }
    }
    if (config_validate(&w.cfg)) return 2;

    if (!w.cfg.seed) w.cfg.seed = (int)time(NULL);
    srand((unsigned)w.cfg.seed);
    pthread_mutex_init(&w.m, NULL);
    w.start_ms = now_ms();
    log_open(&w);

    // по Ctrl+C только ставим флаг, главный поток его проверяет каждый такт
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    pthread_mutex_lock(&w.m);
    logr(&w, "=== Паромная переправа (seed=%d) ===", w.cfg.seed);
    world_init(&w);
    pthread_mutex_unlock(&w.m);

    pthread_t ferry_th, disp_th;
    pthread_create(&ferry_th, NULL, ferry_thread, &w);
    pthread_create(&disp_th, NULL, dispatcher_thread, &w);

    // главный поток: приезд машин, проверки, условия остановки
    for (;;) {
        pthread_mutex_lock(&w.m);
        if (!w.stop) arrivals_step(&w);
        check_invariants(&w);
        int t = now_ticks(&w);
        if (!w.stop && g_interrupted) w.stop = "прервано пользователем (Ctrl+C)";
        if (!w.stop && w.cfg.max_time && t >= w.cfg.max_time) w.stop = "вышло время (max_time)";
        if (w.stop) w.end_t = t;
        int done = w.stop != NULL;
        pthread_mutex_unlock(&w.m);
        if (done) break;
        usleep(w.cfg.tick_ms * 1000);
    }

    stop_threads(&w, ferry_th, disp_th);
    stats_print(&w);
    log_close(&w);
    for (int i = 0; i < w.nall; i++) { sem_destroy(&w.all[i]->go); free(w.all[i]); }
    free(w.all); free(w.q[LEFT].c); free(w.q[RIGHT].c); free(w.ferry.on);
    pthread_mutex_destroy(&w.m);
    return w.failed ? 3 : 0;
}
