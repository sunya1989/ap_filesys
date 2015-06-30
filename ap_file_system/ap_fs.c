#include <string.h>
#include <errno.h>
#include <ap_fs.h>
#include <ap_pthread.h>

static struct ap_file_operations root_file_operations;
static struct ap_inode_operations root_dir_operations;

#define extra_real_inode(inode) ((inode->is_mount_point)? (inode->real_inode):(inode))

static struct ap_inode root_dir = {
    .name = "root_test",
    .is_dir = 1,
    .links = 1,
    .data_lock = PTHREAD_MUTEX_INITIALIZER,
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
    .f_root_inode = &root_dir,
};

struct ap_file_systems f_systems = {
    .f_system_lock = PTHREAD_MUTEX_INITIALIZER,
    .i_file_system = LIST_HEAD_INIT(f_systems.i_file_system),
};

BAG_IMPOR_FREE(AP_INODE_INICATOR_FREE, struct ap_inode_indicator);
BAG_IMPOR_FREE(search_mtp_unlock, struct ap_inode);

int walk_path(struct ap_inode_indicator *start)
{
    char *temp_path;
	const char *path = start->path;
    int get;
    struct ap_inode *curr_mount_p = NULL;
    struct bag_head *mount_ps = MALLOC_BAG_HEAD();
        
    SHOW_TRASH_BAG;
    
    size_t str_len = strlen(path);
    //当strlen为零时认为寻找的是当前工作目录 所以返回 0
    if (str_len == 0) {
        return 0;
    }

    struct ap_inode *cursor_inode = start->cur_inode;
  
    start->slash_remain++;
    
    if (start->slash_remain > 1) {
        temp_path = (char *) Mallocz(str_len + 3);
        
        TRASH_BAG_RAW_PUSH(temp_path, free);
        
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
            B_return(-1);
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
                    BAG_RAW_PUSH(&curr_mount_p, BAG_search_mtp_unlock, mount_ps);
                }
                
                ap_inode_put(start->cur_inode);
                start->cur_inode = temp_inode->is_gate || temp_inode->is_mount_point?
                temp_inode->real_inode:temp_inode;
                ap_inode_get(start->cur_inode);
                
                if (start->slash_remain == 0) {
                    pthread_mutex_unlock(&cursor_inode->ch_lock);
                    BAG_EXCUTE(mount_ps);
                    B_return(0);
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
                start->cur_inode->mount_inode = curr_mount_p;
                pthread_mutex_unlock(&cursor_inode->ch_lock);
                add_inode_to_mt(start->cur_inode, curr_mount_p);
                BAG_EXCUTE(mount_ps);
                B_return(0);
            }
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(mount_ps);
            B_return(-1);
        }
        
        if (cursor_inode->i_ops->get_inode == NULL) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(mount_ps);
            B_return(-1);
        }
        
        get = cursor_inode->i_ops->get_inode(start);
        if (get) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(mount_ps);
            B_return(-1);
        }
        
        list_add(&start->cur_inode->child, &cursor_inode->children);
        add_inode_to_mt(start->cur_inode, curr_mount_p);
        start->cur_inode->parent = cursor_inode;
        start->cur_inode->links++;
        start->cur_inode->mount_inode = curr_mount_p;

        path = start->cur_slash;
        if (start->slash_remain == 0) {
            pthread_mutex_unlock(&cursor_inode->ch_lock);
            BAG_EXCUTE(mount_ps);
            B_return(0);
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
    pthread_mutex_lock(&f_systems.f_system_lock);
    list_add(&fsyst->systems, &f_systems.i_file_system);
    pthread_mutex_unlock(&f_systems.f_system_lock);
    return 0;
}

void inode_ipc_get(void *ind)
{
    struct ap_inode *inode = (struct ap_inode *)ind;
    ap_inode_get(inode);
    return;
}

void inode_ipc_put(void *ind)
{
    struct ap_inode *inode = (struct ap_inode *)ind;
    ap_inode_put(inode);
    return;
}

void iholer_destory(struct ipc_inode_holder *iholder)
{
    struct ap_hash *hash = iholder->ipc_file_hash;
    struct list_head *lis_pos1;
    struct list_head *lis_pos2;
    struct hash_union *un_pos;
    struct ap_file *file_pos;
    for (size_t i; i < hash->size; i++) {
        list_for_each_use(lis_pos1, lis_pos2, &hash->hash_table[i].hash_union_entry){
            list_del(lis_pos1);
            un_pos = list_entry(lis_pos1, struct hash_union, union_lis);
            file_pos = list_entry(un_pos, struct ap_file, f_hash_union);
            AP_FILE_FREE(file_pos);
        }
    }
}

const char *regular_path(const char *path, int *slash_no)
{
    char *slash;
    const char *path_cursor = path;
    const char *reg_path;
    
    size_t path_len = strlen(path);
    if (path_len == 0 || slash_no == NULL) {
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

