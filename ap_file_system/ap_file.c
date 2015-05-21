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
#include <ap_file.h>
#include <ap_fs.h>
#include <ap_pthread.h>
/*检查路径使得路径具有^[/](.*\/)*
 *的形式
 */
static char *regular_path(char *path, int *slash_no)
{
    char *slash;
    char *path_cursor = path;
    char *reg_path;
    
    size_t path_len = strlen(path);
    if (path_len == 0 || slash_no == NULL) {
        return NULL;
    }
    *slash_no = 0;
    char *path_end = path + path_len;
    
    while ((slash = strchr(path_cursor, '/')) != NULL) {
        if (*(++slash) == '/') {
            return NULL;
        }
        (*slash_no)++;
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
        (*slash_no)--;
    }
    
    return reg_path;
}


static int __initial_indicator(char *path, struct ap_inode_indicator *ind, struct ap_file_pthread *ap_fpthr)
{
    int slash_no;
    
    ind->path = path;
    ind->cur_inode = ap_fpthr->c_wd->cur_inode;
    
    path = regular_path(path, &slash_no);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    ind->slash_remain = slash_no;
    
    if (*path == '/') {
        path++;
        ind->cur_inode = ap_fpthr->m_wd->cur_inode;
        ind->slash_remain--;
    }else if(*path == '.'){
        if (*(path + 1) == '/' || *(path + 1) == '\0') {  // path 可能为./ ../ ../* ./*
            path = *(path+1) == '/' ? path + 2 : path + 1;
            ind->cur_inode = ap_fpthr->c_wd->cur_inode;
            ind->slash_remain--;
        }else{
            path = *(path+2) == '/' ? path + 3 : path + 2;
            ind->slash_remain--;
            if (ap_fpthr->c_wd == ap_fpthr->m_wd) {
                ind->cur_inode = ap_fpthr->c_wd->cur_inode;
                return 0;
            }
            struct ap_inode *par = ap_fpthr->c_wd->cur_inode->parent;
            struct ap_inode *cur_inode = ap_fpthr->c_wd->cur_inode;
            ind->cur_inode = cur_inode->mount_inode == NULL ? par:cur_inode->mount_inode->parent;
        }
    }
    ind->path = path;
    ap_inode_get(ind->cur_inode);
    return 0;
}

int initial_indicator(char *path,
                      struct ap_inode_indicator *ind,
                      struct ap_file_pthread *ap_fpthr)
{
     return __initial_indicator(path, ind, ap_fpthr);
}

#ifdef DEBUG
int export_initial_indicator(char *path, struct ap_inode_indicator *ind, struct ap_file_pthread *ap_fpthr)
{
    return __initial_indicator(path, ind, ap_fpthr);
}
extern void deug_regular_path(char *path, int *slash_no)
{
    regular_path(path, slash_no);
}

#endif
/*每当一个独立线程调用ap_open时都会产生新的file结构
 *以后能使用hash使得同一个文件对应一个fd（相对于不同线程来说）
 */
int ap_open(char *path, int flags)
{
    int ap_fd = 0;
    struct ap_file_pthread *ap_fpthr;
    
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file *file;
    struct ap_inode_indicator *final_inode;
    final_inode = MALLOC_INODE_INDICATOR();
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
   int set = __initial_indicator(path, final_inode, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    int get = walk_path(final_inode);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    
    if (final_inode->cur_inode->is_dir) {
        errno = EISDIR;
        return -1;
    }
    
    file = AP_FILE_MALLOC();
    AP_FILE_INIT(file);
    
    if (final_inode->cur_inode->f_ops->open != NULL) {
        int open_s;
        open_s = final_inode->cur_inode->f_ops->open(file, final_inode->cur_inode, flags);
        if (open_s == -1) {
            return -1;
        }
    }
    
    file->f_ops = final_inode->cur_inode->f_ops;
    file->relate_i = final_inode->cur_inode;
    ap_inode_get(final_inode->cur_inode);
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.o_files >= _OPEN_MAX) {
        errno = EMFILE;
        pthread_mutex_unlock(&file_info.files_lock);
        return -1;
    }
    for (int i=0; i<_OPEN_MAX; i++) {
        if (file_info.file_list[i] == NULL) {
            file_info.file_list[i] = file;
            ap_fd = i;
            break;
        }
    }
    file->mod = flags;
    pthread_mutex_unlock(&file_info.files_lock);
    AP_INODE_INICATOR_FREE(final_inode);
    return ap_fd;
}

