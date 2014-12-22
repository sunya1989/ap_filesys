//
//  ger_file_p.h
//  ap_file_system
//
//  Created by sunya on 14/11/29.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#ifndef ap_file_system_ger_file_p_h
#define ap_file_system_ger_file_p_h
#define GER_HASH_MAX 512
#include <pthread.h>
#include "counter.h"
#include "list.h"
#include "ger_fs.h"

struct ger_inode{
    int is_dir;
    
    char *name;
    char *target_file;
    pthread_mutex_t ch_lock;
    struct list_head children;
    struct list_head child;
    struct counter *in_use;
    
    enum file_state state;
};

static inline struct ger_inode *GER_INODE_MALLOC()
{
    struct ger_inode *ger_ind;
    ger_ind = malloc(sizeof(*ger_ind));
    if (ger_ind == NULL) {
        perror("ger_inode malloc failed\n");
        exit(1);
    }
    ger_ind->name = NULL;
    ger_ind->is_dir = 0;
    ger_ind->in_use = MALLOC_COUNTER();
    INIT_LIST_HEAD(&ger_ind->child);
    INIT_LIST_HEAD(&ger_ind->children);
    
    pthread_mutex_init(&ger_ind->ch_lock, NULL);
    return ger_ind;
}

static inline void GER_INODE_FREE(struct ger_inode *ger_ind)
{
    COUNTER_FREE(ger_ind->in_use);
    
    pthread_mutex_destroy(&ger_ind->ch_lock);
    free(ger_ind);
    free(ger_ind->name);
    free(ger_ind->target_file);
}


struct ger_raw_hash_table{
    pthread_mutex_t table_lock;
    struct ger_con_file *hash_table[GER_HASH_MAX];
    unsigned int (*hash_func) (char *);
};



#endif
