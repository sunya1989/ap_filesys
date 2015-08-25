//
//  ap_file.c
//  ap_file_system
//
//  Created by HU XUKAI on 14/11/12.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ap_file.h>
#include <ap_fs.h>
#include <ap_pthread.h>
#include <bag.h>

/*每当一个独立线程调用ap_open时都会产生新的file结构
 *以后能使用hash使得同一个文件对应一个fd（相对于不同线程来说）
 */
int ap_open(const char *path, int flags)
{
    int ap_fd = 0;
    struct ap_file_pthread *ap_fpthr;
    
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    SHOW_TRASH_BAG;
    struct ap_file *file;
    struct ap_inode_indicator *final_indc;
    final_indc = MALLOC_INODE_INDICATOR();
    TRASH_BAG_PUSH(&final_indc->indic_bag);
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    int set = initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        B_return(-1);
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        B_return(-1);
    }
    
    if (final_indc->cur_inode->is_dir) {
        errno = EISDIR;
        B_return(-1);
    }
    
    file = AP_FILE_MALLOC();
    AP_FILE_INIT(file);
    
    if (final_indc->cur_inode->f_ops->open != NULL) {
        int open_s;
        open_s = final_indc->cur_inode->f_ops->open(file, final_indc->cur_inode, flags);
        if (open_s == -1) {
            B_return(-1);
        }
    }
    
    file->f_ops = final_indc->cur_inode->f_ops;
    file->relate_i = final_indc->cur_inode;
    ap_inode_get(final_indc->cur_inode);
    
    pthread_mutex_lock(&file_info.files_lock);
    if (file_info.o_files >= _OPEN_MAX) {
        errno = EMFILE;
        pthread_mutex_unlock(&file_info.files_lock);
        B_return(-1);
    }
    BAG_LOOSE(&file->file_bag);
    for (int i=0; i<_OPEN_MAX; i++) {
        if (file_info.file_list[i] == NULL) {
            file_info.file_list[i] = file;
            file_info.o_files++;
            ap_fd = i;
            break;
        }
    }
    file->mod = flags;
    pthread_mutex_unlock(&file_info.files_lock);
    B_return(ap_fd);
}

static int __ap_mount(void *m_info, struct ap_file_system_type *fsyst, const char *path)
{
    int get;
    struct ap_inode_indicator *par_indic;
    struct ap_inode *mount_point, *parent, *mount_inode;
    char *dir_name;
    
    par_indic = MALLOC_INODE_INDICATOR();
    mount_point = MALLOC_AP_INODE();
    
    mount_point->is_mount_point = 1;

    mount_inode = fsyst->get_initial_inode(fsyst,m_info);
    if (mount_inode == NULL) {
        return -1;
    }
    if (mount_inode->links == 0) {
        mount_inode->links++;
    }
    mount_point->real_inode = mount_inode;
    mount_inode->mount_inode = mount_point;
    
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    if((initial_indicator(path, par_indic, ap_fpthr)) == -1)
        return -1;
    
    get = walk_path(par_indic);
    
    if ((get == -1 && par_indic->slash_remain > 0) ||
        !par_indic->cur_inode->is_dir) {
        errno = ENOENT;
        AP_INODE_INICATOR_FREE(par_indic);
        return -1;
    }
    
    if (!get) {
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
    mount_point->parent = parent;
    mount_point->links++;
    pthread_mutex_unlock(&parent->ch_lock);
    
    size_t len = strlen(par_indic->full_path);
    char *mount_path = Mallocz(len + 1);
    memcpy(mount_path, par_indic->full_path, len);
    mount_point->name = mount_path;
    mount_point->fsyst = fsyst;
    add_mt_inodes(mount_point,par_indic->cur_mtp);
    mount_point->mount_inode = par_indic->cur_mtp;
    
    if (mount_inode->name == NULL) {
        dir_name = strrchr(path, '/');
        if (strcmp(path, "/")) {
            dir_name++;
        }
        len = strlen(dir_name);
        mount_inode->name = Mallocz(len + 1);
        strncpy(mount_inode->name, dir_name, len);
    }
    
    AP_INODE_INICATOR_FREE(par_indic);
    return 0;
}

extern int ap_mount2(char *file_system, const char *path)
{
    if (file_system == NULL || path == NULL) {
        errno = EINVAL;
        return -1;
    }
    const char *tmp_path;
    int slash_no;
    void *m_info;
    tmp_path = regular_path(path, &slash_no);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct ap_file_system_type *fsyst = find_filesystem(file_system);
    if (fsyst == NULL || fsyst->get_mount_info == NULL) {
        fprintf(stderr, "file_system didn't find\n");
        errno = EINVAL;
        return -1;
    }
    
    m_info = fsyst->get_mount_info(tmp_path);
    __ap_mount(m_info, fsyst, path);
    counter_put(&fsyst->fs_type_counter);
    return 0;
}

int ap_mount(void *m_info, char *file_system, const char *path)
{
    if (file_system == NULL || path ==NULL) {
        errno = EINVAL;
        return -1;
    }

    struct ap_file_system_type *fsyst = find_filesystem(file_system);
    if (fsyst == NULL) {
        fprintf(stderr, "file_system didn't find\n");
        errno = EINVAL;
        return -1;
    }
    __ap_mount(m_info, fsyst, path);
    counter_put(&fsyst->fs_type_counter);
    return 0;
}

int ap_unmount(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    SHOW_TRASH_BAG;
    struct ap_inode_indicator *ap_indic;
    int set;
    struct ap_inode *op_inode, *mount_p;
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        perror("ap_thread didn't find\n");
        exit(1);
    }
    
    ap_indic = MALLOC_INODE_INDICATOR();
    TRASH_BAG_PUSH(&ap_indic->indic_bag);
    set = initial_indicator(path, ap_indic, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        B_return(-1);
    }
    
    int get = walk_path(ap_indic);
    if (get == -1) {
        errno = ENOENT;
        B_return(-1);
    }
    
    if (!ap_indic->cur_inode->is_dir) {
        errno = ENOTDIR;
        B_return(-1);
    }
    
    if (ap_indic->cur_inode->mount_inode->real_inode !=
        ap_indic->cur_inode) {
        B_return(-1);
    }
    
    mount_p = ap_indic->cur_inode->mount_inode;
    op_inode = ap_indic->cur_inode;
    ap_inode_put(op_inode);
    ap_indic->cur_inode = NULL;
    int de = decompose_mt(mount_p);
    B_return(de);
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
    file_info.o_files--;
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
    ssize_t read_n;
    
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
    read_n = file->f_ops->read(file, buf, file->off_size, len);
    if (read_n > 0)
        file->off_size += read_n;
    pthread_mutex_unlock(&file->file_lock);
    return read_n;
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
    ssize_t write_n;
    
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
    
    write_n = file->f_ops->write(file, buf, file->off_size, len);
    if (write_n > 0)
        file->off_size += write_n;
    pthread_mutex_unlock(&file->file_lock);
    return write_n;
}