int ap_mount(void *mount_info, char *file_system, char *path)
{
    if (file_system == NULL || path ==NULL) {
        errno = EINVAL;
        return -1;
    }
    int get;
    struct ap_inode_indicator *par_indic;
    struct ap_inode *mount_point, *parent, *mount_inode;
    char *name = NULL;
    int slash_no;
    
    path = regular_path(path, &slash_no);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    par_indic = MALLOC_INODE_INDICATOR();
    mount_point = MALLOC_AP_INODE();
    
    mount_point->is_mount_point = 1;
    
    struct ap_file_system_type *fsyst = find_filesystem(file_system);
    if (fsyst == NULL) {
        fprintf(stderr, "file_system didn't find\n");
        errno = EINVAL;
        return -1;
    }
    
    mount_inode = fsyst->get_initial_inode(fsyst,mount_info);
    mount_point->real_inode = mount_inode;
    mount_point->mount_inode = mount_point;
    
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    if (strlen(path) == 0) {
        par_indic->cur_inode = f_root.f_root_inode;
        name = "/";
        get = 0;
    }else{
        __initial_indicator(path, par_indic, ap_fpthr);
        get = walk_path(par_indic);
    }
    
    if ((get == -1 && par_indic->p_state == stop_in_ance) || !par_indic->cur_inode->is_dir) {
        errno = ENOENT;
        return -1;
    }
    
    if (!get) {
        //由于在还有子目录的情况下是不会释放结构的并且parent是本地变量 所以这里无需增加引用计数器
        parent = par_indic->cur_inode->parent;
    }else{
        parent = par_indic->cur_inode;
    }
    
    pthread_mutex_lock(&parent->ch_lock);
 
    if (!get){
        mount_point->prev_mpoints = par_indic->cur_inode;
        list_del(&par_indic->cur_inode->child);
    }
    
    list_add(&mount_point->child, &parent->children);
    mount_point->links++;
    pthread_mutex_unlock(&parent->ch_lock);
    mount_point->name = name;
    
    AP_INODE_INICATOR_FREE(par_indic);
    return 0;
}
/*任何一个线程调用close都会立即关闭描述符释放file结构
 *这样其它的线程用同一fd就访问不到此文件了
 */
