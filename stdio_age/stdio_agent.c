/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <ap_fs.h>
#include <envelop.h>
#include "stdio_agent.h"

static int age_dirprepare_raw_data(struct ger_stem_node *stem);
static struct stem_inode_operations std_age_inode_operations;
static struct stem_file_operations std_age_file_operations0;
static struct ap_file_operations std_age_file_operations1;

struct std_age_file_info{
    struct std_age *age;
    int fd;
    FILE *fs;
    enum file_state state;
};

static struct stem_inode_operations std_age_file_inode_operations;

void STD_AGE_INIT(struct std_age *age, char *tarf)
{
    STEM_INIT(&age->stem);
    
    size_t strl = strlen(tarf);
    char *name = Malloc_z(strl + 1);
    strncpy(name, tarf, strl);
    age->stem.stem_name = name;
    age->stem.sf_ops = &std_age_file_operations0;
    age->stem.si_ops = &std_age_file_inode_operations;
    age->target_file = tarf;
}

void STD_AGE_DIR_INIT(struct std_age_dir *age_dir, const char *tard)
{
    STEM_INIT(&age_dir->stem);
    size_t strl = strlen(tard);
    char *name = Malloc_z(strl + 1);
    strncpy(name, tard, strl);
    age_dir->stem.stem_name = name;
    age_dir->target_dir = tard;
    if (tard != NULL) 
        age_dir->stem.prepare_raw_data = age_dirprepare_raw_data;
    
    age_dir->stem.si_ops = &std_age_inode_operations;
}

static ssize_t stdio_age_read(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct std_age_file_info *info = file->x_object;
    ssize_t n_read;
    n_read = read(info->fd, buf, len);
    return n_read;
}

static ssize_t stdio_age_write(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct std_age_file_info *info = file->x_object;
    ssize_t n_write;
    n_write = write(info->fd, buf, len);
    return n_write;
}

static off_t stdio_age_llseek(struct ap_file *file, off_t off_set, int origin)
{
    struct std_age_file_info *info = file->x_object;
    off_t off_size;
    off_size = lseek(info->fd, off_set, origin);
    return off_size;
}

static int stdio_age_open(struct ger_stem_node *stem, unsigned long flag)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    struct std_age_file_info *info = Malloc_z(sizeof(info));
    struct ap_file *file = stem->x_object;
    info->fd = -1;
    info->fd = open(sa->target_file, (int)flag);
    if (stem != file->x_object) {
        ap_err("stem != file->x_object at stdio_age_open");
        exit(1);
    }
    counter_put(&stem->stem_inuse);
    file->x_object = info;
    file->f_ops = &std_age_file_operations1;
    return info->fd;
}

static int stdio_age_unlink(struct ger_stem_node *stem)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    STD_AGE_FREE(sa);
    return 0;
}

static int stdio_age_release(struct ap_file *file, struct ap_inode *inode)
{
    struct std_age_file_info *info = file->x_object;
    if (info->fd != -1) {
        close(info->fd);
    }
    free(info);
    file->x_object = NULL;
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
    char *tard = Malloc_z(len1 + len2 + 2);
    char *tard_cp = tard;
    
    memcpy(tard_cp, path1, len1);
    tard_cp += len1;
    if (*(tard_cp - 1) != '/') {
        *tard_cp++ = '/';
    }
    memcpy(tard_cp, path2, len2);
    
    return tard;
}

static int age_dirprepare_raw_data(struct ger_stem_node *stem)
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
        return 0;
    }
    
    dp = opendir(sa_dir->target_dir);
    if (dp == NULL) {
        return 0;
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
            cp_path = Malloc_z(str_len);
            memcpy(cp_path, path, str_len);
            
            lstat(cp_path, &stat_buf);
            
            if (S_ISREG(stat_buf.st_mode)) {
                tard  = combine_path(sa_dir->target_dir, cp_path);
                sa_temp = MALLOC_STD_AGE(tard);
                sa_temp->stem.stem_name = cp_path;
                sa_temp->stem.is_dir = 0;
                sa_temp->stem.stem_mode = 0777 & ~(ap_umask);
                hook_to_stem(stem, &sa_temp->stem);
                sa_temp->stem.parent = stem;
            }else if(S_ISDIR(stat_buf.st_mode)){
                tard = combine_path(sa_dir->target_dir, cp_path);
                sa_dir_temp = MALLOC_STD_AGE_DIR(tard);
                sa_dir_temp->stem.stem_name = cp_path;
                sa_dir_temp->stem.is_dir = 1;
                sa_dir_temp->stem.stem_mode = 0766 & ~(ap_umask);
                hook_to_stem(stem, &sa_dir_temp->stem);
                sa_dir_temp->stem.parent = stem;
            }else{
                free(cp_path);
            }
        }
        stem->raw_data_isset = 1;
    }
    
    if(closedir(dp) < 0){
        ap_err("close dir failed\n");
        exit(1);
    }
    chdir("-");
    return 0;
}

static struct ap_file_operations std_age_file_operations1 = {
    .read = stdio_age_read,
    .write = stdio_age_write,
    .llseek = stdio_age_llseek,
    .release = stdio_age_release,
};

static struct stem_file_operations std_age_file_operations0 = {
    .stem_open = stdio_age_open,
};

static struct stem_inode_operations std_age_inode_operations = {
    .stem_rmdir = stdio_age_rmdir,
    .stem_unlink = stdio_age_unlink,
};

static struct stem_inode_operations std_age_file_inode_operations = {
    .stem_unlink = stdio_age_unlink,
};