off_t ap_lseek(int fd, off_t ops, int origin)
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
    pthread_mutex_unlock(&file_info.files_lock);
    if (file->f_ops->llseek == NULL) {
        errno = ESPIPE;
        return -1;
    }
    file->off_size = file->f_ops->llseek(file, ops, origin);
    pthread_mutex_unlock(&file->file_lock);
    return file->off_size;
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
    
    int set = initial_indicator(path, par_indic, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    get = walk_path(par_indic);
    
    if (!get) {
        errno = EEXIST;
        AP_INODE_INICATOR_FREE(par_indic);
        return -1;
    }
    if (!par_indic->cur_inode->is_dir) {
        errno = ENOTDIR;
        AP_INODE_INICATOR_FREE(par_indic);
        return -1;
    }
    
    if (par_indic->slash_remain != 0) {
        errno = EACCES;
        AP_INODE_INICATOR_FREE(par_indic);
        return -1;
    }
    
    if (!par_indic->cur_inode->i_ops->mkdir) {
        errno = EPERM;
        AP_INODE_INICATOR_FREE(par_indic);
        return -1;
    }
    
    s_make = par_indic->cur_inode->i_ops->mkdir(par_indic); //由此函数直接负责将ap_inode插入到链表中
    AP_INODE_INICATOR_FREE(par_indic);
    if (s_make == -1) {
        return -1;
    }
    return 0;
}

