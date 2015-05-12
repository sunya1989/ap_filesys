//
//  hash.h
//  ap_editor
//
//  Created by sunya on 15/4/30.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_editor_hash_h
#define ap_editor_hash_h
#define AP_IPC_LOCK_HASH_LEN 1024
#include <pthread.h>
#include "list.h"
#include "envelop.h"
#include "ap_ipc.h"

struct hash_identity{
    char *ide_c;
    unsigned long ide_i;
};

struct hash_uion{
    struct hash_identity ide;
    struct list_head union_lis;
};
struct hash_table_union{
    struct list_head hash_union_entry;
    pthread_mutex_t t_lock;
};

struct ap_hash{
    size_t size;
    struct hash_table_union hash_table[0];
};

struct holder{
    void *x_object;
    unsigned hash_n;
    struct hash_identity ide;
    struct list_head hash_lis;
    void (*ipc_get)(void *);
    void (*ipc_put)(void *);
};

extern struct ipc_holder_hash{
    struct holder_table_union{
        struct list_head holder;
        pthread_mutex_t table_lock;
    }hash_table[AP_IPC_LOCK_HASH_LEN];
}ipc_hold_table;

static inline struct ipc_sock *MALLOC_IPC_SOCK()
{
    struct ipc_sock *ipc_s = Mallocx(sizeof(*ipc_s));
    ipc_s->sever_name = NULL;
    return ipc_s;
}

static inline void INITIALIZE_IPC_HASH_UNION(struct hash_uion *ihu)
{
    ihu->ide.ide_c = NULL;
    ihu->ide.ide_i = 0;
    INIT_LIST_HEAD(&ihu->union_lis);
}

static inline struct ap_hash *MALLOC_IPC_HASH(size_t size)
{
    struct ap_hash *hash_t = Mallocx(sizeof(struct hash_table_union) *size);
    hash_t->size = size;
    return hash_t;
}

static inline struct holder *MALLOC_HOLDER()
{
    struct holder *hl = Mallocx(sizeof(*hl));
    hl->x_object = NULL;
    INIT_LIST_HEAD(&hl->hash_lis);
    hl->ide.ide_c = NULL;
    hl->ipc_get = hl->ipc_put = NULL;
    return hl;
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
extern struct hash_uion *hash_uinon_get(struct ap_hash *table, struct hash_identity ide);
extern void hash_uinon_insert(struct ap_hash *table, struct hash_uion *un);
extern void ipc_holder_hash_insert(struct holder *hl);
extern struct holder *ipc_holer_hash_get(struct hash_identity ide);

#endif
