//
//  ap_file.c
//  ap_file_system
//
//  Created by sunya on 14/11/12.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "ap_file.h"
#include "ap_fs.h"

static char *regular_path(char *path)
{
    char *slash;
    char *path_cursor = path;
    char *reg_path;
    
    size_t path_len = strlen(path);
    char *path_end = path + path_len;
    
    while ((slash = strchr(path_cursor, '/')) != NULL) {
        if (*(slash++) == '/') {
            return NULL;
        }
        path_cursor = slash;
        if (path_cursor == path_end) {
            break;
        }
    }
    reg_path = path;
    if (*(path_end-1) == '/') {
        *(path_end-1) = '\0';
    }
    
    return reg_path;
}


int ap_open(char *path, int flags)
{
    int ap_fd;
    
    if (!ap_fs_start) {
        perror("ap_fs didn't start");
        exit(1);
    }
    
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file *file;
    struct ap_inode_indicator *final_inode;
    final_inode = malloc(sizeof(*final_inode));
    if (final_inode == NULL) {
        perror("ap_open malloc");
        exit(1);
    }
    
    AP_INODE_INDICATOR_INIT(final_inode);
        
    path = regular_path(path);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if (*path == '/') {
        path++;
        final_inode->path = path;
        final_inode->cur_inode = file_info->m_wd->cur_inode;
    }
    
    int get = walk_path(final_inode);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    
    file = malloc(sizeof(*file));
    if (file == NULL) {
        perror("ap_open malloc");
        exit(1);
    }
    
    AP_FILE_INIT(file);
    
    file->f_ops = final_inode->cur_inode->f_ops;
    file->relate_i = final_inode->cur_inode;
    if (final_inode->ker_fs) {
        file->real_fd = final_inode->real_fd;
    }
    
    pthread_mutex_lock(&file_info->files_lock);
    if (file_info->o_files >= _OPEN_MAX) {
        errno = EMFILE;
        pthread_mutex_unlock(&file_info->files_lock);
        return -1;
    }
    for (int i=0; i<_OPEN_MAX; i++) {
        if (file_info->file_list[i] == NULL) {
            file_info->file_list[i] = file;
            ap_fd = i;
            break;
        }
    }
    file->mod = flags;
    pthread_mutex_unlock(&file_info->files_lock);
    return ap_fd;
}

static struct ap_file_system_type *find_filesystem(char *fsn)
{
    struct list_head *cursor;
    struct ap_file_system_type *temp_sys;
    
    pthread_mutex_lock(&f_systems.f_system_lock);
    list_for_each(cursor, &f_systems.i_file_system){
        temp_sys = list_entry(cursor, struct ap_file_system_type, systems);
        if (strcmp(fsn, temp_sys->name) == 0) {
            counter_get(&temp_sys->fs_type_counter);
            pthread_mutex_unlock(&f_systems.f_system_lock);
            return temp_sys;
        }
    }
    
    return NULL;
}


extern int ap_mount(char *file_system, char *path)
{
    if (file_system == NULL || path ==NULL) {
        errno = EINVAL;
        return -1;
    }
    
    return 0;
}

























