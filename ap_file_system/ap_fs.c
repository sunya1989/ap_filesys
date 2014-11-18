#include "ap_fs.h"
#include "ap_pthread.h"
#include <string.h>

struct ap_file_root f_root = {
    .f_root_lock = PTHREAD_MUTEX_INITIALIZER,
};

struct ap_file_systems f_systems = {
    .f_system_lock = PTHREAD_MUTEX_INITIALIZER,
};

int walk_path(struct ap_inode_indicator *start)
{
    char *temp_path;
    char *cur_slash;
    char *path_end;
    
	char *path = start->path;
    struct ap_inode *cursor_inode = start->cur_inode;
    
    size_t str_len = strlen(path);
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
    ap_inode_get(start->cur_inode);
    
AGAIN:
    while (1){
        struct list_head *_cusor;
        struct ap_inode *temp_inode;
        
        *cur_slash = '\0';
        
        pthread_mutex_lock(&cursor_inode->ch_lock);
        
        list_for_each(_cusor, &cursor_inode->children){
           temp_inode = list_entry(_cusor, struct ap_inode, child);
            if (strcmp(temp_inode->name, path) == 0) {
                if (temp_inode->is_mount_point) {
                    temp_inode = temp_inode->real_inode;
                }
                start->cur_inode = temp_inode;
                
                path = cur_slash;
                if (path == path_end) {
                    ap_inode_put(cursor_inode);
                    if (start->cur_inode->is_dir) {
                        pthread_mutex_unlock(&cursor_inode->ch_lock);
                        return -1;
                    }
                    ap_inode_get(start->cur_inode);
                    pthread_mutex_unlock(&cursor_inode->ch_lock);
                    return 0;
                }
                cur_slash = strchr(++path, '/');
                if (cur_slash == NULL) {
                    cur_slash = path_end;
                }
                
                ap_inode_put(cursor_inode);
                ap_inode_get(start->cur_inode);
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                cursor_inode = start->cur_inode;
                
                goto AGAIN;
            }
        }
        
        int get = cursor_inode->i_ops->get_inode(start);
        if (!get) {
            return -1;
        }

        path = cur_slash;
        if (path == path_end) {
            ap_inode_put(cursor_inode);
            if (start->cur_inode->is_dir) {
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                return -1;
            }
            ap_inode_get(start->cur_inode);
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            return 0;
        }
        cur_slash = strchr(++path, '/');
        if (cur_slash == NULL) {
            cur_slash = path_end;
        }
        
        ap_inode_put(cursor_inode);
        ap_inode_get(start->cur_inode);
        pthread_mutex_unlock(&cursor_inode->ch_lock);
        cursor_inode = start->cur_inode;
    }
}