int ap_close(int fd)
{
    if (fd < 0 || fd > _OPEN_MAX) {
        errno = EBADF;
        return -1;
    }
    
    struct ap_file *file;
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.file_list[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    file = file_info.file_list[fd];
    pthread_mutex_lock(&file->file_lock);
    file_info.file_list[fd] = NULL;
    pthread_mutex_unlock(&file->file_lock);
    pthread_mutex_unlock(&file_info.files_lock);
    
    if (file->f_ops->release != NULL) {
        file->f_ops->release(file, file->relate_i);
    }
    AP_FILE_FREE(file);
    return 0;
}

ssize_t ap_read(int fd, void *buf, size_t len)
{
    if (fd < 0 || fd > _OPEN_MAX) {
        errno = EBADF;
        return -1;
    }
    if (buf == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file *file;
    ssize_t h_read;
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.file_list[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    file = file_info.file_list[fd];
    pthread_mutex_lock(&file->file_lock);
    pthread_mutex_unlock(&file_info.files_lock);
    
    if (file->f_ops->read == NULL) {
        pthread_mutex_unlock(&file->file_lock);
        errno = EINVAL;
        return -1;
    }
    h_read = file->f_ops->read(file, buf, file->off_size, len);
    pthread_mutex_unlock(&file->file_lock);
    return h_read;
}

ssize_t ap_write(int fd, void *buf, size_t len)
{
    if (fd < 0 || fd > _OPEN_MAX) {
        errno = EBADF;
        return -1;
    }
    if (buf == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file *file;
    ssize_t h_write;
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.file_list[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    file = file_info.file_list[fd];
    pthread_mutex_lock(&file->file_lock);
    pthread_mutex_unlock(&file_info.files_lock);
    if (file->f_ops->write == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    h_write = file->f_ops->write(file, buf, file->off_size, len);
    pthread_mutex_unlock(&file->file_lock);
    return h_write;
}

off_t ap_lseek(int fd, off_t ops, int origin)
{
    if (fd < 0 || fd > _OPEN_MAX) {
        errno = EBADF;
        return -1;
    }
    struct ap_file *file;
    off_t now_off;
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.file_list[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    file = file_info.file_list[fd];
    pthread_mutex_lock(&file->file_lock);
    pthread_mutex_unlock(&file_info.files_lock);
    if (file->f_ops->llseek == NULL) {
        errno = ESPIPE;
        return -1;
    }
    now_off = file->f_ops->llseek(file, ops, origin);
    pthread_mutex_unlock(&file->file_lock);
    return now_off;
}

int ap_mkdir(char *path, unsigned long mode)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    struct ap_inode_indicator *par_indic;
    struct ap_file_pthread *ap_fpthr;
    int get, s_make;
    
    par_indic = MALLOC_INODE_INDICATOR();
    
    if (path == NULL || (*path == '/' && strlen(path) == 1)) {
        errno = EINVAL;
        return -1;
    }
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    int set = __initial_indicator(path, par_indic, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    get = walk_path(par_indic);
    
    if (!get) {
        errno = EEXIST;
        return -1;
    }
    if (!par_indic->cur_inode->is_dir) {
        errno = ENOTDIR;
        return -1;
    }
    
    if (par_indic->p_state != stop_in_par) {
        errno = EACCES;
        return -1;
    }
    
    if (!par_indic->cur_inode->i_ops->mkdir) {
        errno = EPERM;
        return -1;
    }
    
    s_make = par_indic->cur_inode->i_ops->mkdir(par_indic); //由此函数直接负责将ap_inode插入到链表中
    if (s_make == -1) {
        errno = EPERM;
        return -1;
    }
    AP_INODE_INICATOR_FREE(par_indic);
    return 0;
}

int ap_unlik(char *path)
{
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }
    
    struct ap_inode_indicator *final_indc;
    struct ap_inode *op_inode, *gate;
    struct ap_file_pthread *ap_fpthr;
    int link;
    final_indc = MALLOC_INODE_INDICATOR();
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    int set = __initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    if (final_indc->cur_inode->is_dir) {
        errno = EISDIR;
        return -1;
    }
    op_inode = final_indc->cur_inode; //op_inode is a inode_gate or a real_inode
    gate = final_indc->gate;
    if (gate!=NULL) {
        pthread_mutex_lock(&gate->parent->ch_lock);
        ap_inode_put(gate);
        if (gate->inode_counter.in_use != 0) {
            pthread_mutex_unlock(&gate->parent->ch_lock);
            errno = EPERM;
            return -1;
        }
        list_del(&gate->child);
        pthread_mutex_unlock(&gate->parent->ch_lock);
    }
    
    pthread_mutex_lock(&op_inode->parent->ch_lock); //因为父目录不为空所以不会被删除
    ap_inode_put(op_inode);
    if (op_inode->inode_counter.in_use != 0) {
        pthread_mutex_unlock(&op_inode->parent->ch_lock);
        errno = EPERM;
        return -1;
    }
    pthread_mutex_unlock(&op_inode->parent->ch_lock);
    pthread_mutex_lock(&op_inode->data_lock);
    link = op_inode->links--;
    pthread_mutex_unlock(&op_inode->data_lock);
    
    int o = 0;
    
    if (link == 0) {     //已经没有其它目录链接此文件
        if (op_inode->i_ops->unlink != NULL) {
            int unlik_s = op_inode->i_ops->unlink(op_inode);
            if (unlik_s == -1) {
                errno = EPERM;
                o = -1;
            }
        }
        AP_INODE_FREE(op_inode);
    }
    if (gate) {
        AP_INODE_FREE(gate);
    }
    AP_INODE_INICATOR_FREE(final_indc);
    return o;
}

int ap_link(char *l_path, char *t_path)
{
    if (l_path == NULL || t_path == NULL) {
        errno = EFAULT;
        return -1;
    }
    
    struct ap_inode_indicator *final_indc;
    struct ap_inode *op_inode;
    struct ap_file_pthread *ap_fpthr;
    struct ap_inode *inode_gate;
    
    final_indc = MALLOC_INODE_INDICATOR();
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    int set = __initial_indicator(l_path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    
    op_inode = final_indc->cur_inode;
    if (op_inode->is_dir) {
        errno = EISDIR;
        return -1;
    }
    
    pthread_mutex_lock(&op_inode->data_lock);
    op_inode->links++;
    pthread_mutex_unlock(&op_inode->data_lock);
    
    ap_inode_get(op_inode);
    ap_inode_put(final_indc->cur_inode);
    final_indc->cur_inode = NULL;
    
    set = __initial_indicator(t_path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    get = walk_path(final_indc);
    
    if (!get) {
        errno = EEXIST;
        return -1;
    }
    if (!final_indc->cur_inode->is_dir) {
        errno = ENOTDIR;
        return -1;
    }
    if (final_indc->p_state != stop_in_par) {
        errno = EACCES;
        return -1;
    }
    inode_gate = MALLOC_AP_INODE();
    inode_gate->is_gate = 1;
    inode_gate->real_inode = op_inode->is_gate? op_inode->real_inode:op_inode;
    
    inode_add_child(final_indc->cur_inode, inode_gate);
   
    ap_inode_put(op_inode);
    AP_INODE_INICATOR_FREE(final_indc);
    
    return 0;
}

static inline struct ap_inode *convert_to_mountp(struct ap_inode *ind)
{
    return ind->mount_inode == NULL? ind:ind->mount_inode;
}

static inline struct ap_inode *convert_to_real_ind(struct ap_inode *ind)
{
    return ind->real_inode == NULL? ind:ind->real_inode;
}

int ap_rmdir(char *path)
{
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }
    
    struct ap_inode_indicator *final_indc;
    struct ap_inode *op_inode, *parent;
    struct ap_file_pthread *ap_fpthr;
    int rm_s;
    
    final_indc = MALLOC_INODE_INDICATOR();
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    int set = __initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        return -1;
    }
    
    op_inode = final_indc->cur_inode;
    
    if (!op_inode->is_dir) {
        errno = ENOTDIR;
        return -1;
    }
    parent = convert_to_mountp(op_inode)->parent;
    pthread_mutex_lock(&parent->ch_lock);
    pthread_mutex_lock(&op_inode->ch_lock);
    if (!list_empty(&op_inode->children)){
        pthread_mutex_unlock(&op_inode->ch_lock);
        pthread_mutex_unlock(&parent->ch_lock);
        errno = EBUSY;
        return -1;
    }
    pthread_mutex_lock(&op_inode->inode_counter.counter_lock);
    if (op_inode->inode_counter.in_use > 1) {
        pthread_mutex_unlock(&op_inode->inode_counter.counter_lock);
        pthread_mutex_unlock(&parent->ch_lock);
        AP_INODE_INICATOR_FREE(final_indc);
        errno = EBUSY;
        return -1;
    }
    pthread_mutex_unlock(&op_inode->inode_counter.counter_lock);
    if (op_inode->i_ops->rmdir != NULL) {
        rm_s = op_inode->i_ops->rmdir(final_indc);
        if (rm_s != 0) {
            pthread_mutex_unlock(&op_inode->ch_lock);
            pthread_mutex_unlock(&parent->ch_lock);
            errno = EBUSY;
            return rm_s;
        }
    }
    
    op_inode = convert_to_mountp(op_inode);
    list_del(&op_inode->child);
    if (op_inode->prev_mpoints != NULL) {
        inode_add_child(parent, op_inode->prev_mpoints);
    }
    pthread_mutex_unlock(&parent->ch_lock);
    op_inode = convert_to_real_ind(op_inode);
    
    AP_INODE_INICATOR_FREE(final_indc);
    if (op_inode->mount_inode != NULL) {
        AP_INODE_FREE(op_inode->mount_inode);
    }
    AP_INODE_FREE(op_inode);
    return 0;
}




