// car.c - машины и очереди
// у каждой машины свой поток, она ждет на своем семафоре go
// экстренные встают вперед, но не обгоняют тех, кто ждет дольше aging_limit
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rnd(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

void car_str(const Car *c, char *buf, size_t n) {
    const char *kind = c->emergency ? "экстренная" : c->size > 1 ? "большая" : "легковая";
    snprintf(buf, n, "#%d (%s, %d кг, %d мест)", c->id, kind, c->mass, c->size);
}

void queue_remove(Queue *q, int idx) {
    memmove(&q->c[idx], &q->c[idx + 1], (q->n - idx - 1) * sizeof(Car *));
    q->n--;
}

static void queue_insert(World *w, int bank, Car *car) {
    Queue *q = &w->q[bank];
    if (q->n == q->cap) {
        q->cap = q->cap ? q->cap * 2 : 16;
        q->c = realloc(q->c, q->cap * sizeof(Car *));
    }
    int pos = q->n;
    if (car->emergency && w->cfg.priority) {
        pos = 0;    // пропускаем экстренных и тех, кто давно ждет
        while (pos < q->n) {
            const Car *o = q->c[pos];
            if (!o->emergency && now_ticks(w) - o->t_arrive < w->cfg.aging_limit) break;
            pos++;
        }
    }
    memmove(&q->c[pos + 1], &q->c[pos], (q->n - pos) * sizeof(Car *));
    q->c[pos] = car;
    q->n++;
}

// поведение машины: ждет разрешения заехать, заезжает, ждет выгрузки, съезжает
void *car_thread(void *arg) {
    Car *c = arg;
    World *w = c->w;
    Ferry *f = &w->ferry;
    char s[96], st[200];

    wait_sem(&c->go);                       // диспетчер пустил на паром
    if (is_quit(w)) return NULL;
    sleep_ticks(w, w->cfg.load_time);       // заезжаем
    if (is_quit(w)) return NULL;
    pthread_mutex_lock(&w->m);
    car_str(c, s, sizeof s);
    ferry_status(w, st, sizeof st);
    logw(w, "Погрузили %s. Паром %s", s, st);
    f->cur_car = NULL;
    pthread_mutex_unlock(&w->m);

    wait_sem(&c->go);                       // паром приплыл, наша очередь выгружаться
    if (is_quit(w)) return NULL;
    sleep_ticks(w, w->cfg.unload_time);     // съезжаем
    if (is_quit(w)) return NULL;
    pthread_mutex_lock(&w->m);
    for (int i = 0; i < f->n; i++)
        if (f->on[i] == c) {
            memmove(&f->on[i], &f->on[i + 1], (f->n - i - 1) * sizeof(Car *));
            f->n--;
            break;
        }
    f->mass -= c->mass;
    f->slots -= c->size;
    c->loc = LOC_DONE;
    w->st.carried[c->bank]++;
    w->delivered++;
    ferry_status(w, st, sizeof st);
    logw(w, "Выгрузили %s. Паром %s", s, st);
    f->cur_car = NULL;
    pthread_mutex_unlock(&w->m);
    sem_post(&f->unloaded);                 // говорим парому, что съехали
    return NULL;
}

// новая машина со случайными параметрами (вызывается при занятом мьютексе)
void car_arrive(World *w, int bank) {
    if (w->nall == w->all_cap) {
        w->all_cap = w->all_cap ? w->all_cap * 2 : 64;
        w->all = realloc(w->all, w->all_cap * sizeof(Car *));
    }
    Car *c = calloc(1, sizeof *c);
    w->all[w->nall++] = c;
    c->w = w;
    c->id = w->nall;
    int r = rand() % 100;
    c->size = r < 60 ? 1 : r < 80 ? 2 : r < 90 ? 3 : 4;
    if (c->size > w->cfg.size_max) c->size = w->cfg.size_max;
    c->mass = c->size * rnd(w->cfg.mass_min, w->cfg.mass_max);
    c->emergency = rand() % 100 < w->cfg.emergency_pct;
    c->bank = bank;
    c->t_arrive = now_ticks(w);
    sem_init(&c->go, 0, 0);
    w->gen[bank]++;
    queue_insert(w, bank, c);

    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Приехала машина %s на %s берег (в очереди: %d)", s, BANK[bank], w->q[bank].n);
    if (pthread_create(&c->th, NULL, car_thread, c) == 0) c->started = true;
    else w->stop = "не удалось создать поток";
}

bool more_coming(const World *w, int bank) {
    return w->cfg.cars[bank] == UNLIMITED || w->gen[bank] < w->cfg.cars[bank];
}

// главный поток создает машины, когда подходит время
void arrivals_step(World *w) {
    for (int b = 0; b < 2; b++)
        if (more_coming(w, b) && w->next_arr[b] <= now_ticks(w)) {
            car_arrive(w, b);
            w->next_arr[b] = now_ticks(w) + rnd(w->cfg.arr_min, w->cfg.arr_max);
        }
}
