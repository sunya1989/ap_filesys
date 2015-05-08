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

static unsigned ipc_get_hash_n(struct ipc_identity *ide)
{
    unsigned hash;
    char *hasf_str;
    char str_arr[32];
    char *str = ultoa(ide->ide_i, str_arr, 10);
    if (ide->ide_c == NULL) {
        hasf_str = str;
    }else{
        size_t strl = strlen(str) + strlen(ide->ide_c);
        char *join = Mallocx(strl);
        strcpy(join, str);
        strcat(join, ide->ide_c);
        hasf_str = join;
    }
    hash = BKDRHash(hasf_str);
    hash = hash % AP_IPC_LOCK_HASH_LEN;
    return hash;
}

void ipc_holer_hash_insert(struct holder *hl)
{
    unsigned hash = ipc_get_hash_n(&hl->ide);
    hl->hash_n = hash;
    pthread_mutex_t *lock = &ipc_lock_table.hash_table[hash].table_lock;
    pthread_mutex_lock(lock);
    if (list_empty(&ipc_lock_table.hash_table[hash].holder)) {
        list_add(&hl->hash_lis, &ipc_lock_table.hash_table[hash].holder);
    }else{
        list_add(&ipc_lock_table.hash_table[hash].holder, &hl->hash_lis);
    }
    pthread_mutex_unlock(lock);
    return;
}

struct holder *ipc_holder_hash_get(struct ipc_identity ide)
{
    unsigned hash = ipc_get_hash_n(&ide);
    struct list_head *hl_indx;
    struct list_head *pos;
    struct holder *hl;
    pthread_mutex_t *lock = &ipc_lock_table.hash_table[hash].table_lock;
    pthread_mutex_lock(lock);
    hl_indx = &ipc_lock_table.hash_table[hash].holder;
    if (hl_indx == NULL) {
        pthread_mutex_unlock(lock);
        return NULL;
    }
    list_for_each(pos, hl_indx){
        hl = list_entry(pos, struct holder, hash_lis);
        if (strcmp(hl->ide.ide_c, ide.ide_c) == 0 && hl->ide.ide_i == ide.ide_i) {
            hl->ipc_get(hl->x_object);
            pthread_mutex_unlock(lock);
            return hl;
        }
    }
    return NULL;
}

void ipc_uinon_insert(struct ipc_hash_uion *un)
{
    
}

struct ipc_hash_uion *ipc_uinon_get(struct ipc_hash *table, struct ipc_identity ide)
{
    unsigned hash_n = ipc_get_hash_n(&ide);
    struct list_head *pos;
    struct ipc_hash_uion *un;
    
    pthread_mutex_t *lock = &table->hash_table[hash_n].t_lock;
    pthread_mutex_lock(lock);
    un = table->hash_table[hash_n].hash_union;
    if (un == NULL) {
        pthread_mutex_unlock(lock);
        return NULL;
    }
    list_for_each(pos, &un->union_lis){
        un = list_entry(pos, struct ipc_hash_uion, union_lis);
        if (strcmp(un->ide.ide_c, ide.ide_c) == 0 && un->ide.ide_i == ide.ide_i) {
            pthread_mutex_unlock(lock);
            return un;
        }
    }

    return NULL;
}



