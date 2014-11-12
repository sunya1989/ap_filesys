#include "ap_fs.h"
#include <string.h>
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
        perror("malloc_faile");
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
        struct list_head *_cusor;
        struct ap_inode *temp_inode;
        
        *cur_slash = '\0';
        
        list_for_each(_cusor, &cursor_inode->children){
           temp_inode = list_entry(_cusor, struct ap_inode, child);
            if (strcmp(temp_inode->name, path) == 0) {
               cursor_inode = start->cur_inode = temp_inode;
                path = cur_slash;
                if (path == path_end) {
                    return 0;
                }
                cur_slash = strchr(++path, '/');
                if (cur_slash == NULL) {
                    cur_slash = path_end;
                }
                goto AGAIN;
            }
        }
        
        int get = cursor_inode->i_ops->get_inode(start);
        if (!get) {
            return -1;
        }
        path = cur_slash;
        if (path == path_end) {
            return 0;
        }
        cur_slash = strchr(++path, '/');
        if (cur_slash == NULL) {
            cur_slash = path_end;
        }
    }
    
}