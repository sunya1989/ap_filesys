//
//  ger_fs.c
//  ap_file_system
//
//  Created by sunya on 14/12/24.
//  Copyright (c) 2014年 sunya. All rights reserved.
//
#include <ger_fs.h>
#include <ap_fs.h>
#include <counter.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ap_pthread.h>

static struct ap_file_operations ger_file_operations;
static struct ap_inode_operations ger_inode_operations;

static struct ap_inode *ger_alloc_inode(struct ger_stem_node *stem)
{
    struct ap_inode *ind;
    ssize_t n_len;
    ind = MALLOC_AP_INODE();
    n_len = strlen(stem->stem_name);
    ind->name = Mallocz(n_len + 1);
    memcpy(ind->name, stem->stem_name, n_len);
    ind->x_object = stem;
    
    counter_get(&stem->stem_inuse);
    ap_inode_get(ind);

    ind->is_dir = stem->is_dir;
    if (ind->is_dir) {
        ind->i_ops = &ger_inode_operations;
    }else{
        ind->f_ops = &ger_file_operations;
    }

    return ind;
}

void *ger_ger_mount_info(const char *path)
{
    char *d = strrchr(path, '/');
    int add = 0;
    size_t strl = strlen(d);
    struct ger_stem_node *node = MALLOC_STEM();
    add = strl == 1? 0:1;
    d += add;
    char *name = Mallocz(strl + 1);
    strncpy(name, d, strl);
    
    node->stem_name = name;
    node->is_dir = 1;
    return node;
}

static int ger_get_inode(struct ap_inode_indicator *indc)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)indc->cur_inode->x_object; //类型检查？
    struct ger_stem_node *temp_stem;
    const char *name = indc->the_name;
    struct list_head *cusor;
    
    if (stem->prepare_raw_data != NULL) {
        stem->prepare_raw_data(stem);
    }
    
    pthread_mutex_lock(&stem->ch_lock);
    list_for_each(cusor, &stem->children){
        temp_stem = list_entry(cusor, struct ger_stem_node, child);
        if (strcmp(temp_stem->stem_name, name) == 0) {
            counter_get(&temp_stem->stem_inuse);
            pthread_mutex_unlock(&stem->ch_lock);
            goto  FINDED;
        }
    }
    pthread_mutex_unlock(&stem->ch_lock);
    errno = ENOENT;
    return -1;
    
FINDED:
    ap_inode_put(indc->cur_inode);
    indc->cur_inode = ger_alloc_inode(temp_stem);
    indc->cur_inode->mount_inode = indc->cur_mtp;
    counter_get(&indc->cur_mtp->mount_p_counter);
    counter_put(&temp_stem->stem_inuse);
    return 0;
}

static ssize_t ger_read(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)file->x_object; //类型检查？
    if (stem->sf_ops->stem_read == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_read(stem, buf, off_set, len);
}

static ssize_t ger_write(struct ap_file *file, char *buf, off_t off_set, size_t len)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)file->x_object;
    if (stem->sf_ops->stem_read == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_write(stem, buf, off_set, len);
}

static int ger_release(struct ap_file *file,struct ap_inode *ind)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)file->x_object;
    counter_put(&stem->stem_inuse);
    file->x_object = NULL;
    
    if (stem->sf_ops->stem_release != NULL) {
        return stem->sf_ops->stem_release(stem);
    }
    return 0;
}

static int ger_open(struct ap_file *file, struct ap_inode *ind, unsigned long flags)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)ind->x_object; //类型检查？
    
    file->x_object = stem;
    counter_get(&stem->stem_inuse);
    
    if (stem->sf_ops->stem_open != NULL) {
      return stem->sf_ops->stem_open(stem, flags);
    }
    
    return 0;
}

static off_t ger_llseek(struct ap_file *file, off_t off_set, int origin)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)file->x_object; //类型检查？
    
    if (stem->sf_ops->stem_llseek == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stem->sf_ops->stem_llseek(stem, off_set, origin);
}

static int ger_unlink(struct ap_inode *ind)
{
    struct ger_stem_node *stem = ind->x_object; //类型检查??
    int o;

    if (stem->parent->si_ops->stem_unlink == NULL) {
        errno = EPERM;
        return -1;
    }
    ind->x_object = NULL;
    pthread_mutex_lock(&stem->parent->ch_lock);
    counter_put(&stem->stem_inuse);
    if (stem->stem_inuse.in_use != 0) {
        pthread_mutex_unlock(&stem->parent->ch_lock);
        errno = EPERM;
        return -1;
    }
    list_del(&stem->child);
    pthread_mutex_unlock(&stem->parent->ch_lock);
    if (stem->parent->si_ops != NULL && stem->parent->si_ops->stem_unlink != NULL) {
        o = stem->parent->si_ops->stem_unlink(stem);
    }
    return o;
}

static int ger_rmdir(struct ap_inode_indicator *indc)
{
    struct ap_inode *ind = indc->cur_inode;
    struct ger_stem_node *stem = ind->x_object; //类型检查??
   
    pthread_mutex_lock(&stem->ch_lock);
    if (!list_empty(&stem->children)) {
        pthread_mutex_unlock(&stem->ch_lock);
        errno = EBUSY;
        return -1;
    }
    
    pthread_mutex_lock(&stem->stem_inuse.counter_lock);
    if (stem->stem_inuse.in_use > 1) {
        pthread_mutex_unlock(&stem->stem_inuse.counter_lock);
        pthread_mutex_unlock(&stem->ch_lock);
        errno = EBUSY;
        return -1;
    }
    list_del(&stem->child);
    pthread_mutex_unlock(&stem->stem_inuse.counter_lock);
    pthread_mutex_unlock(&stem->ch_lock);
    if (stem->si_ops == NULL || stem->si_ops->stem_rmdir == NULL) {
        errno = EPERM;
        return -1;
    }
    counter_put(&stem->stem_inuse);
    ind->x_object = NULL;
    int o = stem->si_ops->stem_rmdir(stem);
    return o;
}

