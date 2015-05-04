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

struct ipc_identity{
    char *ide_c;
    unsigned long ide_i;
};

struct lock_holder{
    void *holer;
    struct ipc_identity ide;
    struct list_head hash_lis;
    void (*lock)(void *, struct list_head);
    void (*unlock)(void *, struct list_head);
};

extern struct ipc_locker_hash{
    struct lock_table_union{
        struct lock_holder *holder;
        pthread_mutex_t table_lock;
    }hash_table[AP_IPC_LOCK_HASH_LEN];
}ipc_lock_table;

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
extern void ipc_lock_hash_insert(struct lock_holder *hl);

#endif
