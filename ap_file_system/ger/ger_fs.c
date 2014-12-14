//
//  ger_fs.c
//  ap_file_system
//
//  Created by sunya on 14/12/9.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "ger_fs.h"
#include "ap_fs.h"
#include "ger_file_p.h"

static unsigned int ger_hash(struct ger_con_file *con);

static struct ger_raw_hash_table ger_table = {
    .hash_func = ger_hash,
    .table_lock = PTHREAD_RWLOCK_INITIALIZER,
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

static unsigned int ger_hash(struct ger_con_file *con)
{
    unsigned int hash;
   
    char *path = con->path;
    hash = BKDRHash(path);
    return hash;
}

static struct ger_con_file *ger_get_raw_data(struct ger_con_file *con)
{
    unsigned int hash = ger_table.hash_func(con);
    struct ger_con_file *p;
    char *path = con->path;
    pthread_rwlock_rdlock(&ger_table.table_lock);
    p = ger_table.hash_table[hash];
  
    while (p != NULL) {
        if (strcmp(path, p->path) == 0) {
            counter_get(p->in_use);
            pthread_rwlock_unlock(&ger_table.table_lock);
            return p;
        }
    }
    pthread_rwlock_unlock(&ger_table.table_lock);
    return NULL;
}

static int ger_insert_raw_date(struct ap_file_system_type *f_type, void *info)
{
    struct ger_con_file *con = (struct ger_con_file*)info;//需要类型检查？
    struct ger_con_file **pp;
    unsigned int hash = ger_table.hash_func(con);
    
    pthread_rwlock_rdlock(&ger_table.table_lock);
    pp = &ger_table.hash_table[hash];
    
    while (*pp != NULL) {
        pp = &((*pp)->ger_con_next);
    }
    *pp = con;
    pthread_rwlock_unlock(&ger_table.table_lock);
    
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
    con->in_use = MALLOC_COUNTER();
    return con;
}

static struct ap_inode *ger_alloc_ap_inode(struct ger_inode *ger_ind)
{
    return NULL;
}

static struct ger_inode *find_from_raw_data(struct ap_inode_indicator *indc)
{
    return NULL;
}

static int ger_get_inode(struct ap_inode_indicator *indc)
{
    struct ger_inode *ger_ind = (struct ger_inode *)indc->cur_inode->x_object;
    if (!ger_ind->is_dir) {
        return -1;
    }
    
    int finding_dir = indc->slash_remain == 0? 0:1;
    struct list_head *cusor;
    struct ger_inode *temp_inode;
    struct ap_inode *ap_inode;
    
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
    ap_inode = ger_alloc_ap_inode(temp_inode);
    indc->cur_inode = ap_inode;
    counter_put(temp_inode->in_use);
    return 0;
}


