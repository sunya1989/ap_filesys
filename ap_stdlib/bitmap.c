/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <stdio.h>
#include <pthread.h>
#include <bitmap.h>

ssize_t find_next_unset_bit(bitmap_t *b, size_t from)
{
    size_t b_size = b->size;
    int bit;
    size_t i;
    for (i = 0; i < b_size; i++) {
        size_t c_bit = (from + i) % b_size;
        bit = get_bitmap(b, c_bit);
        if (bit == 0)
            return c_bit;
    }
    return -1;
}

ssize_t set_next_unset_bit(bitmap_t *b)
{
    size_t bit =  find_next_unset_bit(b, 0);
    if (bit == -1)
        return -1;
    set_bitmap(b, bit);
    return bit;
}

void atomic_set_bitmap(bitmap_t *b, size_t i)
{
    pthread_mutex_lock(&b->map_lock);
    set_bitmap(b, i);
    pthread_mutex_unlock(&b->map_lock);
}
extern void atomic_unset_bitmap(bitmap_t *b, size_t i)
{
    pthread_mutex_lock(&b->map_lock);
    unset_bitmap(b, i);
    pthread_mutex_unlock(&b->map_lock);
}
extern int atomic_get_bitmap(bitmap_t *b, size_t i)
{
    pthread_mutex_lock(&b->map_lock);
    int bit = get_bitmap(b, i);
    pthread_mutex_unlock(&b->map_lock);
    return bit;
}
extern ssize_t atomic_find_next_unset_bit(bitmap_t *b, size_t from)
{
    ssize_t bit;
    pthread_mutex_lock(&b->map_lock);
    bit = find_next_unset_bit(b, from);
    pthread_mutex_unlock(&b->map_lock);
    return bit;
}
extern ssize_t atomic_set_next_unset_bit(bitmap_t *b)
{
    ssize_t bit;
    pthread_mutex_lock(&b->map_lock);
    bit = set_next_unset_bit(b);
    pthread_mutex_unlock(&b->map_lock);
    return bit;
}


