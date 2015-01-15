//
//  stdio_agent.c
//  ap_file_system
//
//  Created by sunya on 14/12/26.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include "stdio_agent.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>


static void age_dirprepare_raw_data(struct ger_stem_node *stem);
static struct stem_inode_operations std_age_inode_operations;
static struct stem_file_operations std_age_file_operations;

void STD_AGE_INIT(struct std_age *age,char *tarf, enum file_state state)
{
    STEM_INIT(&age->stem);
    
    age->stem.sf_ops = &std_age_file_operations;
    age->fd = -1;
    age->fs = NULL;
    age->target_file = tarf;
    age->state = state;
}

void STD_AGE_DIR_INIT(struct std_age_dir *age_dir, char *tard)
{
    STEM_INIT(&age_dir->stem);
    age_dir->target_dir = tard;
    age_dir->stem.si_ops = &std_age_inode_operations;
    age_dir->stem.prepare_raw_data = age_dirprepare_raw_data;
}

ssize_t stdio_age_read(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_read;
    
    n_read = read(sa->fd, buf, len);
    return n_read;
}

ssize_t stdio_age_write(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_write;
    
    n_write = write(sa->fd, buf, len);
    return n_write;
}

off_t stdio_age_llseek(struct ger_stem_node *stem, off_t off_set, int origin)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    off_t off_size;
    
    off_size = lseek(sa->fd, off_size, origin);
    return off_size;
}

int stdio_age_open(struct ger_stem_node *stem, unsigned long flags)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    
    if (sa->fd != -1) {
        return sa->fd;
    }
    sa->fd = open(sa->target_file, (int)flags);
    return sa->fd;
}

int stdio_age_unlink(struct ger_stem_node *stem)
{
    pthread_mutex_lock(&stem->parent->ch_lock);
    counter_put(&stem->stem_inuse);
    if (stem->stem_inuse.in_use != 0) {
        pthread_mutex_unlock(&stem->parent->ch_lock);
        errno = EPERM;
        return -1;
    }
    list_del(&stem->child);
    pthread_mutex_unlock(&stem->parent->ch_lock);
    struct std_age *sa = container_of(stem, struct std_age, stem);
    STD_AGE_FREE(sa);
    
    return 0;
}

int stdio_age_rmdir(struct ger_stem_node *stem)
{
    pthread_mutex_lock(&stem->ch_lock);
    if (!list_empty(&stem->children)) {
         pthread_mutex_unlock(&stem->ch_lock);
         errno = EBUSY;
         return -1;
    }
    
    pthread_mutex_lock(&stem->stem_inuse.counter_lock);
    if (stem->stem_inuse.in_use > 0) {
        pthread_mutex_unlock(&stem->stem_inuse.counter_lock);
        pthread_mutex_unlock(&stem->ch_lock);
        errno = EBUSY;
        return -1;
    }
    pthread_mutex_unlock(&stem->stem_inuse.counter_lock);
    pthread_mutex_unlock(&stem->ch_lock);
    struct std_age_dir *sa_dir = container_of(stem, struct std_age_dir, stem);
    STD_AGE_DIR_FREE(sa_dir);
    return 0;
}

static void age_dirprepare_raw_data(struct ger_stem_node *stem)
{
    DIR *dp;
    struct dirent *dirp;
    struct std_age_dir *sa_dir_temp;
    struct std_age *sa_temp;
    struct stat stat_buf;
    size_t str_len;
    char *path;
    
    struct std_age_dir *sa_dir = container_of(stem, struct std_age_dir, stem); //类型检查？
    if (sa_dir->target_dir == NULL) {
        return;
    }
    
    dp = opendir(sa_dir->target_dir);
    if (dp == NULL) {
        return;
    }
    
    while ((dirp = readdir(dp)) != NULL) {
        path = dirp->d_name;
        if (strcmp(path, ".") == 0 ||
            strcmp(path, "..") == 0) {
            continue;
        }
        str_len = strlen(path) + 1;
        char *cp_path = malloc(str_len);
        strncpy(cp_path, path, str_len);
        
        lstat(cp_path, &stat_buf);
        
        if (S_ISREG(stat_buf.st_mode)) {
            sa_temp = MALLOC_STD_AGE(cp_path, g_fileno);
            sa_temp->stem.name = cp_path;
            hook_to_stem(stem, &sa_temp->stem);
        }else if(S_ISDIR(stat_buf.st_mode)){
            sa_dir_temp = MALLOC_STD_AGE_DIR(cp_path);
            sa_dir_temp->stem.name = cp_path;
            hook_to_stem(stem, &sa_dir_temp->stem);
        }else {
            continue;
        }
    }
    
    if(closedir(dp) < 0){
        perror("close dir failed\n");
        exit(1);
    }
    return;
}

static struct stem_file_operations std_age_file_operations={
    .stem_read = stdio_age_read,
    .stem_write = stdio_age_write,
    .stem_llseek = stdio_age_llseek,
    .stem_open = stdio_age_open,
};

static struct stem_inode_operations std_age_inode_operations={
    .stem_rmdir = stdio_age_rmdir,
    .stem_unlink = stdio_age_unlink,
};



