// ferry.h - паром (отдельный поток)
#pragma once
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include "car.h"

typedef enum { F_LOADING, F_RAMP_CLOSING, F_CROSSING, F_UNLOADING } FerryState;
enum { REFUSE_MASS, REFUSE_SLOTS, REFUSE_COUNT };   // причины отказа

typedef struct {
    FerryState state;
    int bank;           // у какого берега стоит (в рейсе - откуда отплыл)
    Car **on;           // машины на борту
    int n, mass, slots; // сколько занято
    Car *cur_car;       // кого грузим/выгружаем (NULL - никого)
    int t_open;         // когда начали погрузку у этого берега
    bool ramp_open;
    int trips;
    sem_t depart;       // диспетчер разрешил отплытие
    sem_t unloaded;     // машина съехала с парома
} Ferry;

void ferry_init(World *w);
void *ferry_thread(void *arg);
void ferry_board(World *w, Car *c);             // диспетчер пускает машину на паром
void ferry_close(World *w, int reason);         // диспетчер завершает погрузку
bool ferry_fits(const World *w, const Car *c, int *why);
void ferry_status(const World *w, char *buf, size_t n);
