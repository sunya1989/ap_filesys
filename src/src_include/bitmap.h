/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_tester_bitmap_h
#define ap_tester_bitmap_h
#include <envelop.h>
struct bit_map{
    pthread_mutex_t map_lock;
    size_t size;
    char *map;
};

typedef struct bit_map bitmap_t;

static inline void set_bitmap(bitmap_t *b, size_t i) {
    b->map[i / 8] |= 1 << (i & 7);
}

static inline void unset_bitmap(bitmap_t *b, size_t i) {
    b->map[i / 8] &= ~(1 << (i & 7));
}

static inline int get_bitmap(bitmap_t *b, size_t i) {
    return b->map[i / 8] & (1 << (i & 7)) ? 1 : 0;
}

static inline bitmap_t *create_bitmap(size_t n) {
    bitmap_t *bm = Malloc_z(sizeof(*bm));
    pthread_mutex_init(&bm->map_lock, NULL);
    bm->map = Malloc_z((n + 7) / 8);
    bm->size = n;
    return bm;
}

static inline void bitmap_free(bitmap_t *b)
{
    pthread_mutex_destroy(&b->map_lock);
    free(b->map);
}

extern ssize_t find_next_unset_bit(bitmap_t *b, size_t from);
extern ssize_t set_next_unset_bit(bitmap_t *b);

extern void atomic_set_bitmap(bitmap_t *b, size_t i);
extern void atomic_unset_bitmap(bitmap_t *b, size_t i);
extern int atomic_get_bitmap(bitmap_t *b, size_t i);
extern ssize_t atomic_find_next_unset_bit(bitmap_t *b, size_t from);
extern ssize_t atomic_set_next_unset_bit(bitmap_t *b);

#endif
