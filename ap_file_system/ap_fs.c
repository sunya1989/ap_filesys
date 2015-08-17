//
//  ap_fs.c
//  ap_file_system
//
//  Created by HU XUKAI on 14/11/12.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//
#include <string.h>
#include <errno.h>
#include <ap_fs.h>
#include <ap_pthread.h>

static struct ap_file_operations root_file_operations;
static struct ap_inode_operations root_dir_operations;
static struct ap_inode root_dir;

#define extra_real_inode(inode) ((inode->is_mount_point)? (inode->real_inode):(inode))
static struct ap_inode root_mt = {
    .name = "",
    .is_mount_point = 1,
    .real_inode = &root_dir,
    .links = 1,
    .mt_pass_lock = PTHREAD_MUTEX_INITIALIZER,
    .mt_ch_lock = PTHREAD_MUTEX_INITIALIZER,
    .mt_children =LIST_HEAD_INIT(root_mt.mt_children),
};

static struct ap_inode root_dir = {
    .name = "root_t",
    .is_dir = 1,
    .links = 1,
    .mount_inode = &root_mt,
    .data_lock = PTHREAD_MUTEX_INITIALIZER,
    .inode_counter = INIT_COUNTER,
    .ch_lock = PTHREAD_MUTEX_INITIALIZER,
    .children = LIST_HEAD_INIT(root_dir.children),
    .child = LIST_HEAD_INIT(root_dir.child),
    .i_ops = &root_dir_operations,
    .f_ops = &root_file_operations,
};

struct ap_file_struct file_info = {
    .o_files = 0,
    .files_lock = PTHREAD_MUTEX_INITIALIZER,
};

struct ap_file_root f_root = {
    .f_root_lock = PTHREAD_MUTEX_INITIALIZER,
    .f_root_inode = &root_mt,
};

struct ap_file_systems f_systems = {
    .f_system_lock = PTHREAD_MUTEX_INITIALIZER,
    .i_file_system = LIST_HEAD_INIT(f_systems.i_file_system),
};

BAG_IMPOR_FREE(AP_INODE_INICATOR_FREE, struct ap_inode_indicator);
BAG_IMPOR_FREE(search_mtp_unlock, struct ap_inode);
BAG_IMPOR_FREE(AP_INODE_FREE, struct ap_inode);
BAG_IMPOR_FREE(AP_FILE_FREE, struct ap_file);

int walk_path(struct ap_inode_indicator *start)
{
    char *temp_path;
	const char *path = start->path;
    int get;
    struct ap_inode *curr_mount_p = start->cur_mtp;
    struct bag_head mount_ps = BAG_HEAD_INIT(mount_ps);
    
    size_t str_len = strlen(path);
    //当strlen为零时认为寻找的是当前工作目录 所以返回 0
    if (str_len == 0) {
        return 0;
    }

    struct ap_inode *cursor_inode = start->cur_inode;
    search_mtp_lock(curr_mount_p);
    BAG_RAW_PUSH(curr_mount_p, BAG_search_mtp_unlock, &mount_ps);
  
    start->slash_remain++;
    
    if (start->slash_remain > 1) {
        start->name_buff = temp_path = (char *) Mallocz(str_len + 3);
        
        if (*path == '/') {
            *temp_path = '/';
            temp_path += 1;
        }
        
        memcpy(temp_path, path, str_len+1);
        path = temp_path;
        
        start->cur_slash = strchr(path, '/');
        if (start->cur_slash == path) {
            path--;
        }
    }
    
AGAIN:
    while (1){
        if (!cursor_inode->is_dir) {
            errno = ENOTDIR;
            BAG_EXCUTE(&mount_ps);
            return(-1);
        }
        
        struct list_head *_cusor;
        struct ap_inode *temp_inode;
       
        start->slash_remain--;

        if (start->slash_remain > 0) {
            *start->cur_slash = '\0';
        }
        start->the_name = path;
                
        pthread_mutex_lock(&cursor_inode->ch_lock);
        list_for_each(_cusor, &cursor_inode->children){
           temp_inode = list_entry(_cusor, struct ap_inode, child);
            if (strcmp(extra_real_inode(temp_inode)->name, path) == 0) {
                if (temp_inode->is_gate) {
                    start->gate = temp_inode;
                    ap_inode_get(start->gate);
                }
                if (temp_inode->is_mount_point) {
                    curr_mount_p = temp_inode;
                    search_mtp_lock(curr_mount_p);
                    start->cur_mtp = curr_mount_p;
                    BAG_RAW_PUSH(curr_mount_p, BAG_search_mtp_unlock, &mount_ps);
                }
                
                ap_inode_put(start->cur_inode);
                start->cur_inode = temp_inode->is_gate || temp_inode->is_mount_point?
                temp_inode->real_inode:temp_inode;
                ap_inode_get(start->cur_inode);
                
                if (start->slash_remain == 0) {
                    pthread_mutex_unlock(&cursor_inode->ch_lock);
                    BAG_EXCUTE(&mount_ps);
                    return 0;
                }
                
                path = start->cur_slash;
                start->cur_slash = strchr(++path, '/');
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                cursor_inode = start->cur_inode;
                
                goto AGAIN;
            }
        }
        if(cursor_inode->i_ops->find_inode != NULL){
            get = cursor_inode->i_ops->find_inode(start);
            if (!get) {
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                add_inode_to_mt(start->cur_inode, curr_mount_p);
                BAG_EXCUTE(&mount_ps);
                return 0;
            }
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(&mount_ps);
            return -1;
        }
        
        if (cursor_inode->i_ops->get_inode == NULL) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(&mount_ps);
            return -1;
        }
        
        get = cursor_inode->i_ops->get_inode(start);
        if (get) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(&mount_ps);
            return -1;
        }
        
        list_add(&start->cur_inode->child, &cursor_inode->children);
        add_inode_to_mt(start->cur_inode, curr_mount_p);
        start->cur_inode->parent = cursor_inode;
        if (start->cur_inode->links == 0) {
            start->cur_inode->links++;
        }

        path = start->cur_slash;
        if (start->slash_remain == 0) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(&mount_ps);
            return 0;
        }
        start->cur_slash = strchr(++path, '/');
        pthread_mutex_unlock(&cursor_inode->ch_lock);
        cursor_inode = start->cur_inode;
    }
}

