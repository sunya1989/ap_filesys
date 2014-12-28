//
//  ger_fs.h
//  ap_file_system
//
//  Created by sunya on 14/12/24.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#ifndef ap_file_system_ger_fs_h
#define ap_file_system_ger_fs_h
#include"list.h"
#include "counter.h"
#include <pthread.h>

struct stem_file_operations;
struct stem_inode_operations;

struct ger_stem{
    char *name;
    int is_dir;
    
    struct ger_stem *parent;
    struct list_head children;
    struct list_head child;
    pthread_mutex_t ch_lock;
    
    struct counter stem_inuse;

    struct stem_file_operations *sf_ops;
    struct stem_inode_operations *si_ops;
};

struct ger_dir{
    struct ger_stem;
};



static inline void STEM_INIT(struct ger_stem *stem)
{
    stem->name = NULL;
    INIT_LIST_HEAD(&stem->children);
    INIT_LIST_HEAD(&stem->child);
    
    pthread_mutex_init(&stem->ch_lock, NULL);

}

static inline void STEM_FREE(struct ger_stem *stem)
{
    COUNTER_FREE(&stem->stem_inuse);
    pthread_mutex_destroy(&stem->ch_lock);
    free(stem);
}

static inline struct ger_stem *MALLOC_STEM()
{
    struct ger_stem *stem;
    stem = malloc(sizeof(*stem));
    if (stem == NULL) {
        perror("ger_stem malloc failed\n");
        exit(1);
    }
    
    STEM_INIT(stem);
    return stem;
}

struct stem_file_operations{
    ssize_t (*stem_read) (struct ger_stem *, char *, off_t, size_t);
    ssize_t (*stem_write) (struct ger_stem *, char *, off_t, size_t);
    off_t (*stem_llseek) (struct ger_stem *, off_t, int);
    int (*stem_release) (struct ger_stem *);
    int (*stem_open) (struct ger_stem *, unsigned long);
};

struct stem_inode_operations{
    int (*stem_rmdir) (struct ger_stem *);
    struct ger_stem *(*stem_mkdir) (struct ger_stem *);
    int (*stem_destory)(struct ger_stem *);
};

extern int hook_to_stem(struct ger_stem *root_stem, struct ger_stem *stem);

#endif
