// world.h - все состояние модели в одной структуре
#pragma once
#include <pthread.h>
#include "common.h"
#include "config.h"
#include "car.h"
#include "ferry.h"
#include "dispatcher.h"
#include "stats.h"
#include "log.h"
#include "sync.h"

struct World {
    Config cfg;
    pthread_mutex_t m;              // один мьютекс на все общие данные
    long start_ms;                  // когда началась симуляция
    Car **all; int nall, all_cap;   // все созданные машины
    Queue q[2];                     // очереди на берегах
    int next_arr[2], gen[2];        // когда приедет следующая, сколько создано
    Ferry ferry;
    Stats st;
    int log_fd;
    int delivered, total;           // перевезено, всего
    int end_t;                      // время остановки
    int stamp, failed;
    const char *stop;               // причина остановки (NULL пока работаем)
    bool quit;                      // true - потоки должны завершиться
};
