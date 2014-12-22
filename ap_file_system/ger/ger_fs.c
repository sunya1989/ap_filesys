//
//  ger_fs.c
//  ap_file_system
//
//  Created by sunya on 14/12/9.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "ger_fs.h"
#include "ap_fs.h"
#include "ger_file_p.h"

static unsigned int ger_hash(char *path);
static int ger_open(struct ap_file *file, struct ap_inode *ind, unsigned long flags);

static struct ger_raw_hash_table ger_table = {
    .hash_func = ger_hash,
    .table_lock = PTHREAD_RWLOCK_INITIALIZER,
};

static struct ap_file_operations ger_file_operations = {
    .open = ger_open,
};

static struct ap_inode_operations ger_inode_operations = {
    
};

static unsigned int BKDRHash(char *str)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;
    
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    
    return (hash & 0x7FFFFFFF) % GER_HASH_MAX;
}

static unsigned int ger_hash(char *path)
{
    unsigned int hash;
    hash = BKDRHash(path);
    return hash;
}


static struct ger_con_file *ger_getrm_raw_data(char *path)
{
    struct ger_con_file **p;
    struct ger_con_file *get;
    unsigned int hash = ger_table.hash_func(path);
    
    pthread_mutex_lock(&ger_table.table_lock);
    p = &ger_table.hash_table[hash];
    
    while (*p != NULL) {
        if (strcmp(path, (*p)->path) == 0) {
            get = *p;
           *p = (*p)->ger_con_next;
            get->ger_con_next = NULL;
            pthread_mutex_unlock(&ger_table.table_lock);
            return get;
        }
        p = &(*p)->ger_con_next;
    }
    pthread_mutex_unlock(&ger_table.table_lock);
    return NULL;
}

static int ger_insert_raw_date(struct ap_file_system_type *f_type, void *info)
{
    struct ger_con_file *con = (struct ger_con_file*)info;//需要类型检查？
    struct ger_con_file **pp;
    unsigned int hash = ger_table.hash_func(con->path);
    
    if ( con->path == NULL ||con->name == NULL || con->target_file == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&ger_table.table_lock);
    pp = &ger_table.hash_table[hash];
    con->ger_con_next = *pp;
    *pp = con;
    pthread_mutex_unlock(&ger_table.table_lock);
    
    return 0;
}

struct ger_con_file *MALLOC_GER_CON()
{
    struct ger_con_file *con;
    con = malloc(sizeof(*con));
    if (con == NULL) {
        perror("ger_con_file malloc failed");
        exit(1);
    }
    con->ger_con_next = NULL;
    con->name = con->path = con->target_file = NULL;
        return con;
}

static struct ap_inode *ger_alloc_ap_inode(struct ger_inode *ger_ind)
{
    struct ap_inode *ap_ind = MALLOC_AP_INODE();
    char *name;
    ssize_t n_len =strlen(ger_ind->name);
    name = malloc(n_len + 1);
    
    ap_ind->x_object = ger_ind;
    counter_get(ger_ind->in_use);
    
    ap_ind->is_dir = ger_ind->is_dir;
    if (ger_ind->is_dir) {
        ap_ind->i_ops = &ger_inode_operations;
    }else{
        ap_ind->f_ops = &ger_file_operations;
    }
    
    ap_ind->name = name;
    ap_inode_get(ap_ind);
    
    return ap_ind;
}

static struct ger_inode *find_from_raw_data(struct ap_inode_indicator *indc)
{
    struct ger_con_file *con = ger_getrm_raw_data(indc->the_name);
    struct ger_inode *ger_ind;
    char *name, *target_file;
    if (con == NULL) {
        return NULL;
    }
    
    ger_ind = GER_INODE_MALLOC();
    ssize_t n_len = strlen(con->name);
    ssize_t tar_len = strlen(con->target_file);
    
    name = malloc(n_len + 1);
    target_file = malloc(tar_len + 1);
    
    if (name == NULL || target_file == NULL) {
        perror("targer_file name malloc failed");
        exit(1);
    }
    
    strncpy(name, con->name, n_len + 1);
    strncpy(target_file, con->target_file, tar_len + 1);
    ger_ind->target_file = target_file;
    ger_ind->name = name;
    ger_ind->state = con->state;
  
    
    free(con); //con_file 是一次性使用的;
    counter_get(ger_ind->in_use);
    return ger_ind;
}

static int ger_get_inode(struct ap_inode_indicator *indc)
{
    struct ger_inode *ger_ind = (struct ger_inode *)indc->cur_inode->x_object; //类型检查
    if (!ger_ind->is_dir) {
        return -1;
    }
    
    int finding_dir = indc->slash_remain == 0? 0:1;
    struct list_head *cusor;
    struct ger_inode *temp_inode;
    
    pthread_mutex_lock(&ger_ind->ch_lock);
    list_for_each(cusor, &ger_ind->children){
        temp_inode = list_entry(cusor, struct ger_inode, child);
        counter_get(temp_inode->in_use);
        if (strcmp(temp_inode->name, indc->the_name) == 0) {
            if (finding_dir^temp_inode->is_dir) {
                return -1;
            }
            goto FINED;
        }
    }
    if (!finding_dir) {
        temp_inode = find_from_raw_data(indc);
        if (temp_inode == NULL) {
            pthread_mutex_unlock(&ger_ind->ch_lock);
            return -1;
        }
        list_add(&temp_inode->child, &ger_ind->children);
        
    }else{
        pthread_mutex_unlock(&ger_ind->ch_lock);
        return -1;
    }
    
FINED:
    pthread_mutex_unlock(&ger_ind->ch_lock);
    indc->cur_inode = ger_alloc_ap_inode(temp_inode);;
    counter_put(temp_inode->in_use);
    return 0;
}

static int ger_open(struct ap_file *file, struct ap_inode *ind, unsigned long flags)
{
    struct ger_inode *ger_ind = (struct ger_inode *)ind->x_object; //类型jiancha
   
    file->real_fd = open(ger_ind->target_file, (int)flags);
    if (file->real_fd == -1) {
        perror("ger_open failed\n");
        return -1;
    }
    
    file->x_object = ger_ind;
    counter_get(ger_ind->in_use);
    return 0;
}

static ssize_t ger_read(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    ssize_t nr;
    nr = read(file->real_fd, buf, len);
    return nr;
}

static ssize_t ger_write(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    ssize_t nr;
    nr = write(file->real_fd, buf, len);
    return nr;
}

int covert_to_real_fd(int fd)
{
    struct ap_file *file;
    int real_fd;
    
    pthread_mutex_lock(&file_info->files_lock);
    if (file_info->file_list[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    file = file_info->file_list[fd];
    real_fd = file->real_fd;
    pthread_mutex_unlock(&file_info->files_lock);
    return real_fd;
}

FILE *convert_to_fs(int fd, char *flags)
{
    struct ap_file *file;
    int real_fd;
    FILE *fs;
    
    pthread_mutex_lock(&file_info->files_lock);
    if (file_info->file_list[fd] == NULL) {
        errno = ENOENT;
        return NULL;
    }
    file = file_info->file_list[fd];
    real_fd = file->real_fd;
    if (real_fd == -1) {
        return NULL;
    }
    
    if (file->real_fs != NULL) {
        return file->real_fs;
    }
    
    fs = fdopen(real_fd, flags);
    file->real_fs = fs;
    
    return fs;
}