struct ap_file_system_type *find_filesystem(char *fsn)
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

void inode_add_child(struct ap_inode *parent, struct ap_inode *child)
{
    pthread_mutex_lock(&parent->ch_lock);
    list_add(&child->child, &parent->children);
    pthread_mutex_unlock(&parent->ch_lock);
}

void inode_del_child(struct ap_inode *parent, struct ap_inode *child)
{
    pthread_mutex_lock(&parent->ch_lock);
    list_del(&child->child);
    pthread_mutex_unlock(&parent->ch_lock);
}

int register_fsyst(struct ap_file_system_type *fsyst)
{
    if (fsyst->get_initial_inode == NULL ||
        fsyst->name == NULL) {
        return -1;
    }
    pthread_mutex_lock(&f_systems.f_system_lock);
    list_add(&fsyst->systems, &f_systems.i_file_system);
    pthread_mutex_unlock(&f_systems.f_system_lock);
    return 0;
}

void inode_ipc_get(void *ind)
{
    struct ipc_inode_holder *ihl = ind;
    ap_inode_get(ihl->inde);
    return;
}

void inode_ipc_put(void *ind)
{
    struct ipc_inode_holder *ihl = ind;
    ap_inode_put(ihl->inde);
    return;
}

static void free_o_file_in_byp(thrd_byp_t *byp)
{
    struct list_head *lis_pos1;
    struct list_head *lis_pos2;
    struct ap_file *file;
    list_for_each_use(lis_pos1, lis_pos2, &byp->file_o){
        list_del(lis_pos1);
        file = list_entry(lis_pos1, struct ap_file, ipc_file);
        AP_FILE_FREE(file);
    }
}

void THRD_BYP_FREE(thrd_byp_t *byp)
{
    pthread_mutex_destroy(&byp->file_lock);
    if (byp->dir_o != NULL) {
        AP_DIR_FREE(byp->dir_o);
    }
    free_o_file_in_byp(byp);
    free(byp);
}

void iholer_destory(struct ipc_inode_holder *iholder)
{
    struct ap_hash *hash = iholder->ipc_byp_hash;
    struct list_head *lis_pos1;
    struct list_head *lis_pos2;
    struct hash_union *un_pos;
    thrd_byp_t *byp;
    for (size_t i; i < hash->size; i++) {
        list_for_each_use(lis_pos1, lis_pos2, &hash->hash_table[i].hash_union_entry){
            list_del(lis_pos1);
            un_pos = list_entry(lis_pos1, struct hash_union, union_lis);
            byp = list_entry(un_pos, thrd_byp_t, h_un);
            THRD_BYP_FREE(byp);
        }
    }
}

static int check_decompose(struct ap_inode *mt)
{
    struct bag_head stack = BAG_HEAD_INIT(stack);
    struct ap_inode *pos0, *pos1;
    struct list_head *lis_pos;
    BAG_RAW_PUSH(mt, NULL, &stack);
    
    while (!BAG_EMPTY(&stack)) {
        pos0 = BAG_POP(&stack);
        if (pos0->mount_p_counter.in_use > 0) {
            return -1;
        }
        list_for_each(lis_pos, &pos0->mt_children){
            pos1 = list_entry(lis_pos, struct ap_inode, mt_child);
            BAG_RAW_PUSH(pos1, NULL, &stack);
        }
    }
    BAG_POUR(&stack);
    return 0;
}

