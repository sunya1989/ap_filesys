/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_thread_age_h
#define ap_file_system_thread_age_h

#include <pthread.h>
#include <ap_fsys/ger_fs.h>
#include <ap_fsys/list.h>

struct thread_attr_operations{
    ssize_t (*attr_read)(void *,struct ger_stem_node*, off_t, size_t);
    ssize_t (*attr_write)(void *, struct ger_stem_node*, off_t, size_t);
};

struct thread_age_attribute{
    struct thread_attr_operations *thr_attr_ops;
    struct ger_stem_node thr_stem;
    void *x_object;
};
struct thread_age_dir{
    struct ger_stem_node thr_dir_stem;
    pthread_t thr_id;
};

extern void THREAD_AGE_DIR_INIT(struct thread_age_dir *thr_dir, const char *name);
extern void THREAD_AGE_ATTR_INIT(struct thread_age_attribute *thr_attr, const char *name);

static inline struct thread_age_dir *MALLOC_THREAD_AGE_DIR(const char *name)
{
    struct thread_age_dir *thr_dir;
    thr_dir = malloc(sizeof(*thr_dir));
    if (thr_dir == NULL) {
        ap_err("malloc thread_age_dir failed\n");
        exit(1);
    }
    THREAD_AGE_DIR_INIT(thr_dir,name);
    return thr_dir;
    
}
static inline struct
thread_age_attribute *MALLOC_THREAD_AGE_ATTR
(struct thread_attr_operations *thr_attr_ops,
 const char *name)
{
    struct thread_age_attribute *thr_attr;
    thr_attr = malloc(sizeof(*thr_attr));
    if (thr_attr == NULL) {
        ap_err("malloc thread_age_attr failed\n");
        exit(1);
    }
    THREAD_AGE_ATTR_INIT(thr_attr, name);
    thr_attr->thr_attr_ops = thr_attr_ops;
    return thr_attr;
}

static inline void THREAD_AGE_ATTR_FREE(struct thread_age_attribute *thr_attr)
{
    COUNTER_FREE(&thr_attr->thr_stem.stem_inuse);
    pthread_mutex_destroy(&thr_attr->thr_stem.ch_lock);
    free(thr_attr);
}

static inline void THREAD_AGE_DIR_FREE(struct thread_age_dir *thr_dir)
{
    COUNTER_FREE(&thr_dir->thr_dir_stem.stem_inuse);
    pthread_mutex_destroy(&thr_dir->thr_dir_stem.ch_lock);
    free(thr_dir);
}

extern struct thread_age_dir *thr_compose_attrs
(const char *dir_name, struct thread_age_attribute **attrs, size_t size);
#endif
