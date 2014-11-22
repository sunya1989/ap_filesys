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
#include "ap_pthread.h"

static char *regular_path(char *path)
{
    char *slash;
    char *path_cursor = path;
    char *reg_path;
    
    size_t path_len = strlen(path);
    if (path_len == 0) {
        return NULL;
    }
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
    
    if (*path == '.') {
        if (!(*(path + 1) == '/' || (*(path+1) == '.' && *(path + 2) == '/'))) {
            return NULL;
        }
    }
    
    reg_path = path;
    if (*(path_end-1) == '/') {
        *(path_end-1) = '\0';
    }
    
    return reg_path;
}

static void initial_indicator(char *path, struct ap_inode_indicator *ind, struct ap_file_pthread *ap_fpthr)
{
    if (*path == '/') {
        path++;
        ind->path = path;
        ind->cur_inode = ap_fpthr->m_wd->cur_inode;
    }else if(*path == '.'){
        if (*(path + 1) == '/' || *(path + 1) == '\0') {
            path = *(path+1) == '/' ? path + 2 : path + 1;
            ind->path = path;
            ind->cur_inode = ap_fpthr->c_wd->cur_inode;
        }else{
            path = *(path+2) == '/' ? path + 3 : path + 2;
            ind->path = path;
            
            if (ap_fpthr->c_wd == ap_fpthr->m_wd) {
                ind->cur_inode = ap_fpthr->c_wd->cur_inode;
                return;
            }
            
            struct ap_inode *par = ap_fpthr->c_wd->cur_inode->parent;
            struct ap_inode *cur_inode = ap_fpthr->c_wd->cur_inode;
            ind->cur_inode = cur_inode->mount_inode == NULL ? par:cur_inode->mount_inode->parent;
        }
    }
   
}

int ap_open(char *path, int flags)
{
    int ap_fd;
    struct ap_file_pthread *ap_fpthr;
    
    if (!ap_fs_start) {
        fprintf(stderr, "ap_fs didn't start\n");
        exit(1);
    }
    
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file *file;
    struct ap_inode_indicator *final_inode;
    final_inode = MALLOC_INODE_INDICATOR();
    
    path = regular_path(path);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
    }
    
    initial_indicator(path, final_inode, ap_fpthr);
    
    int get = walk_path(final_inode);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    
    if (final_inode->cur_inode->is_dir) {
        errno = EISDIR;
        return -1;
    }
    
    file = malloc(sizeof(*file));
    if (file == NULL) {
        perror("ap_open malloc\n");
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


int ap_mount(void *mount_info, char *file_system, char *path)
{
    if (file_system == NULL || path ==NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_inode_indicator *par_indic;
    struct ap_inode *mount_point, *parent, *mount_inode, *temp_node;
    struct list_head *cusor;
    
    path = regular_path(path);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    par_indic = MALLOC_INODE_INDICATOR();
    mount_point = MALLOC_AP_INODE();
    
    mount_point->is_mount_point = 1;
    
    struct ap_file_system_type *ap_fst = find_filesystem(path);
    if (ap_fst == NULL) {
        fprintf(stderr, "file_system didn't find\n");
        errno = EINVAL;
        return -1;
    }
    
    mount_inode = ap_fst->get_initial_inode(ap_fst,mount_info);
    mount_point->real_inode = mount_inode;
    
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    
    char *last_slash = strrchr(path, '/');
    if (last_slash!=NULL && last_slash == path-1) {
        fprintf(stderr, "need mount path\n");
        errno = EINVAL;
        return -1;
    }
    
    last_slash = '\0';
    last_slash++;
    
    ssize_t len = strlen(last_slash);
    
    char *name = malloc(sizeof(len+1));
    if (name == NULL) {
        perror("malloc failed");
        return -1;
    }
    strlcpy(name, last_slash, len+1);
    
    initial_indicator(path, par_indic, ap_fpthr);
    
    int get = walk_path(par_indic);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    if (par_indic->cur_inode == NULL) {
        par_indic->cur_inode = f_root.f_root_inode;
        name = "/";
    }
    
    parent = par_indic->cur_inode;
    if (!parent->is_dir) {
        fprintf(stderr, "mount point isn't dir\n");
        errno = ENOTDIR;
        return -1;
    }
    pthread_mutex_lock(&parent->ch_lock);
    list_for_each(cusor, &parent->children){
        temp_node = list_entry(cusor, struct ap_inode, child);
        if (strcmp(name, temp_node->name) == 0) {
            list_add_tail(&mount_point->prev_mpoints, &temp_node->prev_mpoints);
            list_del(&temp_node->child);
            break;
        }
    }
    
    list_add(&mount_point->child, &parent->children);
    mount_point->links++;
    pthread_mutex_unlock(&parent->ch_lock);
    mount_point->name = name;
    
    ap_inode_put(parent);
    return 0;
}

























