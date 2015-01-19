#include <string.h>
#include <errno.h>
#include "ap_fs.h"
#include "ap_pthread.h"

static struct ap_inode root_dir = {
    .name = "",
    .is_dir = 1,
    .data_lock = PTHREAD_MUTEX_INITIALIZER,
    .ch_lock = PTHREAD_MUTEX_INITIALIZER,
    .children = LIST_HEAD_INIT(root_dir.children),
    .child = LIST_HEAD_INIT(root_dir.child),
};

struct ap_file_root f_root = {
    .f_root_lock = PTHREAD_MUTEX_INITIALIZER,
    .f_root_inode = &root_dir,
};

struct ap_file_systems f_systems = {
    .f_system_lock = PTHREAD_MUTEX_INITIALIZER,
    .i_file_system = LIST_HEAD_INIT(f_systems.i_file_system),
};

int walk_path(struct ap_inode_indicator *start)
{
    char *temp_path;
    char *cur_slash;
    char *path_end;
	char *path = start->path;
    
    size_t str_len = strlen(path);
    //当strlen为零时认为寻找的是当前工作目录 所以返回 0
    if (str_len == 0) {
        return 0;
    }

    struct ap_inode *cursor_inode = start->cur_inode;
    temp_path = (char *) malloc(str_len + 1);
    
    if (temp_path == NULL) {
        fprintf(stderr, "walk_path malloc_faile\n");
        exit(1);
    }
    
    path_end = temp_path + str_len;
    strncpy(temp_path, path, str_len+1);
    path = temp_path;
    
    cur_slash = strchr(path, '/');
    if (cur_slash == NULL) {
        cur_slash = path_end;
    }
    
AGAIN:
    while (1){
        if (!cursor_inode->is_dir) {
            errno = ENOTDIR;
            return -1;
        }
        
        struct list_head *_cusor;
        struct ap_inode *temp_inode;
        
        *cur_slash = '\0';
        start->slash_remain--;
        start->the_name = path;
        
        pthread_mutex_lock(&cursor_inode->ch_lock);
        
        list_for_each(_cusor, &cursor_inode->children){
           temp_inode = list_entry(_cusor, struct ap_inode, child);
            if (strcmp(temp_inode->name, path) == 0) {
                if (temp_inode->is_mount_point) {
                    temp_inode = temp_inode->real_inode;
                }
                ap_inode_put(start->cur_inode);
                start->cur_inode = temp_inode;
                ap_inode_get(start->cur_inode);
                
                path = cur_slash;
                if (path == path_end) {
                    pthread_mutex_unlock(&cursor_inode->ch_lock);
                    return 0;
                }
                
                cur_slash = strchr(++path, '/');
                if (cur_slash == NULL) {
                    cur_slash = path_end;
                }
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                cursor_inode = start->cur_inode;
                
                goto AGAIN;
            }
        }
        int get = cursor_inode->i_ops->get_inode(start);
        if (get) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            if (cur_slash == path_end) {
                start->p_state = stop_in_par;
            }else{
                start->p_state = stop_in_ance;
            }
            return -1;
        }
        
        list_add(&start->cur_inode->child, &cursor_inode->children);
        start->par = cursor_inode;
        start->cur_inode->links++;

        path = cur_slash;
        if (path == path_end) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            return 0;
        }
        cur_slash = strchr(++path, '/');
        if (cur_slash == NULL) {
            cur_slash = path_end;
        }
        
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

int register_fsyst(struct ap_file_system_type *fsyst)
{
    pthread_mutex_lock(&f_systems.f_system_lock);
    list_add(&fsyst->systems, &f_systems.i_file_system);
    pthread_mutex_unlock(&f_systems.f_system_lock);
    return 0;
}

