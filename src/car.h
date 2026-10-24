// car.h - машины (у каждой свой поток) и очереди
#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include "common.h"

typedef enum { LOC_QUEUE, LOC_FERRY, LOC_DONE } CarLoc;

typedef struct Car {
    World *w;
    int id;
    int mass;           // кг
    int size;           // сколько мест занимает
    bool emergency;
    int bank;           // где появилась
    CarLoc loc;
    int t_arrive;
    int skipped;        // сколько раз пропущена (lookahead)
    int rides;          // сколько раз перевезена, должно быть не больше 1
    int stamp;          // для проверки инвариантов
    pthread_t th;
    bool started;       // поток создан
    sem_t go;           // семафор: паром/диспетчер разрешили заезд или выезд
} Car;

typedef struct { Car **c; int n, cap; } Queue;

void car_arrive(World *w, int bank);    // новая машина и ее поток (мьютекс занят)
void *car_thread(void *arg);
void car_str(const Car *c, char *buf, size_t n);
void queue_remove(Queue *q, int idx);
void arrivals_step(World *w);
bool more_coming(const World *w, int bank);
