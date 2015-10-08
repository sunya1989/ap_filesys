/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_counter_h
#define ap_file_system_counter_h
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <bag.h>

#define INIT_COUNTER {PTHREAD_MUTEX_INITIALIZER,0}
struct counter{
    pthread_mutex_t counter_lock;
    int in_use;
};

static inline int COUNTER_INIT(struct counter *counter)
{
    int init = pthread_mutex_init(&counter->counter_lock, NULL);
    if (init != 0) 
        return -1;
    
    counter->in_use = 0;
    return 0;
}

static inline struct counter *MALLOC_COUNTER()
{
    struct counter *counter;
    counter = malloc(sizeof(*counter));
    if (counter == NULL) {
        perror("counter malloc failed\n");
        exit(1);
    }
    COUNTER_INIT(counter);
    return counter;
}

static inline void COUNTER_FREE(struct counter *counter)
{
    pthread_mutex_destroy(&counter->counter_lock);
}

static inline void counter_get(struct counter *counter)
{
    pthread_mutex_lock(&counter->counter_lock);
    if (counter->in_use<0) {
        fprintf(stderr, "counter wrong!\n");
        exit(1);
    }
    counter->in_use++;
    pthread_mutex_unlock(&counter->counter_lock);
    return;
}

BAG_DEFINE_FREE(counter_put);
static inline void counter_put(struct counter *counter)
{
    pthread_mutex_lock(&counter->counter_lock);
    counter->in_use--;
    if (counter->in_use<0) {
        fprintf(stderr, "counter wrong!\n");
        exit(1);
    }
    pthread_mutex_unlock(&counter->counter_lock);
    return;
}

static inline void
counter_put_release(struct counter *counter, void (*release) (struct counter *))
{
    pthread_mutex_lock(&counter->counter_lock);
    counter->in_use--;
    if (counter->in_use<0) {
        fprintf(stderr, "counter wrong!\n");
        exit(1);
    }
    if (counter->in_use == 0)
        release(counter);
    
    pthread_mutex_unlock(&counter->counter_lock);
    
}
#endif
