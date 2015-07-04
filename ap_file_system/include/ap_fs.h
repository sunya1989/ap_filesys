#ifndef ap_file_system_ap_fs_h
#define ap_file_system_ap_fs_h

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <bag.h>
#include "counter.h"
#include "list.h"
#include "ap_hash.h"

#define _OPEN_MAX 1024
#define FULL_PATH_LEN 255
struct ap_inode_operations;
struct ap_file_operations;
struct ap_inode_indicator;

struct ap_inode{
	char *name;
	int is_dir;
    int is_mount_point,is_gate; //是否为挂载点,是否为link点
    int is_search_mt;
    
    struct ap_file_system_type *fsyst;
    
    struct ap_inode *real_inode; //point to the real entry
    struct ap_inode *mount_inode;
    struct ap_inode *parent;
    
    int links;
    struct counter inode_counter;
    struct counter mount_p_counter;
    
    unsigned long mode; //权限检查暂时还没有
    
	void *x_object;
	
    union{
        struct list_head mt_inode_h;
        struct list_head inodes;
    }mt_inodes;
    
    struct list_head children;
    struct list_head child;
    struct list_head mt_children;
    
    struct list_head mt_child;
    
    pthread_mutex_t inodes_lock;
    pthread_mutex_t mt_pass_lock;
    pthread_mutex_t mt_ch_lock;
    pthread_mutex_t data_lock; //目前用于保护link数据
    pthread_mutex_t ch_lock;
    
    struct hash_union ipc_path_hash;
    
    struct ap_inode *prev_mpoints;
	struct ap_file_operations *f_ops;
    struct ap_inode_operations *i_ops;
};

struct ap_inode_operations{
    int (*get_inode) (struct ap_inode_indicator *);
    int (*find_inode) (struct ap_inode_indicator *);
    int (*creat) (struct ap_inode_indicator *);
    int (*rmdir) (struct ap_inode_indicator *);
    int (*mkdir) (struct ap_inode_indicator *);
    int (*destory) (struct ap_inode *);
    int (*unlink) (struct ap_inode *);
};

struct ipc_inode_holder{
    struct ap_inode *inde;
    struct ap_hash *ipc_file_hash;
};

static inline struct ipc_inode_holder *MALLOC_IPC_INODE_HOLDER()
{
    struct ipc_inode_holder *hl = Mallocx(sizeof(*hl));
    hl->inde = NULL;
    hl->ipc_file_hash = NULL;
    return hl;
}

static inline int AP_INODE_INIT(struct ap_inode *inode)
{
    inode->name = NULL;
    inode->is_dir = inode->is_mount_point = inode->is_gate = 0;
    
    inode->mount_inode = inode->parent = inode->prev_mpoints = NULL;
    inode->real_inode = inode;
    inode->links = 0;
    inode->is_search_mt = 0;
    inode->fsyst = NULL;
    
    COUNTER_INIT(&inode->inode_counter);
    COUNTER_INIT(&inode->mount_p_counter);
    
    inode->x_object = NULL;
    
    INIT_LIST_HEAD(&inode->mt_inodes.inodes);
    INIT_LIST_HEAD(&inode->child);
    INIT_LIST_HEAD(&inode->children);
    INIT_LIST_HEAD(&inode->mt_children);
    INIT_LIST_HEAD(&inode->mt_child);
    
    INITIALIZE_HASH_UNION(&inode->ipc_path_hash);
    
    pthread_mutex_init(&inode->ch_lock, NULL);
    pthread_mutex_init(&inode->data_lock, NULL);
    pthread_mutex_init(&inode->mt_pass_lock, NULL);
    pthread_mutex_init(&inode->mt_ch_lock, NULL);
    pthread_mutex_init(&inode->mt_ch_lock, NULL);
  
    inode->f_ops = NULL;
    inode->i_ops = NULL;

    return 1;
}

BAG_DEFINE_FREE(AP_INODE_FREE);
static inline void AP_INODE_FREE(struct ap_inode *inode)
{
    struct ap_inode *i_d = inode->is_dir? inode:inode->parent;
    if (i_d->i_ops != NULL && i_d->i_ops->destory != NULL) {
        i_d->i_ops->destory(inode);
    }
    pthread_mutex_destroy(&inode->ch_lock);
    COUNTER_FREE(&inode->inode_counter);
    free(inode);
}

