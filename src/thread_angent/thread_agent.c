/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <string.h>
#include <stdio.h>
#include <envelop.h>
#include "thread_agent.h"

static struct stem_file_operations thr_age_file_operations;
static struct stem_inode_operations thr_age_inode_operations;

void THREAD_AGE_DIR_INIT(struct thread_age_dir *thr_dir, const char *name)
{
    STEM_INIT(&thr_dir->thr_dir_stem);
    size_t strl = strlen(name);
    char *n = Malloc_z(strl+1);
    strncpy(n, name, strl);
    thr_dir->thr_dir_stem.is_dir = 1;
    thr_dir->thr_dir_stem.stem_name = n;
    thr_dir->thr_dir_stem.si_ops = &thr_age_inode_operations;
    return;
}

void THREAD_AGE_ATTR_INIT(struct thread_age_attribute *thr_attr, const char *name)
{
    STEM_INIT(&thr_attr->thr_stem);
    size_t strl = strlen(name);
    char *n = Malloc_z(strl+1);
    strncpy(n, name, strl);
    thr_attr->thr_stem.is_dir = 0;
    thr_attr->thr_stem.stem_name = n;
    thr_attr->thr_stem.sf_ops = &thr_age_file_operations;
    thr_attr->thr_stem.si_ops = &thr_age_inode_operations;
    thr_attr->x_object = NULL;
    return;
}

struct thread_age_dir *thr_compose_attrs
(const char *dir_name, struct thread_age_attribute **attrs, size_t size)
{
    struct thread_age_dir *dir = MALLOC_THREAD_AGE_DIR(dir_name);
    for (size_t i = 0; i < size; i++) {
        list_add(&attrs[i]->thr_stem.child, &dir->thr_dir_stem.children);
    }
    return dir;
}

static ssize_t thread_age_read(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct thread_age_attribute *thr_attr = container_of(stem, struct thread_age_attribute, thr_stem);
    ssize_t n_read;
    n_read = thr_attr->thr_attr_ops->attr_read(buf,stem,off_set,len);
    return n_read;
}

static ssize_t thread_age_write(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct thread_age_attribute *thr_attr = container_of(stem, struct thread_age_attribute, thr_stem);
    ssize_t n_write;
    n_write = thr_attr->thr_attr_ops->attr_write(buf,stem,off_set,len);
    return n_write;
}

static int thread_age_unlink(struct ger_stem_node *stem)
{
    struct thread_age_attribute *thr_attr = container_of(stem, struct thread_age_attribute, thr_stem);
    THREAD_AGE_ATTR_FREE(thr_attr);
    return 0;
}

static int thread_age_rmdir(struct ger_stem_node *stem)
{
    struct thread_age_dir *thr_dir = container_of(stem, struct thread_age_dir, thr_dir_stem);
    THREAD_AGE_DIR_FREE(thr_dir);
    return 0;
}

static struct stem_file_operations thr_age_file_operations = {
    .stem_read = thread_age_read,
    .stem_write = thread_age_write,
};

static struct stem_inode_operations thr_age_inode_operations = {
    .stem_rmdir = thread_age_rmdir,
    .stem_unlink = thread_age_unlink,
};

