// sync.c - время и вспомогательные функции для потоков
// 1 такт = tick_ms миллисекунд реального времени
#include "world.h"
#include <errno.h>
#include <time.h>
#include <unistd.h>

int now_ticks(World *w) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    long ms = t.tv_sec * 1000L + t.tv_nsec / 1000000L;
    return (int)((ms - w->start_ms) / w->cfg.tick_ms);
}

bool is_quit(World *w) {
    pthread_mutex_lock(&w->m);
    bool q = w->quit;
    pthread_mutex_unlock(&w->m);
    return q;
}

// спим по одному такту, чтобы быстро заметить завершение
void sleep_ticks(World *w, int n) {
    for (int i = 0; i < n && !is_quit(w); i++)
        usleep(w->cfg.tick_ms * 1000);
}

// сигнал (Ctrl+C) может прервать sem_wait, тогда ждем заново
void wait_sem(sem_t *s) {
    while (sem_wait(s) == -1 && errno == EINTR)
        ;
}