static inline struct ap_inode *MALLOC_AP_INODE()
{
    struct ap_inode *inode;
    inode = malloc(sizeof(*inode));
    if (inode == NULL) {
        perror("ap_inode malloc failed\n");
        exit(-1);
    }
    AP_INODE_INIT(inode);
    return inode;
}

static inline void ap_inode_get(struct ap_inode *inode)
{
    if (inode->mount_inode != NULL) {
        counter_get(&inode->mount_inode->mount_p_counter);
    }
    counter_get(&inode->inode_counter);
}

static inline void ap_inode_put(struct ap_inode *inode)
{
    counter_put(&inode->inode_counter);
    if (inode->mount_inode != NULL) {
        counter_put(&inode->mount_inode->mount_p_counter);
    }
    if (inode->links == 0 && inode->inode_counter.in_use == 0) {
        AP_INODE_FREE(inode);
    }
}

extern void iholer_destory(struct ipc_inode_holder *iholder);

static inline void IHOLDER_FREE(struct ipc_inode_holder *iholder)
{
    iholer_destory(iholder);
    ap_inode_put(iholder->inde);
    free(iholder);
}

struct ap_inode_indicator{
    char full_path[FULL_PATH_LEN];
	const char *path;
    int slash_remain;
    const char *the_name;
    char *name_buff;
    char *cur_slash;
    struct bag indic_bag;
    struct ap_inode *gate;
    struct ap_inode *cur_mtp;
	struct ap_inode *cur_inode;
};

BAG_DEFINE_FREE(AP_INODE_INICATOR_FREE);
static inline void AP_INODE_INDICATOR_INIT(struct ap_inode_indicator *indc)
{
    indc->path = NULL;
    indc->cur_inode = NULL;
    indc->the_name = NULL;
    indc->gate = NULL;
    indc->cur_mtp = NULL;
    indc->name_buff = NULL;
    indc->indic_bag.trash = indc;
    indc->indic_bag.release = BAG_AP_INODE_INICATOR_FREE;
    indc->indic_bag.is_embed = 1;
}

static inline void AP_INODE_INICATOR_FREE(struct ap_inode_indicator *indc)
{
    if (indc->cur_inode != NULL) {
         ap_inode_put(indc->cur_inode);
    }
    if (indc->gate != NULL) {
        ap_inode_put(indc->gate);
    }
    if (indc->name_buff != NULL) {
        free(indc->name_buff);
    }
    free(indc);
}

static inline struct ap_inode_indicator *MALLOC_INODE_INDICATOR()
{
    struct ap_inode_indicator *indic;
    indic = malloc(sizeof(*indic));
    if (indic == NULL) {
        perror("malloc failed");
        exit(1);
    }
    AP_INODE_INDICATOR_INIT(indic);
    return indic;
}

struct ap_file{
	struct ap_inode *relate_i;
    unsigned long mod;
    pthread_mutex_t file_lock;
	struct ap_file_operations *f_ops;
    off_t off_size;
    
    char *f_idec;
    
    struct hash_union f_hash_union;
    void *x_object;
};

static inline void AP_FILE_INIT(struct ap_file *file)
{
    file->f_ops = NULL;
    file->mod = 0;
    file->off_size = 0;
    INITIALIZE_HASH_UNION(&file->f_hash_union);
    int init = pthread_mutex_init(&file->file_lock, NULL);
    if (init != 0) {
        perror("file init fialed\n");
        exit(1);
    }
    file->f_ops = NULL;
}

static inline struct ap_file *AP_FILE_MALLOC()
{
    struct ap_file *apf;
    apf = malloc(sizeof(*apf));
    if (apf == NULL) {
        perror("ap_File malloc failed\n");
        exit(1);
    }
    AP_FILE_INIT(apf);
    return apf;
}

static inline void AP_FILE_FREE(struct ap_file *apf)
{
    pthread_mutex_destroy(&apf->file_lock);
    if (apf->relate_i != NULL) {
        ap_inode_put(apf->relate_i);
    }
    free(apf);
}

struct ap_file_operations{
	ssize_t (*read) (struct ap_file *, char *, off_t, size_t);
	ssize_t (*write) (struct ap_file *, char *, off_t, size_t);
    off_t (*llseek) (struct ap_file *, off_t, int);
    int (*release) (struct ap_file *,struct ap_inode *);
    int (*open) (struct ap_file *, struct ap_inode *, unsigned long);
	int (*readdir) (struct ap_file *, void *);
    int (*destory) (struct ap_inode *);
};