int ap_unlink(const char *path)
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
    
    int set = initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        return -1;
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    if (final_indc->cur_inode->is_dir) {
        errno = EISDIR;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    op_inode = final_indc->cur_inode;
    gate = final_indc->gate;
    if (gate != NULL) {
        pthread_mutex_lock(&gate->mount_inode->inodes_lock);
        pthread_mutex_lock(&gate->parent->ch_lock);
        list_del(&gate->child);
        list_del(&gate->mt_inodes.inodes);
        inode_put_link(gate);
        pthread_mutex_unlock(&gate->parent->ch_lock);
        pthread_mutex_unlock(&gate->mount_inode->inodes_lock);
    }else{
        pthread_mutex_lock(&op_inode->mount_inode->inodes_lock);
        pthread_mutex_lock(&op_inode->parent->ch_lock);
        list_del(&op_inode->child);
        list_del(&op_inode->mt_inodes.inodes);
        pthread_mutex_unlock(&op_inode->parent->ch_lock);
        pthread_mutex_unlock(&op_inode->mount_inode->inodes_lock);
    }
    
    pthread_mutex_lock(&op_inode->data_lock);
    link = --op_inode->links;
    pthread_mutex_unlock(&op_inode->data_lock);
    
    int o = 0;
    
    if (link == 0) {     //已经没有其它目录链接此文件
        if (op_inode->parent->i_ops->unlink != NULL) {
            int unlik_s = op_inode->parent->i_ops->unlink(op_inode);
            if (unlik_s == -1) {
                errno = EPERM;
                o = -1;
            }
        }
    }
    AP_INODE_INICATOR_FREE(final_indc);
    return o;
}

int ap_link(const char *l_path, const char *t_path)
{
    if (l_path == NULL || t_path == NULL) {
        errno = EFAULT;
        return -1;
    }
    
    int set,get;
    
    struct ap_inode_indicator *gate_indc, *or_indc;
    struct ap_inode *op_inode;
    struct ap_file_pthread *ap_fpthr;
    struct ap_inode *inode_gate;
    struct ap_inode *gate_parent, *mt_p;
    char *gate_name;
    
    gate_indc = MALLOC_INODE_INDICATOR();
    or_indc = MALLOC_INODE_INDICATOR();
    
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (ap_fpthr == NULL) {
        fprintf(stderr, "ap_thread didn't find\n");
        exit(1);
    }
    
    set = initial_indicator(t_path, gate_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        AP_INODE_INICATOR_FREE(gate_indc);
        return -1;
    }
    
    get = walk_path(gate_indc);
    
    if (!get) {
        errno = EEXIST;
        AP_INODE_INICATOR_FREE(gate_indc);
        return -1;
    }
    if (!gate_indc->cur_inode->is_dir) {
        errno = ENOTDIR;
        AP_INODE_INICATOR_FREE(gate_indc);
        return -1;
    }
    if (gate_indc->slash_remain != 0) {
        errno = EACCES;
        AP_INODE_INICATOR_FREE(gate_indc);
        return -1;
    }
    
    size_t strl = strlen(gate_indc->the_name);
    gate_name = Mallocz(strl + 1);
    strncpy(gate_name, gate_indc->the_name, strl);
    
    gate_parent = gate_indc->cur_inode;
    ap_inode_get(gate_indc->cur_inode);
    mt_p = gate_indc->cur_mtp;
    AP_INODE_INICATOR_FREE(gate_indc);
    
    set = initial_indicator(l_path, or_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        AP_INODE_INICATOR_FREE(or_indc);
        return -1;
    }
    
    get = walk_path(or_indc);
    if (get == -1) {
        errno = ENOENT;
        AP_INODE_INICATOR_FREE(or_indc);
        return -1;
    }
    
    op_inode = or_indc->cur_inode;
    if (op_inode->is_dir) {
        errno = EISDIR;
        return -1;
    }
    
    inode_get_link(op_inode);
    inode_gate = MALLOC_AP_INODE();
    inode_gate->is_gate = 1;
    inode_gate->name = gate_name;
    inode_gate->links++;
    inode_gate->real_inode = op_inode->is_gate? op_inode->real_inode:op_inode;
    inode_gate->mount_inode = mt_p;
    add_inode_to_mt(inode_gate, mt_p);
    
    inode_add_child(gate_parent, inode_gate);
    inode_gate->parent = gate_parent;
    
    ap_inode_put(gate_parent);
    AP_INODE_INICATOR_FREE(or_indc);
    return 0;
}

