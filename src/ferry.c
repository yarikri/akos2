// ferry.c - паром (свой поток)
// состояния: погрузка -> закрытие аппарели -> рейс -> выгрузка -> ...
// все изменения общих данных делаем под мьютексом w->m
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ferry_status(const World *w, char *buf, size_t n) {
    const Ferry *f = &w->ferry;
    int len = snprintf(buf, n, "[%d/%d авто, %d/%d мест, %d/%d кг]", f->n, w->cfg.max_cars,
                       f->slots, w->cfg.slots, f->mass, w->cfg.capacity_kg);
    for (int i = 0; i < f->n && len < (int)n - 12; i++)
        len += snprintf(buf + len, n - len, " #%d", f->on[i]->id);
}

// влезет ли машина, иначе в why причина
bool ferry_fits(const World *w, const Car *c, int *why) {
    const Ferry *f = &w->ferry;
    if (f->n >= w->cfg.max_cars)                     *why = REFUSE_COUNT;
    else if (f->slots + c->size > w->cfg.slots)      *why = REFUSE_SLOTS;
    else if (f->mass + c->mass > w->cfg.capacity_kg) *why = REFUSE_MASS;
    else return true;
    return false;
}

void ferry_init(World *w) {
    Ferry *f = &w->ferry;
    memset(f, 0, sizeof *f);
    f->on = calloc(w->cfg.max_cars, sizeof(Car *));
    f->state = F_LOADING;
    f->bank = LEFT;
    f->ramp_open = true;
    sem_init(&f->depart, 0, 0);
    sem_init(&f->unloaded, 0, 0);
    logw(w, "Паром у берега: %s, начинаем погрузку", BANK[f->bank]);
}

static void open_for_loading(World *w) {
    Ferry *f = &w->ferry;
    f->state = F_LOADING;
    f->t_open = now_ticks(w);
    f->cur_car = NULL;
    f->ramp_open = true;
    logw(w, "Погрузка, берег: %s (в очереди: %d)", BANK[f->bank], w->q[f->bank].n);
    if (w->total != UNLIMITED && w->delivered >= w->total)
        w->stop = "все автомобили перевезены";
    else if (w->cfg.max_trips && f->trips >= w->cfg.max_trips)
        w->stop = "выполнено заданное число рейсов";
}

// диспетчер пустил машину: она уходит из очереди на паром, место занимаем сразу
void ferry_board(World *w, Car *c) {
    Ferry *f = &w->ferry;
    Queue *q = &w->q[f->bank];
    for (int i = 0; i < q->n; i++)
        if (q->c[i] == c) { queue_remove(q, i); break; }
    c->loc = LOC_FERRY;
    c->rides++;
    f->on[f->n++] = c;
    f->mass += c->mass;
    f->slots += c->size;
    f->cur_car = c;
    int wt = now_ticks(w) - c->t_arrive;
    w->st.wait_sum[c->emergency] += wt;
    w->st.wait_n[c->emergency]++;
    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Грузим %s (ждала %d мин)", s, wt);
    sem_post(&c->go);                   // будим поток машины
}

// диспетчер закончил погрузку: будим паром
void ferry_close(World *w, int reason) {
    Ferry *f = &w->ferry;
    char st[200];
    ferry_status(w, st, sizeof st);
    logw(w, "Конец погрузки (%s). Паром %s", DEP_TXT[reason], st);
    f->state = F_RAMP_CLOSING;
    sem_post(&f->depart);
}

void *ferry_thread(void *arg) {
    World *w = arg;
    Ferry *f = &w->ferry;
    char s[96];
    for (;;) {
        wait_sem(&f->depart);               // ждем, пока диспетчер разрешит отплыть
        if (is_quit(w)) break;
        sleep_ticks(w, w->cfg.ramp_time);   // закрываем аппарель
        if (is_quit(w)) break;

        pthread_mutex_lock(&w->m);
        f->ramp_open = false;
        f->state = F_CROSSING;
        f->trips++;
        w->st.trips++;
        if (!f->n) w->st.empty_trips++;
        logw(w, "Отправление, рейс %d, берег отправления: %s", f->trips, BANK[f->bank]);
        pthread_mutex_unlock(&w->m);
        sleep_ticks(w, w->cfg.cross_time);  // плывем
        if (is_quit(w)) break;

        pthread_mutex_lock(&w->m);
        f->bank = OTHER(f->bank);
        f->ramp_open = true;
        f->state = F_UNLOADING;
        logw(w, "Паром приплыл, берег: %s, на борту %d авто", BANK[f->bank], f->n);
        pthread_mutex_unlock(&w->m);

        // выгружаем по одной машине: будим ее и ждем, пока съедет
        for (;;) {
            pthread_mutex_lock(&w->m);
            Car *c = f->n ? f->on[0] : NULL;
            if (c) {
                f->cur_car = c;
                car_str(c, s, sizeof s);
                logw(w, "Выгружаем %s", s);
            }
            pthread_mutex_unlock(&w->m);
            if (!c) break;
            sem_post(&c->go);
            wait_sem(&f->unloaded);
            if (is_quit(w)) return NULL;
        }

        pthread_mutex_lock(&w->m);
        open_for_loading(w);
        pthread_mutex_unlock(&w->m);
    }
    return NULL;
}
