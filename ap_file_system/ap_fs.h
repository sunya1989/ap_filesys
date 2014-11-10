
#include "list.h"
#include <sys/types.h>

struct ap_inode_operations;
struct ap_file_operations;

struct ap_inode{
	char *name;
	int is_dir;
	void *x_object
	struct list_head inodes;
	struct ap_file_operations *f_ops;
	struct ap_inode_operation *i_ops;
};

struct ap_inode_indicator{
	char *path;
	struct ap_inode *cur_inode;
};

struct ap_inode_operations{
	int (*get_inode) (struct ap_inode_indicator *);
	int (*creat) (struct ap_inode_indicator *);
	int (*rmdir) (struct ap_inode_indicator *);
	int (*mkdir) ( struct ap_inode_indicator *);
};

struct ap_file{
	struct ap_inode_indicator relate_i;
	int real_fd;
	struct ap_file_operations *f_ops;
};

struct ap_file_operations{
	ssize_t (*read) (struct ap_file *, char *, off_t, size_t);
	ssize_t (*write) (struct ap_file *, char *, off_t, size_t);
	int (*readdir) (struct ap_file *, void *);
};

extern struct ap_inode_indicator *walk_path(struct ap_inode_indicator *start);

