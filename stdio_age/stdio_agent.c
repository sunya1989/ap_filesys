//
//  stdio_agent.c
//  ap_file_system
//
//  Created by HU XUKAI on 14/12/26.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//
#include "stdio_agent.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <envelop.h>

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

void STD_AGE_DIR_INIT(struct std_age_dir *age_dir, const char *tard)
{
    STEM_INIT(&age_dir->stem);
    age_dir->target_dir = tard;
    if (tard != NULL) {
        age_dir->stem.prepare_raw_data = age_dirprepare_raw_data;
    }
    age_dir->stem.si_ops = &std_age_inode_operations;
}

static ssize_t stdio_age_read(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_read;
    n_read = read(sa->fd, buf, len);
    return n_read;
}

static ssize_t stdio_age_write(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_write;
    n_write = write(sa->fd, buf, len);
    return n_write;
}

static off_t stdio_age_llseek(struct ger_stem_node *stem, off_t off_set, int origin)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    off_t off_size;
    
    off_size = lseek(sa->fd, off_set, origin);
    return off_size;
}

static int stdio_age_open(struct ger_stem_node *stem, unsigned long flags)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    
    if (sa->fd != -1) {
        return sa->fd;
    }
    sa->fd = open(sa->target_file, (int)flags);
    return sa->fd;
}

static int stdio_age_unlink(struct ger_stem_node *stem)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    STD_AGE_FREE(sa);
    return 0;
}

static int stdio_age_rmdir(struct ger_stem_node *stem)
{
    struct std_age_dir *sa_dir = container_of(stem, struct std_age_dir, stem);
    STD_AGE_DIR_FREE(sa_dir);
    return 0;
}

static inline char *combine_path(const char *path1, const char *path2){
    size_t len1  = strlen(path1);
    size_t len2 = strlen(path2);
    char *tard = Mallocz(len1 + len2 + 2);
    char *tard_cp = tard;
    
    memcpy(tard_cp, path1, len1);
    tard_cp += len1;
    if (*(tard_cp - 1) != '/') {
        *tard_cp++ = '/';
    }
    memcpy(tard_cp, path2, len2);
    
    return tard;
}

static void age_dirprepare_raw_data(struct ger_stem_node *stem)
{
    DIR *dp;
    struct dirent *dirp;
    char *tard, *cp_path = NULL;
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
    chdir(sa_dir->target_dir);
    if (stem->raw_data_isset == 0) {
        while ((dirp = readdir(dp)) != NULL) {
            path = dirp->d_name;
            if (strcmp(path, ".") == 0 ||
                strcmp(path, "..") == 0) {
                continue;
            }
            str_len = strlen(path) + 1;
            cp_path = Mallocz(str_len);
            memcpy(cp_path, path, str_len);
            
            lstat(cp_path, &stat_buf);
            
            if (S_ISREG(stat_buf.st_mode)) {
                tard  = combine_path(sa_dir->target_dir, cp_path);
                sa_temp = MALLOC_STD_AGE(tard, g_fileno);
                sa_temp->stem.stem_name = cp_path;
                sa_dir->stem.is_dir = 0;
                sa_dir->stem.stem_mode = 0777 & ~(022);
                hook_to_stem(stem, &sa_temp->stem);
                sa_temp->stem.parent = stem;
            }else if(S_ISDIR(stat_buf.st_mode)){
                tard = combine_path(sa_dir->target_dir, cp_path);
                sa_dir_temp = MALLOC_STD_AGE_DIR(tard);
                sa_dir_temp->stem.stem_name = cp_path;
                sa_dir_temp->stem.is_dir = 1;
                sa_dir->stem.stem_mode = 0766 & ~(022);
                hook_to_stem(stem, &sa_dir_temp->stem);
                sa_dir_temp->stem.parent = stem;
            }else{
                free(cp_path);
            }
        }
        stem->raw_data_isset = 1;
    }
    
    if(closedir(dp) < 0){
        perror("close dir failed\n");
        exit(1);
    }
    chdir("-");
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