static void __decompose_mt(struct ap_inode *mt)
{
    struct bag_head stack = BAG_HEAD_INIT(stack);
    struct ap_inode *pos_mt, *pos_inode, *pos_mt_c;
    struct list_head *lis_pos_inode, *lis_pos1, *lis_pos_mt;
    BAG_RAW_PUSH(mt, BAG_AP_INODE_FREE, &stack);
    
    while (!BAG_EMPTY(&stack)) {
        pos_mt = BAG_RES_POP(&stack);
        list_for_each_use(lis_pos_inode, lis_pos1, &pos_mt->mt_inodes.mt_inode_h){
            pos_inode = list_entry(lis_pos_inode, struct ap_inode, mt_inodes.inodes);
            list_del(lis_pos_inode);
            if (pos_inode->is_gate) {
                inode_put_link(pos_inode->real_inode);
            }
            if (pos_inode->links <= 1) {
                AP_INODE_FREE(pos_inode);
            }
        }
        list_for_each(lis_pos_mt, &pos_mt->mt_children){
            pos_mt_c = list_entry(lis_pos_mt, struct ap_inode, mt_child);
            BAG_RAW_PUSH(pos_mt_c, BAG_AP_INODE_FREE, &stack);
        }
    }
    BAG_EXCUTE(&stack);
    return;
}

int decompose_mt(struct ap_inode *mt)
{
    pthread_mutex_lock(&mt->mount_inode->mt_ch_lock);
    pthread_mutex_lock(&mt->parent->ch_lock);
    pthread_mutex_lock(&mt->mt_ch_lock);
    if (mt->is_search_mt || mt->mount_p_counter.in_use > 0) {
        errno = EBUSY;
        pthread_mutex_unlock(&mt->mt_ch_lock);
        pthread_mutex_unlock(&mt->parent->ch_lock);
        pthread_mutex_unlock(&mt->mount_inode->mt_ch_lock);
        return -1;
    }
    
    int de = check_decompose(mt);
    if (de == -1) {
        errno = EBUSY;
        pthread_mutex_unlock(&mt->mt_ch_lock);
        pthread_mutex_unlock(&mt->parent->ch_lock);
        pthread_mutex_unlock(&mt->mount_inode->mt_ch_lock);
        return -1;
    }
    list_del(&mt->mt_child);
    list_del(&mt->child);
    pthread_mutex_unlock(&mt->mt_ch_lock);
    pthread_mutex_unlock(&mt->parent->ch_lock);
    pthread_mutex_unlock(&mt->mount_inode->mt_ch_lock);
    __decompose_mt(mt);
    return 0;
}

void clean_inode_tides(struct ap_inode *inode)
{
    list_del(&inode->mt_child);
    list_del(&inode->mt_inodes.inodes);
    list_del(&inode->child);
}

static int __initial_indicator(const char *path, struct ap_inode_indicator *indc, struct ap_file_pthread *ap_fpthr)
{
    int slash_no;
    indc->cur_inode = ap_fpthr->c_wd;
    
    path = regular_path(path, &slash_no);
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    indc->slash_remain = slash_no;
    
    if (*path == '/') {
        indc->cur_inode = ap_fpthr->m_wd;
    }else if(*path == '.'){
        if (*(path + 1) == '/' || *(path + 1) == '\0') {  // path 可能为./ ../ ../* ./*
            path = *(path+1) == '/' ? path + 2 : path + 1;
            indc->cur_inode = ap_fpthr->c_wd;
            indc->slash_remain--;
        }else if(*(path+2) == '/'){
            path = path + 2;
            indc->slash_remain--;
            if (ap_fpthr->c_wd == ap_fpthr->m_wd) {
                indc->cur_inode = ap_fpthr->m_wd;
                goto COMPLETE;
            }
            struct ap_inode *par = ap_fpthr->c_wd->parent;
            struct ap_inode *cur_inode = ap_fpthr->c_wd;
            indc->cur_inode = cur_inode->mount_inode->real_inode == indc->cur_inode ?
            cur_inode->mount_inode->parent:par;
        }
    }
COMPLETE:
    indc->path = path;
    indc->cur_mtp = indc->cur_inode->mount_inode;
    strncpy(indc->full_path, path, strlen(path));
    ap_inode_get(indc->cur_inode);
    return 0;
}

int initial_indicator(const char *path,
                      struct ap_inode_indicator *ind,
                      struct ap_file_pthread *ap_fpthr)
{
    return __initial_indicator(path, ind, ap_fpthr);
}


const char *regular_path(const char *path, int *slash_no)
{
    char *slash;
    const char *path_cursor = path;
    const char *reg_path;
    
    size_t path_len = strlen(path);
    if (path_len == 0 || slash_no == NULL || path_len > FULL_PATH_LEN) {
        return NULL;
    }
    *slash_no = 0;
    const char *path_end = path + path_len;
    
    if (*path == '.') {
        if (!(*(path + 1) == '/' || (*(path+1) == '.' && *(path + 2) == '/'))) {
            return NULL;
        }
    }
    
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
    
    reg_path = path;
    if (*(path_end-1) == '/') {
        (*slash_no)--;
    }
    
    return reg_path;
}