struct ap_file_struct{
    struct ap_file *file_list[_OPEN_MAX];
    unsigned long o_files;
    pthread_mutex_t files_lock;
};

extern struct ap_file_struct file_info;

struct ap_file_root{
    pthread_mutex_t f_root_lock;
    struct ap_inode *f_root_inode;
};

extern struct ap_file_root f_root;

struct ap_file_system_type{
    const char *name;
    struct list_head systems;
    struct list_head mts;
    struct counter fs_type_counter;
    struct ap_inode *(*get_initial_inode)(struct ap_file_system_type *, void *);
};

static inline struct ap_file_system_type *MALLOC_FILE_SYS_TYPE()
{
    struct ap_file_system_type *fsyst = malloc(sizeof(struct ap_file_system_type));
    if (fsyst == NULL) {
        perror("file_sys_type malloc filed\n");
        exit(1);
    }
    COUNTER_INIT(&fsyst->fs_type_counter);
    fsyst->name = NULL;
    fsyst->get_initial_inode =  NULL;
    return fsyst;
}

static inline void FILE_SYS_TYPE_FREE(struct ap_file_system_type *fsyst)
{
    COUNTER_FREE(&fsyst->fs_type_counter);
    free(fsyst);
}

struct ap_file_systems{
    pthread_mutex_t f_system_lock;
    pthread_mutex_t mt_lock;
    struct list_head mts;
    struct list_head i_file_system;
};

extern struct ap_file_systems f_systems;

static inline void
add_inodes_to_fsys(struct ap_inode *ind, struct ap_inode *mt_par)
{
    if (mt_par != NULL) {
        pthread_mutex_lock(&mt_par->mt_ch_lock);
        list_add(&ind->mt_child, &mt_par->children);
        pthread_mutex_unlock(&mt_par->mt_ch_lock);
    }
    pthread_mutex_lock(&f_systems.mt_lock);
    list_add(&ind->mt_child, &f_systems.mts);
    pthread_mutex_unlock(&f_systems.mt_lock);
}

static inline void add_inode_to_mt(struct ap_inode *inode, struct ap_inode *mt)
{
    pthread_mutex_lock(&mt->inodes_lock);
    list_add(&inode->mt_inodes.inodes, &mt->mt_inodes.mt_inode_h);
    pthread_mutex_unlock(&mt->inodes_lock);
}

static inline void del_inode_from_mt(struct ap_inode *inode)
{
    pthread_mutex_lock(&inode->mount_inode->inodes_lock);
    list_del(&inode->mt_inodes.inodes);
    pthread_mutex_unlock(&inode->mount_inode->inodes_lock);
}

static inline void del_inode_from_fsys(struct ap_file_system_type *fsyst, struct ap_inode *inod)
{
    pthread_mutex_lock(&f_systems.mt_lock);
    list_del(&inod->mt_child);
    pthread_mutex_unlock(&f_systems.mt_lock);
}

static inline void search_mtp_lock(struct ap_inode *inode)
{
    pthread_mutex_lock(&inode->mt_pass_lock);
    inode->is_search_mt = 1;
    pthread_mutex_unlock(&inode->mt_pass_lock);
}

BAG_DEFINE_FREE(search_mtp_unlock);
static inline void search_mtp_unlock(struct ap_inode *inode)
{
    pthread_mutex_lock(&inode->mt_pass_lock);
    inode->is_search_mt = 0;
    pthread_mutex_unlock(&inode->mt_pass_lock);
}

struct ap_file_pthread;

extern struct ap_file_systems f_systems;
extern void clean_inode_tides(struct ap_inode *inode);
extern int register_fsyst(struct ap_file_system_type *fsyst);
extern int walk_path(struct ap_inode_indicator *start);
extern void inode_add_child(struct ap_inode *parent, struct ap_inode *child);
extern void inode_del_child(struct ap_inode *parent, struct ap_inode *child);
extern struct ap_file_system_type *find_filesystem(char *fsn);
extern int initial_indicator(const char *path,
                             struct ap_inode_indicator *ind,
                             struct ap_file_pthread *ap_fpthr);
extern void inode_ipc_get(void *ind);
extern void inode_ipc_put(void *ind);
extern int decompose_mt(struct ap_inode *mt);
extern const char *regular_path(const char *path, int *slash_no);
#endif

