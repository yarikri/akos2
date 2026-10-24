// sync.h - время и вспомогательные функции для потоков
#pragma once
#include <semaphore.h>
#include <stdbool.h>
#include "common.h"

int now_ticks(World *w);            // сколько тактов прошло с начала
void sleep_ticks(World *w, int n);  // спим n тактов (раньше, если пора завершаться)
void wait_sem(sem_t *s);            // sem_wait, который переживает сигналы
bool is_quit(World *w);             // true, если программу пора завершать
