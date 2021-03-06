/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_ger_fs_h
#define ap_file_system_ger_fs_h

#include <pthread.h>
#include <ap_fsys/list.h>
#include <ap_fsys/counter.h>
#include <ap_fsys/envelop.h>
#define GER_FILE_FS "ger"

struct stem_file_operations;
struct stem_inode_operations;

struct ger_stem_node{
    const char *stem_name;
    int is_dir;
    int stem_mode;
    
    struct ger_stem_node *parent;
    struct list_head children;
    struct list_head child;
    pthread_mutex_t ch_lock;
    
    struct counter stem_inuse;
    int raw_data_isset;
    
    int (*prepare_raw_data) (struct ger_stem_node *);
    
    struct stem_file_operations *sf_ops;
    struct stem_inode_operations *si_ops;
    
    void *x_object;
};

static inline void STEM_INIT(struct ger_stem_node *stem)
{
    stem->stem_name = NULL;
    stem->prepare_raw_data = NULL;
    stem->sf_ops = NULL;
    stem->si_ops = NULL;
    stem->x_object = NULL;
    stem->raw_data_isset = 0;
    
    INIT_LIST_HEAD(&stem->children);
    INIT_LIST_HEAD(&stem->child);
    COUNTER_INIT(&stem->stem_inuse);
    
    pthread_mutex_init(&stem->ch_lock, NULL);
}

static inline void STEM_FREE(struct ger_stem_node *stem)
{
    COUNTER_FREE(&stem->stem_inuse);
    pthread_mutex_destroy(&stem->ch_lock);
    free(stem);
}

static inline struct ger_stem_node *MALLOC_STEM()
{
    struct ger_stem_node *stem;
    stem = Malloc_z(sizeof(*stem));
    STEM_INIT(stem);
    return stem;
}

struct stem_file_operations{
    ssize_t (*stem_read) (struct ger_stem_node *, char *, off_t, size_t);
    ssize_t (*stem_write) (struct ger_stem_node *, char *, off_t, size_t);
    off_t (*stem_llseek) (struct ger_stem_node *, off_t, int);
    int (*stem_release) (struct ger_stem_node *);
    int (*stem_open) (struct ger_stem_node *, unsigned long);
};

struct stem_inode_operations{
    int (*stem_rmdir) (struct ger_stem_node *);
    int (*stem_unlink) (struct ger_stem_node *);
    struct ger_stem_node *(*stem_mkdir) (struct ger_stem_node *);
    int (*stem_destory) (struct ger_stem_node *);
};

extern int init_fs_ger();
extern struct ger_stem_node *find_stem_p(const char *p);
extern struct ger_stem_node *find_stem_r(struct ger_stem_node *root_stem, char **names, int counts);
extern int hook_to_stem(struct ger_stem_node *par, struct ger_stem_node *stem);
#endif
