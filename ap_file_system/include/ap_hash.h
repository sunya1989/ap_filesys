//
//  hash.h
//  ap_editor
//
//  Created by sunya on 15/4/30.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_ap_hash_h
#define ap_file_system_ap_hash_h
#define AP_IPC_HOLDER_HASH_LEN 1024
#define AP_IPC_FILE_HASH_LEN 256
#define AP_IPC_INODE_HASH_LEN 1024

#include <pthread.h>
#include "list.h"
#include "envelop.h"
#include "ap_ipc.h"

struct hash_table_union;

struct hash_identity{
    const char *ide_c;
    union{
        pthread_t thr_id;
        pid_t pid;
        unsigned long ide_i;
    }ide_type;
};

struct hash_union{
    struct hash_identity ide;
    struct list_head union_lis;
    struct hash_table_union *table;
};
struct hash_table_union{
    struct list_head hash_union_entry;
    pthread_mutex_t t_lock;
};

struct ap_hash{
    pthread_mutex_t r_size_lock;
    size_t r_size;
    size_t size;
    struct hash_table_union hash_table[0];
};

struct holder_table_union{
    struct list_head holder;
    pthread_mutex_t table_lock;
};

struct ipc_holder_hash{
    struct holder_table_union hash_table[AP_IPC_HOLDER_HASH_LEN];
};

extern struct ipc_holder_hash ipc_hold_table;
static inline struct ipc_sock *MALLOC_IPC_SOCK()
{
    struct ipc_sock *ipc_s = Mallocx(sizeof(*ipc_s));
    ipc_s->sever_name = NULL;
    return ipc_s;
}

static inline void increase_hash_rsize(struct ap_hash *table)
{
    pthread_mutex_lock(&table->r_size_lock);
    table->r_size++;
    pthread_mutex_unlock(&table->r_size_lock);
}

static inline void decrease_hash_rsize(struct ap_hash *table)
{
    pthread_mutex_lock(&table->r_size_lock);
    table->r_size--;
    pthread_mutex_unlock(&table->r_size_lock);
}

static inline void INITIALIZE_HASH_UNION(struct hash_union *ihu)
{
    ihu->ide.ide_c = NULL;
    ihu->ide.ide_type.ide_i = 0;
    INIT_LIST_HEAD(&ihu->union_lis);
}

static inline struct ap_hash *MALLOC_IPC_HASH(size_t size)
{
    struct ap_hash *hash_t = Mallocx(sizeof(struct hash_table_union) *size);
    hash_t->size = size;
    hash_t->r_size = 0;
    for (size_t i = 0; i<size; i++) {
        pthread_mutex_init(&hash_t->hash_table[i].t_lock, NULL);
    }
    return hash_t;
}

static inline unsigned int BKDRHash(char *str)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;
    
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

extern char *itoa(int num, char*str, int radix);
extern char *ultoa(unsigned long value, char *string, int radix);
extern struct hash_union *hash_union_get(struct ap_hash *table, struct hash_identity ide);
extern void hash_union_insert(struct ap_hash *table, struct hash_union *un);
extern void hash_union_delet(struct hash_union *un);
struct hash_union
*hash_union_insert_recheck(struct ap_hash *table, struct hash_union *un);

extern struct holder *ipc_holder_hash_get(struct hash_identity ide, int inc_cou);

#endif
