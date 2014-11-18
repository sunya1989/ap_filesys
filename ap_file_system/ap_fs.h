
#include "list.h"
#include <sys/types.h>
#include <pthread.h>
#include "counter.h"

#define _OPEN_MAX 1024

int ap_fs_start = 0;

struct ap_inode_operations;
struct ap_file_operations;

struct ap_inode{
	char *name;
	int is_dir;
    int is_mount_point;
    
    struct ap_inode *real_inode;
    
    int links;
    struct counter inode_counter;
    
	void *x_object;
	struct list_head inodes;
    pthread_mutex_t ch_lock;
    struct list_head children;
    struct list_head child;
	struct ap_file_operations *f_ops;
    struct ap_inode_operations *i_ops;
};

static inline void ap_inode_get(struct ap_inode *inode)
{
    counter_get(&inode->inode_counter);
}

static inline void ap_inode_put(struct ap_inode *inode)
{
    counter_put(&inode->inode_counter);
}

struct ap_inode_indicator{
	char *path;
    int ker_fs, real_fd;
	struct ap_inode *cur_inode;
};

static inline void AP_INODE_INDICATOR_INIT(struct ap_inode_indicator *ind)
{
    ind->path = NULL;
    ind->ker_fs = ind->real_fd = 0;
    ind->cur_inode = NULL;
}

struct ap_inode_operations{
	int (*get_inode) (struct ap_inode_indicator *);
	int (*creat) (struct ap_inode_indicator *);
	int (*rmdir) (struct ap_inode_indicator *);
	int (*mkdir) ( struct ap_inode_indicator *);
};

struct ap_file{
	struct ap_inode *relate_i;
	int real_fd;
    unsigned long mod;
	struct ap_file_operations *f_ops;
};

struct ap_file_operations{
	ssize_t (*read) (struct ap_file *, char *, off_t, size_t);
	ssize_t (*write) (struct ap_file *, char *, off_t, size_t);
	int (*readdir) (struct ap_file *, void *);
};

struct ap_file_struct{
    struct ap_inode_indicator *m_wd, c_wd;/*work direct*/
    struct ap_file *file_list[_OPEN_MAX];
    unsigned long o_files;
    pthread_mutex_t files_lock;
}*file_info;

static inline void AP_FILE_INIT(struct ap_file *file)
{
    file->f_ops = NULL;
    file->real_fd = -1;
    file->mod = 0;
    file->f_ops = NULL;
}

struct ap_file_root{
    pthread_mutex_t f_root_lock;
    struct list_head f_root_inode;
};

extern struct ap_file_root f_root;

struct ap_file_system_type{
    const char *name;
    struct list_head systems;
    struct counter fs_type_counter;
    
    struct ap_inode *(*get_initial_inode)(struct ap_file_system_type *, void *);
    void (*off_the_tree)(struct ap_inode *);
};

struct ap_file_systems{
    pthread_mutex_t f_system_lock;
    struct list_head i_file_system;
    struct list_head i_inodes;
};

extern struct ap_file_systems f_systems;

extern int walk_path(struct ap_inode_indicator *start);














