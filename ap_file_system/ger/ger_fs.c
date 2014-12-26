//
//  ger_fs.c
//  ap_file_system
//
//  Created by sunya on 14/12/24.
//  Copyright (c) 2014年 sunya. All rights reserved.
//
#include "ger_fs.h"
#include "ap_fs.h"
#include "counter.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>


static struct ger_stem stem_root = {
    
};

static struct ap_file_operations ger_file_operations;
static struct ap_inode_operations ger_inode_operations;

static struct ap_inode *ger_alloc_inode(struct ger_stem *stem)
{
    struct ap_inode *ind;
    char *name;
    ssize_t n_len;
    ind = MALLOC_AP_INODE();
    
    n_len = strlen(stem->name);
    name = malloc(n_len + 1);
    if (name == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    ind->name = name;
    
    ind->x_object = stem;
    counter_get(&stem->stem_inuse);
    
    ind->is_dir = stem->is_dir;
    if (ind->is_dir) {
        ind->i_ops = &ger_inode_operations;
    }else{
        ind->f_ops = &ger_file_operations;
    }
    
    return ind;
}

static int ger_get_inode(struct ap_inode_indicator *indc)
{
    struct ger_stem *stem = (struct ger_stem *)indc->cur_inode->x_object; //类型检查？
    struct ger_stem *temp_stem;
    char *name = indc->the_name;
    
    struct list_head *cusor;
    
    pthread_mutex_lock(&stem->ch_lock);
    list_for_each(cusor, &stem->children){
        temp_stem = list_entry(cusor, struct ger_stem, child);
        if (strcmp(temp_stem->name, name)) {
            counter_get(&temp_stem->stem_inuse);
            pthread_mutex_unlock(&stem->ch_lock);
            goto  FINDED;
        }
    }
    pthread_mutex_unlock(&stem->ch_lock);
    return -1;
    
FINDED:
    indc->cur_inode = ger_alloc_inode(temp_stem);;
    counter_put(&temp_stem->stem_inuse);
    return 0;
}

static ssize_t ger_read(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct ger_stem *stem = (struct ger_stem *)file->x_object; //类型检查？
    if (stem->sf_ops->stem_read == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_read(stem, buf, off_set, len);
}

static ssize_t ger_write(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct ger_stem *stem = (struct ger_stem *)file->x_object;
    if (stem->sf_ops->stem_read == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_write(stem, buf, off_set, len);
}

static int ger_release(struct ap_file *file,struct ap_inode *ind)
{
    struct ger_stem *stem = (struct ger_stem *)file->x_object;
    counter_put(&stem->stem_inuse);
    file->x_object = NULL;
    
    if (stem->sf_ops->stem_release != NULL) {
        return stem->sf_ops->stem_release(stem);
    }
    return 0;
}

static int ger_open(struct ap_file *file, struct ap_inode *ind, unsigned long flags)
{
    struct ger_stem *stem = (struct ger_stem *)ind->x_object; //类型检查？
    
    file->x_object = stem;
    counter_get(&stem->stem_inuse);
    
    if (stem->sf_ops->stem_open != NULL) {
      return stem->sf_ops->stem_open(stem);
    }
    
    return 0;
}

static off_t ger_llseek(struct ap_file *file, off_t off_set, int origin)
{
    struct ger_stem *stem = (struct ger_stem *)file->x_object; //类型检查？
    
    if (stem->sf_ops->stem_llseek == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_llseek(stem, off_set, origin);
}

static int ger_rmdir(struct ap_inode_indicator *indc)
{
    struct ap_inode *ind = indc->cur_inode;
    struct ger_stem *stem = ind->x_object; //类型检查??
    
    return stem->si_ops->stem_rmdir(stem);
}

static int ger_mkdir(struct ap_inode_indicator *indc)
{
    struct ap_inode *ind = indc->cur_inode;
    struct ger_stem *stem = (struct ger_stem *)ind->x_object; //类型检查？
    struct ger_stem *new_stem;
    struct ap_inode *new_ind;
    
    new_stem = stem->si_ops->stem_mkdir(stem);
    if (new_stem == NULL) {
        return -1;
    }
    new_ind = ger_alloc_inode(new_stem);
    inode_add_child(indc->cur_inode, new_ind);
    return 0;
}

static int ger_destory(struct ap_inode *ind)
{
    struct ger_stem *stem = (struct ger_stem *)ind->x_object; //类型检查？
    if (stem->si_ops->stem_destory != NULL) {
        return stem->si_ops->stem_destory(stem);
    }
    return 0;
}

static struct ap_file_operations ger_file_operations = {
    .read = ger_read,
    .write = ger_write,
    .llseek = ger_llseek,
    .release = ger_release,
    .open = ger_open,
};

static struct ap_inode_operations ger_inode_operations = {
    .get_inode = ger_get_inode,
    .rmdir = ger_rmdir,
    .mkdir = ger_mkdir,
    .destory = ger_destory,
};