int ap_rmdir(const char *path)
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
    
    int set = initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    
    op_inode = final_indc->cur_inode;
    
    if (!op_inode->is_dir) {
        errno = ENOTDIR;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    parent = convert_to_mountp(op_inode)->parent;
    pthread_mutex_lock(&op_inode->mount_inode->mt_ch_lock);
    pthread_mutex_lock(&parent->ch_lock);
    pthread_mutex_lock(&op_inode->ch_lock);
    if (!list_empty(&op_inode->children)){
        pthread_mutex_unlock(&op_inode->ch_lock);
        pthread_mutex_unlock(&parent->ch_lock);
        pthread_mutex_unlock(&op_inode->mount_inode->mt_ch_lock);
        errno = EBUSY;
        AP_INODE_INICATOR_FREE(final_indc);
        return -1;
    }
    pthread_mutex_lock(&op_inode->inode_counter.counter_lock);
    if (op_inode->inode_counter.in_use > 1) {
        pthread_mutex_unlock(&op_inode->inode_counter.counter_lock);
        pthread_mutex_unlock(&parent->ch_lock);
        pthread_mutex_unlock(&op_inode->ch_lock);
        pthread_mutex_unlock(&op_inode->mount_inode->mt_ch_lock);
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
            pthread_mutex_unlock(&op_inode->mount_inode->mt_ch_lock);
            errno = EBUSY;
            AP_INODE_INICATOR_FREE(final_indc);
            return rm_s;
        }
    }
    
    op_inode = convert_to_mountp(op_inode);
    clean_inode_tides(op_inode);
    op_inode->links--;

    if (op_inode->prev_mpoints != NULL) {
        list_add(&op_inode->prev_mpoints->child, &parent->children);
    }
    
    pthread_mutex_unlock(&op_inode->ch_lock);
    pthread_mutex_unlock(&parent->ch_lock);
    pthread_mutex_unlock(&op_inode->mount_inode->mt_ch_lock);
    op_inode = convert_to_real_ind(op_inode);
    
    if (op_inode->mount_inode->real_inode == op_inode) {
        AP_INODE_FREE(op_inode->mount_inode);
    }
    AP_INODE_INICATOR_FREE(final_indc);
    return 0;
}

int ap_chdir(const char *path)
{
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }
    struct ap_file_pthread *ap_fpthr;
    ap_fpthr = pthread_getspecific(file_thread_key);
    if (strcmp(path, AP_REWIND_DIR) == 0) {
        ap_inode_put(ap_fpthr->c_wd);
        ap_fpthr->c_wd = ap_fpthr->m_wd;
        return 0;
    }

    SHOW_TRASH_BAG;
    struct ap_inode_indicator *final_indc;
    final_indc = MALLOC_INODE_INDICATOR();
    TRASH_BAG_PUSH(&final_indc->indic_bag);
    
    int set = initial_indicator(path, final_indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        B_return(-1);
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        errno = ENOENT;
        B_return(-1);
    }
    if (!final_indc->cur_inode->is_dir) {
        errno = ENOTDIR;
        B_return(-1);
    }
    ap_inode_put(ap_fpthr->c_wd);
    ap_inode_get(final_indc->cur_inode);
    ap_fpthr->c_wd = final_indc->cur_inode;
    B_return(0);
}

AP_DIR *ap_open_dir(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    AP_DIR *dir;
    struct ap_inode_indicator *indc;
    struct ap_file_pthread *ap_fpthr;
    ap_fpthr = pthread_getspecific(file_thread_key);
    
    SHOW_TRASH_BAG;
    indc = MALLOC_INODE_INDICATOR();
    TRASH_BAG_PUSH(&indc->indic_bag);
    
    int set = initial_indicator(path, indc, ap_fpthr);
    if (set == -1) {
        errno = EINVAL;
        B_return(NULL);
    }
    
    int get = walk_path(indc);
    if (get == -1) {
        errno = ENOENT;
        B_return(NULL);
    }
    if (!indc->cur_inode->is_dir) {
        errno = ENOTDIR;
        B_return(NULL);
    }
    
    dir = MALLOC_AP_DIR();
    dir->dir_i = indc->cur_inode;
    ap_inode_get(indc->cur_inode);
    B_return(dir);
}

struct ap_dirent *ap_readdir(AP_DIR *dir)
{
    if (dir == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    struct ap_inode *inode = dir->dir_i;
    if (inode->i_ops->readdir == NULL) {
        errno = ESRCH;
        return NULL;
    }
    
    ssize_t read;
    struct ap_dirent *dirt;
    if (dir->d_buff_p == dir->d_buff_end) {
        if (dir->done) {
            dir->done = 0;
            return NULL;
        }
        memset(dir->d_buff, '\0', (dir->d_buff_end - dir->d_buff));
        dir->d_buff_p = dir->d_buff;
        read = inode->i_ops->
        readdir(inode, dir, dir->d_buff, DEFALUT_DIR_RD_ONECE_NUM);
        dir->d_buff_end = dir->d_buff + read;
        if (read <= 0) {
            errno = ENOENT;
            return NULL;
        }
    }
    dirt = (struct ap_dirent *)dir->d_buff_p;
    dir->d_buff_p += sizeof(*dirt);
    return dirt;
}

int ap_closedir(AP_DIR *dir)
{
    if (dir == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (dir->dir_i->i_ops->closedir != NULL) {
        dir->dir_i->i_ops->closedir(dir->dir_i);
    }
    AP_DIR_FREE(dir);
    return 0;
}

