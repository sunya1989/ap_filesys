//
//  ap_hash.c
//  ap_editor
//
//  Created by sunya on 15/5/8.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <ap_hash.h>
#include <string.h>

static unsigned get_hash_n(struct hash_identity *ide, size_t size)
{
    unsigned hash;
    char *hasf_str;
    char *join;
    char str_arr[32];
    char *str = ultoa(ide->ide_i, str_arr, 10);
    if (ide->ide_c == NULL) {
        hasf_str = str;
    }else{
        size_t strl = strlen(str) + strlen(ide->ide_c);
        join = Mallocx(strl);
        strcpy(join, str);
        strcat(join, ide->ide_c);
        hasf_str = join;
    }
    hash = BKDRHash(hasf_str);
    hash = hash % size;
    free(join);
    return hash;
}

void ipc_holer_hash_insert(struct holder *hl)
{
    unsigned hash = get_hash_n(&hl->ide, AP_IPC_LOCK_HASH_LEN);
    hl->hash_n = hash;
    pthread_mutex_t *lock = &ipc_hold_table.hash_table[hash].table_lock;
    pthread_mutex_lock(lock);
    list_add(&hl->hash_lis, &ipc_hold_table.hash_table->holder);
    pthread_mutex_unlock(lock);
    return;
}

struct holder *ipc_holder_hash_get(struct hash_identity ide, int inc_cou)
{
    unsigned hash = get_hash_n(&ide, AP_IPC_LOCK_HASH_LEN);
    struct list_head *hl_indx;
    struct list_head *pos;
    struct holder *hl;
    pthread_mutex_t *lock = &ipc_hold_table.hash_table[hash].table_lock;
    pthread_mutex_lock(lock);
    hl_indx = &ipc_hold_table.hash_table[hash].holder;
    list_for_each(pos, hl_indx){
        hl = list_entry(pos, struct holder, hash_lis);
        if (strcmp(hl->ide.ide_c, ide.ide_c) == 0 && hl->ide.ide_i == ide.ide_i) {
            if (inc_cou) {
                hl->ipc_get(hl->x_object);
            }
            pthread_mutex_unlock(lock);
            return hl;
        }
    }
    pthread_mutex_unlock(lock);
    return NULL;
}

void hash_union_insert(struct ap_hash *table, struct hash_union *un)
{
    unsigned hash_n = get_hash_n(&un->ide, table->size);
    if (hash_n > table->size) {
        printf("hash table size erro\n");
        exit(1);
    }
    pthread_mutex_t *lock = &table->hash_table[hash_n].t_lock;
    pthread_mutex_lock(lock);
    list_add(&un->union_lis, &table->hash_table[hash_n].hash_union_entry);
    pthread_mutex_unlock(lock);
    un->table = &table->hash_table[hash_n];
    return;
}

struct hash_union *hash_union_get(struct ap_hash *table, struct hash_identity ide)
{
    unsigned hash_n = get_hash_n(&ide, table->size);
    if (hash_n > table->size) {
        printf("hash table size erro\n");
        exit(1);
    }

    struct list_head *pos;
    struct list_head *un;
    struct hash_union *hun;
    
    pthread_mutex_t *lock = &table->hash_table[hash_n].t_lock;
    pthread_mutex_lock(lock);
    un = &table->hash_table[hash_n].hash_union_entry;
    list_for_each(pos, un){
        hun = list_entry(pos, struct hash_union, union_lis);
        if (strcmp(hun->ide.ide_c, ide.ide_c) == 0 && hun->ide.ide_i == ide.ide_i) {
            pthread_mutex_unlock(lock);
            return hun;
        }
    }
    pthread_mutex_unlock(lock);
    return NULL;
}


void hash_union_delet(struct hash_union *un)
{
    struct hash_table_union *t_un = un->table;
    pthread_mutex_lock(&t_un->t_lock);
    list_del(&un->union_lis);
    pthread_mutex_unlock(&t_un->t_lock);
    return;
}



