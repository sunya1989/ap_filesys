
#include "list.h"
#include <sys/types.h>
#include <pthread.h>

#define _OPEN_MAX 1024

int ap_fs_start = 0;

struct ap_inode_operations;
struct ap_file_operations;

struct ap_inode{
	char *name;
	int is_dir;
    int in_use;
	void *x_object;
	struct list_head inodes;
    pthread_rwlock_t ch_lock;
    struct list_head children;
    struct list_head child;
	struct ap_file_operations *f_ops;
	struct ap_inode_operations *i_ops;
};

struct ap_inode_indicator{
	char *path;
    int ker_fs, real_fd;
	struct ap_inode *cur_inode;
};

struct ap_inode_operations{
	int (*get_inode) (struct ap_inode_indicator *);
	int (*creat) (struct ap_inode_indicator *);
	int (*rmdir) (struct ap_inode_indicator *);
	int (*mkdir) ( struct ap_inode_indicator *);
};

struct ap_file{
	struct ap_inode *relate_i;
	int real_fd;
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
    
}*file_info;

extern int walk_path(struct ap_inode_indicator *start);