static int ger_mkdir(struct ap_inode_indicator *indc)
{
    struct ap_inode *ind = indc->cur_inode;
    struct ger_stem_node *stem = (struct ger_stem_node *)ind->x_object; //类型检查？
    struct ger_stem_node *new_stem;
    struct ap_inode *new_ind;
    
    if (stem->si_ops == NULL || stem->si_ops->stem_mkdir == NULL) {
        errno = EPERM;
        return -1;
    }

    new_stem = stem->si_ops->stem_mkdir(stem);
    if (new_stem == NULL) {
        return -1;
    }
    new_ind = ger_alloc_inode(new_stem);
    new_ind->links++;
    inode_add_child(indc->cur_inode, new_ind);
    return 0;
}

static int ger_destory(struct ap_inode *ind)
{
    struct ger_stem_node *stem = (struct ger_stem_node *)ind->x_object; //类型检查？
   
    if (stem != NULL && stem->si_ops->stem_destory != NULL) {
        struct ger_stem_node *i_stem = stem->is_dir? stem:stem->parent;
        counter_put(&stem->stem_inuse);
        ind->x_object = NULL;
        if (i_stem->si_ops != NULL && i_stem->si_ops->stem_destory != NULL) {
            return stem->si_ops->stem_destory(stem);
        }
    }
    return 0;
}

static struct ap_file_operations ger_file_operations = {
    .read = ger_read,
    .write = ger_write,
    .llseek = ger_llseek,
    .release = ger_release,
    .open = ger_open,
};

static struct ap_inode_operations ger_inode_operations = {
    .get_inode = ger_get_inode,
    .rmdir = ger_rmdir,
    .mkdir = ger_mkdir,
    .destory = ger_destory,
    .unlink = ger_unlink,
};

static struct ap_inode *gget_initial_inode(struct ap_file_system_type *fsyst, void *x_object)
{
    struct ger_stem_node *root_stem = (struct ger_stem_node *)x_object; //类型检查？
    struct ap_inode *ind = MALLOC_AP_INODE();
    char *name;
    ssize_t n_len;
    
    ind->x_object = root_stem;
    counter_get(&root_stem->stem_inuse);
    ind->is_dir = 1;
    
    n_len = strlen(root_stem->stem_name);
    name = malloc(n_len + 1);
    memcpy(name, root_stem->stem_name, n_len+1);
    
    root_stem->stem_name = name;
    ind->name = name;
    ind->i_ops = &ger_inode_operations;
    ind->links++;
    
    return ind;
}

struct ger_stem_node *find_stem_p(const char *p)
{
    SHOW_TRASH_BAG;
    struct ap_inode_indicator *final_indc = MALLOC_INODE_INDICATOR();
    struct ap_file_pthread *afp = pthread_getspecific(file_thread_key);
    struct ap_inode *s_inode;
    
    TRASH_BAG_PUSH(&final_indc->indic_bag);
    int set = initial_indicator(p, final_indc, afp);
    if (set == -1) {
        B_return(NULL);
    }
    
    int get = walk_path(final_indc);
    if (get == -1) {
        B_return(NULL);
    }
    if (!final_indc->cur_inode->is_dir) {
        B_return(NULL);
    }
    
    s_inode = final_indc->cur_inode;
    if (strcmp(s_inode->mount_inode->fsyst->name, GER_FILE_FS)) {
        B_return(NULL);
    }
    return s_inode->x_object;
}

struct ger_stem_node *find_stem_r(struct ger_stem_node *root_stem, char **names, int counts)
{
    int i = 0;
    char *name_cusor;
    struct ger_stem_node *stem_cusor = root_stem;
    struct ger_stem_node *temp_stem;
    struct list_head *child_cusor;

AGAIN:
    while (i<counts) {
        name_cusor = names[i];
       
        pthread_mutex_lock(&stem_cusor->ch_lock);
        list_for_each(child_cusor, &stem_cusor->children){
            temp_stem = list_entry(child_cusor, struct ger_stem_node, child);
            if (strcmp(temp_stem->stem_name, name_cusor) == 0) {
                if (i == counts-1) {
                    counter_get(&temp_stem->stem_inuse);
                    pthread_mutex_unlock(&stem_cusor->ch_lock);
                    return temp_stem;
                }
                counter_put(&stem_cusor->stem_inuse);
                stem_cusor = temp_stem;
                counter_get(&stem_cusor->stem_inuse);
                i++;
                goto AGAIN;
            }
        }
        pthread_mutex_unlock(&stem_cusor->ch_lock);
        return NULL;
    }
    pthread_mutex_unlock(&stem_cusor->ch_lock);
    return NULL;
}

void hook_to_stem(struct ger_stem_node *par, struct ger_stem_node *stem)
{
    pthread_mutex_lock(&par->ch_lock);
    list_add(&stem->child, &par->children);
    stem->parent = par;
    pthread_mutex_unlock(&par->ch_lock);
}

int init_fs_ger()
{
    struct ap_file_system_type *ger_fsyst = MALLOC_FILE_SYS_TYPE();
    ger_fsyst->name = GER_FILE_FS;
    
    ger_fsyst->get_initial_inode = gget_initial_inode;
    return register_fsyst(ger_fsyst);
}
