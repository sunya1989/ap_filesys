/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <envelop.h>
#include <errno.h>
#include <stdlib.h>
#include <list.h>
#include <sys/stat.h>
#include <convert.h>
#include <dirent.h>
#include <time.h>
#include <ap_fs.h>
#include <ap_hash.h>
#include <ap_string.h>
#include <ap_file.h>
#include <ap_pthread.h>
#include <ipc_protocol_headers.h>
#include "proc_fs.h"
#define IPC_PATH 0
#define PROC_NAME 1
#define IPC_KEY 2

struct ipc_holder_hash ipc_hold_table;
static struct ap_inode_operations procfs_inode_operations;
static struct ap_inode_operations procff_inode_operations;
static struct ap_inode_operations proc_inode_operations;
static struct ap_inode_operations proc_sub_inode_operations;

static struct ap_inode_operations proc_file_i_operations;
static struct ap_file_operations proc_file_operations;
static void ap_msg_send_err(struct ap_ipc_port *port, int err, int re_type);

static void ap_msg_send_err(struct ap_ipc_port *port, int err, int re_type)
{
    struct ap_msgreply re;
    re.rep_t.err = err;
    re.struct_l = 0;
    re.rep_t.re_type = -1;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
}

static char **pull_req(struct ap_msgreq *req)
{
    size_t data_len;
    char *p;
    char *cp;
    char **msg_req = Malloc_z(sizeof(char *) *req->index_lenth);
    size_t *list = (size_t *)req->req_detail;
    char *detail = (char *)(list + req->index_lenth);
    p = detail;
    for (int i = 0; i<req->index_lenth; i++) {
        data_len = list[i];
        cp = Malloc_z(data_len + 1);
        memcpy(cp, p, data_len);
        msg_req[i] = cp;
        p += data_len;
    }
    return msg_req;
}

static struct ap_msgbuf *
constr_req(void *buf, size_t buf_len, size_t list[], int lis_len, size_t *ttlen)
{
    *ttlen = sizeof(struct ap_msgbuf) + buf_len + sizeof(size_t)*lis_len;
    struct ap_msgbuf *msgbuf = (struct ap_msgbuf *)Malloc_z(*ttlen);
    msgbuf->req.index_lenth = lis_len;
    size_t *si = (size_t *)msgbuf->req.req_detail;
    char *cp = (char *)(si + lis_len);
    
    for (int i = 0; i < lis_len; i++)
        si[i] = list[i];
    
    memcpy(cp, buf, buf_len);
    
    return msgbuf;
}

static inline proc_byp_t *get_proc_byp(struct hash_identity ide)
{
    struct holder *hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL)
        return NULL;
    return (proc_byp_t *)hl->x_objext;
}

static void ihl_proc_byp_destory(void *object)
{
    proc_byp_destory(object);
}


static proc_byp_t *check_add_proc_byp(struct hash_identity ide)
{
    struct holder *hl;
    
    proc_byp_t *byp = get_proc_byp(ide);
    if (byp != NULL)
        return byp;
    
    byp = MALLOC_PROC_BYP();
    hl = MALLOC_HOLDER();
    hl->x_objext = byp;
    hl->ide.ide_c = ide.ide_c;
    hl->ide.ide_type.ide_i = ide.ide_type.ide_i;
    hl->destory = ihl_proc_byp_destory;
    ipc_holder_hash_insert(hl);
    return byp;
}

static struct ap_file *get_o_file(thrd_byp_t *byp, int ipc_fd)
{
    pthread_mutex_lock(&byp->file_lock);
    struct ap_file *pos;
    list_for_each_entry(pos, &byp->file_o, ipc_file){
        if (pos->ipc_fd == ipc_fd) {
            pthread_mutex_unlock(&byp->file_lock);
            return pos;
        }
    }
    pthread_mutex_unlock(&byp->file_lock);
    return NULL;
}

static inline struct ap_ipc_info *get_ipc_info(struct ap_inode *inode)
{
    return inode->is_ipc_base? inode->x_object : inode->parent->x_object;
}

static thrd_byp_t
*get_thr_byp(proc_byp_t *proc_byp, struct ipc_inode_ide *iide)
{
    struct hash_union *un;
    un = hash_union_get(proc_byp->ipc_byp_hash, iide->ide_t);
    if (un == NULL)
        return NULL;
    
    return container_of(un, thrd_byp_t, h_un);
}

static struct ap_file *look_up_file(struct ipc_inode_ide *iide)
{
    struct ap_file *file;
    thrd_byp_t *thr_byp;
    proc_byp_t *proc_byp;
    struct hash_identity tmp_ide;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL)
        return NULL;
    
    thr_byp = get_thr_byp(proc_byp, iide);
    if (thr_byp == NULL)
        return NULL;
    
    file = get_o_file(thr_byp, iide->fd);
    if (file == NULL)
        return NULL;
    return file;
}

static int creat_permission_file(char *path, mode_t mode)
{
    int fd = creat(path,mode);
    if (fd == -1) {
        return -1;
    }
    chmod(path, mode);
    
    close(fd);
    size_t strl = strlen(path);
    char *t_path = Malloc_z(strl + 1);
    strncpy(t_path, path, strl);
    BAG_RAW_PUSH(t_path, clean_file, &global_bag);
    return 0;
}

static inline void add_o_file(thrd_byp_t *byp, struct ap_file *file)
{
    pthread_mutex_lock(&byp->file_lock);
    list_add(&file->ipc_file, &byp->file_o);
    pthread_mutex_unlock(&byp->file_lock);
}

static inline void rest_iide(struct ipc_inode_ide *iide)
{
    iide->ide_p.ide_c = iide->chrs + iide->off_set_p;
    iide->ide_t.ide_c = iide->chrs + iide->off_set_t;
}

static void recode_proc_disc(struct ap_ipc_port *p, const char *sever_name)
{
    time_t time_n;
    int fd;
    char *p_p;
    
    fd = open(AP_PROC_DISC_F, O_CREAT | O_TRUNC | O_RDWR, 0777);
    if (fd == -1)
        return;

    const char *path[] ={
        AP_PROC_FILE,
        sever_name,
    };
    char full_path[AP_IPC_PATH_LEN];
    memset(full_path, '\0', AP_IPC_PATH_LEN);
    p_p = path_names_cat(full_path, path, 2, "/");
    time_n = time(NULL);
    snprintf(p_p, AP_IPC_PATH_LEN, ":%s:%ld\n",p->port_dis, time_n);
    Write_lock(fd, 0, SEEK_SET, 0);
    lseek(fd, 0, SEEK_END);
    write(fd, full_path, AP_IPC_PATH_LEN);
    Un_lock(fd, 0, SEEK_SET, 0);
    return;
}

static void inode_disc_free(struct ap_inode *inode)
{
    struct ap_inode *parent = inode->parent;
    struct ap_ipc_info *info = parent->x_object;
    decrease_hash_rsize(info->inde_hash_table);
    inode_put_link(inode);
    if (inode->links == 0){
        if (inode->i_ops != NULL && inode->i_ops->unlink != NULL)
            inode->i_ops->unlink(inode);
    }
}

static struct ipc_inode_ide *get_iide_path(const char *path, size_t *ttlen)
{
    struct ipc_inode_ide *iide;
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->ide_p.ide_type.pid = getpid();
    iide->ide_t.ide_type.thr_id = pthread_self();
    *ttlen = buf_l;
    return iide;
}

static void
handle_disc(struct ap_inode *inde, struct ap_ipc_port *p, const char *sever_name)
{
    recode_proc_disc(p, sever_name);
    if (inde == NULL) {
        return;
    }
    struct ap_inode *base = inde->is_ipc_base? inde:inde->parent;
    struct ap_ipc_info *info = base->x_object;
    if (base->parent == NULL) {
        inode_disc_free(inde);
        return;
    }
    inode_del_child(base->parent, base);
    base->parent = NULL;
    info->disc = 1;
    inode_disc_free(inde);
}

static int check_connect(struct ap_ipc_info *info, struct ap_inode *inode)
{
    struct ap_ipc_port *s_port;
    struct hash_identity ide;
    if (info->disc) {
        inode_disc_free(inode);
        errno = ENOENT;
        return -1;
    }
    s_port = info->port_pair->far_port;
    if (s_port->ipc_ops->ipc_probe != NULL&&
        s_port->ipc_ops->ipc_probe(s_port) == -1) {
        ide.ide_c = NULL;
        ide.ide_type.pid = getpid();
        struct holder *hl = ipc_holder_hash_get(ide, 0);
        ipc_holder_hash_delet(hl);
        hl->destory(hl->x_objext);
        errno = EBADF;
        handle_disc(inode, s_port, info->info_h->sever_name);
        return -1;
    }
    return 0;
}

static int ap_ipc_kick_start(struct ap_ipc_port *port, const char *path)
{
    int fd = creat(path, 0755);
    if (fd == -1)
        return -1;
    
    FILE *fs = fopen(path, "w+b");
    if (fs == NULL)
        return -1;
    
    size_t w_n = fputs(port->port_dis, fs);
    if (w_n < strlen(port->port_dis)) {
        unlink(path);
        return -1;
    }
    fclose(fs);
    close(fd);
    size_t strl = strlen(path);
    char *t_path = Malloc_z(strl + 1);
    strncpy(t_path, path, strl);
    BAG_RAW_PUSH(t_path, clean_file, &global_bag);
    return 0;
}

static struct ipc_inode_holder *
get_ihl_from_proc_byp(proc_byp_t *byp, struct hash_identity ide)
{
    struct list_head *pos;
    pthread_mutex_lock(&byp->iinode_lock);
    list_for_each(pos, &byp->ipc_iondes){
        struct ipc_inode_holder *ihl =
        list_entry(pos, struct ipc_inode_holder, ipc_proc_byp_lis);
        if (strcmp(ihl->iide.ide_c, ide.ide_c) == 0) {
            pthread_mutex_unlock(&byp->iinode_lock);
            return ihl;
        }
    }
    pthread_mutex_unlock(&byp->iinode_lock);
    return NULL;
}

static void
client_g(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ap_inode_indicator *indic;
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    char *path = iide->chrs + iide->off_set_p;
    struct ap_msgreply re;
    struct ap_inode_indicator *strc_cp;
    size_t len;
    size_t str_len = strlen(path);
    
    indic = MALLOC_INODE_INDICATOR();
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    int set = initial_indicator(path, indic, ap_fpthr);
    if (set == -1) {
        re.rep_t.err = EINVAL;
        re.rep_t.re_type = -1;
        re.struct_l = 0;
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    indic->pm_ide = req->req_t.f_o_info.pm_ide;
    len = sizeof(struct ap_msgreply) + sizeof(*indic) + sizeof(struct ap_inode);
    
    struct ap_msgreply *re_m = Mallocx(len);
    int get = walk_path(indic);
    
    re_m->rep_t.re_type = get;
    re_m->struct_l = 1;
    re_m->rep_t.err = errno;
    strc_cp = (struct ap_inode_indicator*)re_m->re_struct;
    *strc_cp = *indic;
    
    if (!get) {
        proc_byp_t *proc_byp;
        struct hash_identity c_ide;
        char *ide_c = Malloc_z(str_len + 1);
        strncpy(ide_c, path, str_len);
        c_ide.ide_c = NULL;
        c_ide.ide_type.ide_i = iide->ide_p.ide_type.pid;
        proc_byp = check_add_proc_byp(c_ide);
        struct ipc_inode_holder *ihl = MALLOC_IPC_INODE_HOLDER();
        ihl->inde = indic->cur_inode;
        ap_inode_get(indic->cur_inode);
        ihl->iide.ide_c = ide_c;
        
        pthread_mutex_lock(&proc_byp->iinode_lock);
        list_add(&ihl->ipc_proc_byp_lis, &proc_byp->ipc_iondes);
        pthread_mutex_unlock(&proc_byp->iinode_lock);
        
        *((struct ap_inode *)(strc_cp + 1)) = *indic->cur_inode;
    }
    port->ipc_ops->ipc_send(port, re_m, len, NULL);
    AP_INODE_INICATOR_FREE(indic);
    free(re_m);
    return;
}

static void client_rdir(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    struct ap_msgreply *re;
    AP_DIR *dir;
    thrd_byp_t *thr_byp;
    rest_iide(iide);
    size_t read_n,len;
    proc_byp_t *proc_byp;
    struct hash_identity tmp_ide;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    struct ipc_inode_holder *ihl = get_ihl_from_proc_byp(proc_byp, iide->ide_p);
    
    thr_byp = get_thr_byp(proc_byp, iide);
    if (thr_byp == NULL) {
        thr_byp = MALLOC_THRD_BYP();
        size_t str_l = strlen(iide->ide_t.ide_c);
        thr_byp->h_un.ide.ide_c = Malloc_z(str_l + 1);
        strncpy((char *)thr_byp->h_un.ide.ide_c, iide->ide_t.ide_c, str_l);
        thr_byp->h_un.ide.ide_type.thr_id = iide->ide_t.ide_type.thr_id;
        if(hash_union_insert_recheck(proc_byp->ipc_byp_hash, &thr_byp->h_un) != &thr_byp->h_un)
            THRD_BYP_FREE(thr_byp);
    }
    
    if (thr_byp->dir_o == NULL) {
        dir = MALLOC_AP_DIR();
        thr_byp->dir_o = dir;
        dir->dir_i = ihl->inde;
        ap_inode_get(dir->dir_i);
    }
    
    dir = thr_byp->dir_o;
    if (dir->d_buff != NULL)
        memset(dir->d_buff, '\0', DEFALUT_DIR_RD_ONECE_LEN);
    
    read_n = ihl->inde->i_ops->
    readdir(ihl->inde, dir, dir->d_buff, DIR_RD_ONECE_NUM(req->req_t.f_o_info.read_len));
    
    len = sizeof(struct ap_msgreply) + read_n + sizeof(int);
    re = Malloc_z(len);
    re->rep_t.re_type = read_n;
    re->rep_t.read_n = read_n;
    re->struct_l = 1;
    memcpy(re->re_struct, dir->d_buff, read_n);
    re->rep_t.readdir_is_done = dir->done;
    
    port->ipc_ops->ipc_send(port, re, len, NULL);
    free(re);
}

static void client_cdir(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    rest_iide(iide);
    struct ap_msgreply *re;
    thrd_byp_t *thr_byp;
    proc_byp_t *proc_byp;
    
    struct hash_identity tmp_ide;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    thr_byp = get_thr_byp(proc_byp, iide);
    if (thr_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    if (thr_byp->dir_o != NULL)
        AP_DIR_FREE(thr_byp->dir_o);
    
    re = Malloc_z(sizeof(*re));
    re->rep_t.re_type = 0;
    port->ipc_ops->ipc_send(port, re, sizeof(*re), NULL);
    free(re);
    return;
}

static void client_r(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    rest_iide(iide);
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply *re;
    ssize_t read_n;
    size_t len;
    char *read_buf = Malloc_z(req->req_t.f_o_info.read_len);
    
    file = look_up_file(iide);
    inde = file->relate_i;
    if (file == NULL){
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    if (file->f_ops == NULL || file->f_ops->read == NULL) {
        ap_msg_send_err(port, ESRCH, -1);
        return;
    }
    
    read_n = file->f_ops->
    read(file, read_buf, req->req_t.f_o_info.off_size, req->req_t.f_o_info.read_len);
    if (read_n <= 0) {
        ap_msg_send_err(port, errno, (int)read_n);
        return;
    }
    
    len = sizeof(struct ap_msgreply) + read_n;
    
    re = Malloc_z(len);
    re->rep_t.re_type = read_n;
    re->rep_t.read_n = read_n;
    re->struct_l = 1;
    memcpy(re->re_struct, read_buf, read_n);
    port->ipc_ops->ipc_send(port, re, len, NULL);
    free(re);
    return;
}

static void client_w(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    rest_iide(iide);
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply re;
    ssize_t write_n;
    
    file = look_up_file(iide);
    inde = file->relate_i;
    if (file == NULL){
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    if (file->f_ops == NULL || file->f_ops->write == NULL) {
        ap_msg_send_err(port, ESRCH, -1);
        return;
    }
    
    write_n = file->f_ops->
    write(file, req_d[1], req->req_t.f_o_info.off_size, req->req_t.f_o_info.wirte_len);
    if (write_n < 0) {
        ap_msg_send_err(port ,errno, -1);
        return;
    }
    
    re.rep_t.re_type = write_n;
    re.rep_t.write_n = write_n;
    re.struct_l = 0;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
    return;
}

static void client_o(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    rest_iide(iide);
    int open_s;
    struct ap_msgreply re;
    struct ap_inode *inde;
    struct ap_file *file;
    struct hash_union *un;
    thrd_byp_t *thr_byp;
    proc_byp_t *proc_byp;
    struct hash_identity tmp_ide;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    struct ipc_inode_holder *ihl = get_ihl_from_proc_byp(proc_byp, iide->ide_p);
    inde = ihl->inde;
    file = AP_FILE_MALLOC();
    AP_FILE_INIT(file);
    
    file->f_ops = inde->f_ops;
    
    if (inde->f_ops!= NULL && inde->f_ops->open != NULL) {
        open_s = inde->f_ops->open(file, inde, req->req_t.f_o_info.flags);
        if (open_s == -1) {
            ap_msg_send_err(port, errno, -1);
            AP_FILE_FREE(file);
            return;
        }
    }
    
    file->relate_i = inde;
    file->ipc_fd = (int)atomic_set_next_unset_bit(proc_byp->bitmap);
    ap_inode_get(inde);
    
    iide->ide_t.ide_c  = (iide->chrs + iide->off_set_t);
    un = hash_union_get(proc_byp->ipc_byp_hash, iide->ide_t);
    if (un == NULL) {
        thr_byp = MALLOC_THRD_BYP();
        size_t str_l = strlen(iide->ide_t.ide_c);
        thr_byp->h_un.ide.ide_c = Malloc_z(str_l + 1);
        strncpy((char *)thr_byp->h_un.ide.ide_c, iide->ide_t.ide_c, str_l);
        thr_byp->h_un.ide.ide_type.thr_id = iide->ide_t.ide_type.thr_id;
        if((un = hash_union_insert_recheck(proc_byp->ipc_byp_hash, &thr_byp->h_un)) !=
           &thr_byp->h_un){
            THRD_BYP_FREE(thr_byp);
        }
    }
    thr_byp = container_of(un, thrd_byp_t, h_un);
    add_o_file(thr_byp, file);
    
    re.rep_t.re_type = 0;
    re.ipc_fd = file->ipc_fd;
    re.struct_l = 0;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
}

static void client_c(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    rest_iide(iide);
    struct ap_inode *inde;
    struct ap_file *file;
    thrd_byp_t *thr_byp;
    proc_byp_t *proc_byp;
    struct ap_inode *inode;
    struct ap_msgreply re;
    struct hash_identity tmp_ide;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    struct ipc_inode_holder *ihl = get_ihl_from_proc_byp(proc_byp, iide->ide_p);
    inode = ihl->inde;
    thr_byp = get_thr_byp(proc_byp, iide);
    if (thr_byp == NULL){
        ap_msg_send_err(port, errno, -1);
        return;
    }
    
    file = get_o_file(thr_byp, iide->fd);
    if (file == NULL){
        ap_msg_send_err(port, errno, -1);
        return;
    }
    
    atomic_unset_bitmap(proc_byp->bitmap, file->ipc_fd);
    if (file->f_ops->release != NULL) {
       int rs = file->f_ops->release(file, inde);
        if (rs == -1) {
            ap_msg_send_err(port, errno, -1);
            return;
        }
    }
    pthread_mutex_lock(&thr_byp->file_lock);
    list_del(&file->ipc_file);
    AP_FILE_FREE(file);
    pthread_mutex_unlock(&thr_byp->file_lock);
    re.rep_t.re_type = 0;
    re.struct_l = 0;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
}

static void client_d(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct hash_identity ide;
    struct ipc_inode_ide *iide = (struct ipc_inode_ide *)req_d[0];
    char *path = iide->chrs + iide->off_set_p;
    struct ap_msgreply re;
    proc_byp_t *proc_byp;
    ide.ide_c = path;
    struct hash_identity tmp_ide;
    tmp_ide.ide_c = NULL;
    tmp_ide.ide_type.ide_i = 0;
    tmp_ide.ide_type.pid = iide->ide_p.ide_type.pid;
    
    proc_byp = get_proc_byp(tmp_ide);
    if (proc_byp == NULL) {
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    struct ipc_inode_holder *ihl = get_ihl_from_proc_byp(proc_byp, ide);
    pthread_mutex_lock(&proc_byp->iinode_lock);
    list_del(&ihl->ipc_proc_byp_lis);
    pthread_mutex_unlock(&proc_byp->iinode_lock);
    IPC_INODE_HOLDER_FREE(ihl);
    
    re.rep_t.re_type = 0;
    re.struct_l = 0;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
}

static void client_seek(struct ap_ipc_port *port, char **req_d, struct ap_msgreq *req)
{
    struct ipc_inode_ide *iide =  (struct ipc_inode_ide *)req_d[0];
    struct ap_file *file;
    struct ap_msgreply re;
    rest_iide(iide);
    file = look_up_file(iide);
    if (file == NULL){
        ap_msg_send_err(port, EINVAL, -1);
        return;
    }
    
    if (file->f_ops == NULL || file->f_ops->llseek == NULL) {
        ap_msg_send_err(port, ESPIPE, -1);
        return;
    }
    
    file->off_size = file->f_ops->
    llseek(file, req->req_t.f_o_info.off_size, req->req_t.f_o_info.orign);
    re.rep_t.re_type = file->off_size;
    re.struct_l = 0;
    port->ipc_ops->ipc_send(port, &re, sizeof(re), NULL);
}

static void *ap_proc_sever(void *arg)
{
    ssize_t recv_s;
    struct ap_ipc_port *lisen_port = arg;
    void *buf;
    char **req_detail;
    struct ap_ipc_port *p_port = Malloc_z(sizeof(*p_port));
    struct ap_msgbuf *msg_buf;
    op_type_t type;
    ap_file_thread_init();
    struct bag_head trashs = BAG_HEAD_INIT(trashs);
    
    if (lisen_port->ipc_ops->ipc_lisen != NULL)
        lisen_port->ipc_ops->ipc_lisen(lisen_port);
    while (1) {
        recv_s = lisen_port->ipc_ops->
        ipc_recv(lisen_port, &buf, AP_IPC_ONE_MSG_UNION, NULL, p_port);
        
        BAG_RAW_PUSH(buf, free, &trashs);
        msg_buf = (struct ap_msgbuf *)buf;
        type = msg_buf->req.req_t.op_type;
        req_detail = pull_req(&msg_buf->req);
        for (int i = 0; i<msg_buf->req.index_lenth; i ++)
            BAG_RAW_PUSH(req_detail[i], free, &trashs);
        
        switch (type) {
            case g:
                client_g(p_port, req_detail, &msg_buf->req);
                break;
            case w:
                client_w(p_port, req_detail, &msg_buf->req);
                break;
            case r:
                client_r(p_port, req_detail, &msg_buf->req);
                break;
            case o:
                client_o(p_port, req_detail, &msg_buf->req);
                break;
            case c:
                client_c(p_port, req_detail, &msg_buf->req);
                break;
            case d:
                client_d(p_port, req_detail, &msg_buf->req);
                break;
            case rdir:
                client_rdir(p_port, req_detail, &msg_buf->req);
                break;
            case cdir:
                client_cdir(p_port, req_detail, &msg_buf->req);
                break;
            case seek:
                client_seek(p_port, req_detail, &msg_buf->req);
                break;
            default:
                ap_msg_send_err(p_port, EINVAL, -1);
        }
        BAG_EXCUTE(&trashs);
        BAG_REWIND(&trashs);
        p_port->ipc_ops->ipc_close(p_port);
    }
}

static void ipc_hold_table_init()
{
    for (int i = 0; i < AP_IPC_HOLDER_HASH_LEN; i++) {
        pthread_mutex_init(&ipc_hold_table.hash_table[i].table_lock, NULL);
        INIT_LIST_HEAD(&ipc_hold_table.hash_table[i].holder);
    }
}

static struct ap_inode
*proc_get_initial_inode(struct ap_file_system_type *fsyst, void *x_object)
{
    pid_t pid = getpid();
    struct ap_inode *init_inode;
    struct proc_mount_info *m_info = x_object;
    int k_s;
    struct ap_ipc_operations *ops;
    pthread_t thr_n;
    size_t strl0 = strlen(AP_PROC_FILE);
    size_t strl1 = strlen(m_info->sever_name);
    size_t strl3 = strl0+strl1;
    struct ap_ipc_info_head *h_info;

    ipc_hold_table_init();
    
    umask(0000);
    int mk_s = mkdir(AP_PROC_FILE,S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
    umask(0022);

    if (mk_s == -1 && errno != EEXIST)
        return NULL;
    if (mk_s == 0)
        chmod(AP_PROC_FILE, S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
    
    char p_file[AP_IPC_PATH_LEN];
    memset(p_file, '\0', AP_IPC_PATH_LEN);
    const char *path[] = {
        AP_PROC_FILE,
        m_info->sever_name,
    };
    
    path_names_cat(p_file, path, 2, "/");
    int mkd = mkdir(p_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (mkd == -1 && errno != EEXIST)
        return NULL;
    
    
    snprintf(p_file + strl3 + 1, AP_IPC_PATH_LEN - strl3 -1, "/%ld_sever",(long)pid);
    h_info = MALLOC_IPC_INFO_HEAD();
    ops = ap_ipc_pro_ops[m_info->typ];
    if ((h_info->s_port = ops->ipc_get_port(p_file, 0777)) == NULL){
        IPC_INFO_HEAD_FREE(h_info);
        return NULL;
    }
    h_info->s_port->ipc_ops = ops;
    
    snprintf(p_file + strl3 + 1, AP_IPC_PATH_LEN - strl3 -1, "/%ld_client", (long)pid);
    get_ipc_c_port(m_info->typ, p_file);
    
    snprintf(p_file + strl3 + 1, AP_IPC_PATH_LEN - strl3 -1, "/permission#%ld",(long)pid);
    creat_permission_file(p_file, S_IRUSR | S_IWGRP);

    int thr_cr_s = pthread_create(&thr_n, NULL, ap_proc_sever, h_info->s_port);
	if (thr_cr_s == -1) {
		h_info->s_port->ipc_ops->ipc_close(h_info->s_port);
		IPC_PORT_FREE(h_info->s_port);
		IPC_INFO_HEAD_FREE(h_info);
		return NULL;
	}
	
    pthread_detach(thr_n);
    char *sever_name = Malloc_z(strl1 + 1);
    strncpy(sever_name, m_info->sever_name, strl1);
    h_info->sever_name = sever_name;
    
    snprintf(p_file + strl3 + 1, AP_IPC_PATH_LEN - strl3 -1, "/%s@%ld",m_info->sever_name,(long)pid);
    k_s = ap_ipc_kick_start(h_info->s_port, p_file);
    if (k_s == -1){
        h_info->s_port->ipc_ops->ipc_close(h_info->s_port);
        IPC_PORT_FREE(h_info->s_port);
        IPC_INFO_HEAD_FREE(h_info);
        return NULL;
    }
    
    //initialize inode
    init_inode = MALLOC_AP_INODE();
    init_inode->is_dir = 1;
    init_inode->links++;
    init_inode->x_object = h_info;
    init_inode->i_ops = &procfs_inode_operations;
    return init_inode;
}

/**
 *get the dir of certain kind of service under which can
 *have diffrent process porvide same service,
 *we can get the inode of the process with specified
 *pid by invoking procff_get_inode
 */
static int procfs_get_inode(struct ap_inode_indicator *indc)
{
    DIR *dp;
    struct dirent *dirp;
    char *path;
    const char *sever_name = indc->the_name;
    size_t strl = strlen(sever_name);
    struct ap_inode *inode;
    errno = 0;
    dp = opendir(AP_PROC_FILE);
    if (dp == NULL) {
        ap_err("ap_proc can't open the dir");
        goto FAILED;
    }
    
    while ((dirp = readdir(dp)) != NULL) {
        path = dirp->d_name;
        if (strncmp(path, ".", 1) == 0 ||
            strncmp(path, "..", 2) == 0) {
            continue;
        }
        if (strncmp(path, sever_name, strl) == 0) {
            inode = MALLOC_AP_INODE();
            inode->name = Malloc_z(strl +1);
            inode->is_dir = 1;
            inode->parent = indc->cur_inode;
            inode->mount_inode = indc->cur_inode->mount_inode;
            strncpy(inode->name, sever_name, strl);
            inode->i_ops = &procff_inode_operations;
            ap_inode_put(indc->cur_inode);
            ap_inode_get(inode);
            indc->cur_inode = inode;
            goto FIND;
        }
    }
FIND:
    closedir(dp);
    return 0;
FAILED:
    closedir(dp);
    return -1;
}

static int ipc_get_root(ppair_t *ppair)
{
    struct ap_ipc_port *s_port = ppair->far_port;
    struct ap_ipc_port *c_port = ppair->local_port;
    struct ap_msgbuf *buf;
    char *msg_reply;
    SHOW_TRASH_BAG;
    struct ipc_inode_ide *iide;
    size_t buf_l = sizeof(*iide) + 1;
    iide = Malloc_z(buf_l);
    iide->chrs[0] = '/';
    iide->ide_p.ide_type.pid = getpid();
    iide->ide_t.ide_type.thr_id = pthread_self();
    iide->off_set_p = iide->off_set_t = 0;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t lis[] = {buf_l};
    size_t ttlen;
    buf = constr_req(iide, buf_l, lis, 1, &ttlen);
    buf->req.req_t.op_type = g;
    TRASH_BAG_RAW_PUSH(buf, free);
    
    struct package_hint hint;
    ssize_t sent_s = s_port->ipc_ops->
    ipc_send(s_port, buf, ttlen, &hint);
    if (sent_s == -1)
        B_return(-1);
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    ssize_t recv_s = s_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1) 
        B_return(-1);
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
    return (int)msgre->rep_t.re_type;
}

static pmide_t get_permission_ide(const char *path)
{
    int fd;
    if ((fd = open(path, O_RDONLY)) != -1){
        close(fd);
        return user;
    }
    if ((fd = open(path, O_WRONLY)) != -1){
        close(fd);
        return gruop;
    }
    return other;
}

/**
 *get the inode of the process with respect to certain pid
 */
static int procff_get_inode(struct ap_inode_indicator *indc)
{
    FILE *fs;
    struct ap_ipc_info *info;
    struct ap_inode *inode;
    struct ap_ipc_port *s_port;
    ppair_t *port_pair;
    enum connet_typ c_typ;
    struct ap_ipc_info_head *h_info;
    h_info = indc->cur_inode->parent->x_object;
    size_t strl1 = strlen(indc->the_name);
    char full_p[AP_IPC_PATH_LEN];
    char sever_name[AP_IPC_RECODE_LEN];
    char sever_file[AP_IPC_RECODE_LEN];
    memset(full_p, '\0', AP_IPC_PATH_LEN);
    memset(sever_name, '\0', AP_IPC_RECODE_LEN);
    memset(sever_file, '\0', AP_IPC_RECODE_LEN);
    
    char *at = strchr(indc->the_name, AT);
    size_t strl0 = at - indc->the_name;
    
    strncpy(sever_name, indc->the_name, strl0);
    strncpy(sever_file, indc->the_name, strl1);
    
    const char *s_paths[] = {
        AP_PROC_FILE,
        sever_name,
        sever_file,
    };
    
    path_names_cat(full_p, s_paths, 3,"/");
    fs = fopen(full_p, "r+b");
    if (fs == NULL)
        return -1;
    
    char *port_dis = Malloc_z(AP_IPC_RECODE_LEN);
    if(fgets(port_dis, AP_IPC_RECODE_LEN, fs) == NULL){
        fclose(fs);
        return -1;
    }
    fclose(fs);
    
    char *d = strchr(port_dis, ':');
    if (d == NULL) {
        free(port_dis);
        return -1;
    }
    
    *d = '\0';
    c_typ = atoi(port_dis);
    *d = ':';
    
    s_port = MALLOC_IPC_PORT();
    s_port->port_dis = port_dis;
    s_port->ipc_ops = ap_ipc_pro_ops[c_typ];
    
    size_t strl3 = strlen(AP_PROC_FILE);
    snprintf(full_p+strl3, AP_IPC_PATH_LEN - strl3, "/%s/%ld_client",h_info->sever_name,(long)getpid());
    if ((port_pair = s_port->ipc_ops->ipc_connect(s_port, full_p)) == NULL) {
        handle_disc(NULL, s_port, sever_name);
        free(port_dis);
        return -1;
    }
    
    
    int get_r = ipc_get_root(port_pair);
    if (get_r == -1)
        return -1;
    
    info = MALLOC_IPC_INFO();
    inode = MALLOC_AP_INODE();
    info->port_pair = port_pair;
    info->c_t = c_typ;
    info->info_h = h_info;
    snprintf(full_p+strl3+strl0, AP_IPC_PATH_LEN - strl3 - strl0, "/permission#%s",(h_info->sever_name+strl0+1));
    info->pm_id = get_permission_ide(full_p);
    
    inode->name = Malloc_z(strl1 + 1);
    strncpy(inode->name, indc->the_name, strl1);
    inode->parent = indc->cur_inode;
    inode->mount_inode = indc->cur_inode->mount_inode;
    inode->i_ops = &proc_inode_operations;
    inode->x_object = info;
    inode->is_ipc_base = 1;
    inode->links++;
    inode->is_dir = 1;
    inode->ipc_path_hash.ide.ide_c = "/";
    inode->ipc_path_hash.ide.ide_type.ide_i = 0;
    ap_inode_put(indc->cur_inode);
    ap_inode_get(inode);
    indc->cur_inode = inode;
    free(port_dis);
    return 0;
}

static int get_proc_inode(struct ap_inode_indicator *indc)
{
    size_t str_len;
    struct ap_msgbuf *buf;
    char *msg_reply;
    struct ap_inode *cur_ind, *f_ind;
    struct ap_inode_indicator *f_indc = NULL;
    struct ap_ipc_info *info;
    
    pid_t pid = getpid();
    pthread_t thr_id = pthread_self();
    
    if (indc->slash_remain > 0)
        *indc->cur_slash = '/';
    
    info = get_ipc_info(indc->cur_inode);
    int check = check_connect(info, indc->cur_inode);
    if (check == -1)
        return -1;
    
    struct ap_ipc_port *s_port = info->port_pair->far_port;
    struct ap_ipc_port *c_port = info->port_pair->local_port;
    
    SHOW_TRASH_BAG;
    str_len = strlen(indc->the_name);
    struct ipc_inode_ide *iide;
    size_t buf_l = sizeof(*iide) + str_len + 2;
    iide = Malloc_z(buf_l + 1);
    path_cpy_add_root(iide->chrs, indc->the_name, str_len);
    iide->ide_p.ide_type.pid = pid;
    iide->ide_t.ide_type.thr_id = thr_id;
    iide->off_set_p = iide->off_set_t = 0;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t lis[] = {buf_l};
    size_t ttlen;
    buf = constr_req(iide, buf_l, lis, 1, &ttlen);
    buf->req.req_t.op_type = g;
    buf->req.req_t.f_o_info.pm_ide = info->pm_id;
    TRASH_BAG_RAW_PUSH(buf, free);
    
    struct package_hint hint;
    ssize_t sent_s = s_port->ipc_ops->
    ipc_send(s_port, buf, ttlen, &hint);
    if (sent_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(indc->cur_inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    ssize_t recv_s = s_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(indc->cur_inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    //initialize inode
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
    if (msgre->rep_t.re_type != 0 && !msgre->struct_l) {
        errno = msgre->rep_t.err;
        B_return((int)msgre->rep_t.re_type);
    }

    f_indc = (struct ap_inode_indicator*)msgre->re_struct;
    if(msgre->rep_t.re_type){
        errno = msgre->rep_t.err;
        B_return((int)msgre->rep_t.re_type);
    }
    
    f_ind = (struct ap_inode *)(f_indc + 1);
    cur_ind = MALLOC_AP_INODE();
    cur_ind->is_dir = f_ind->is_dir;
    cur_ind->parent = indc->cur_inode;
    
    const char *dir_name = strrchr(indc->the_name, '/');
    if (dir_name == NULL)
        dir_name = indc->the_name;
    else if (strcmp(indc->the_name, "/"))
        dir_name++;
    
    size_t len = strlen(dir_name);
    cur_ind->name = Malloc_z(len + 1);
    strncpy(cur_ind->name, dir_name, len);
    
    if (f_ind->is_dir){
        cur_ind->i_ops = &proc_sub_inode_operations;
    }
    else{
        cur_ind->f_ops = &proc_file_operations;
        cur_ind->i_ops = &proc_file_i_operations;
    }
    
    
    cur_ind->mount_inode = indc->cur_mtp;
    ap_inode_put(indc->cur_inode);
    ap_inode_get(cur_ind);
    indc->cur_inode = cur_ind;
    
    B_return((int)msgre->rep_t.re_type);
}

static int find_proc_inode(struct ap_inode_indicator *indc)
{
    struct hash_union *un;
    struct hash_identity ide;
    struct ap_ipc_info *info;
    char *path;
    size_t strl;
    int get = 0;
    
    if (indc->slash_remain > 0)
        *indc->cur_slash = '/';

    strl = strlen(indc->the_name) + 2;
    path = Malloc_z(strl);
    path_cpy_add_root(path, indc->the_name, strlen(indc->the_name));
    
    info = get_ipc_info(indc->cur_inode);
    ide.ide_c = path;
    ide.ide_type.ide_i = 0;
    un = hash_union_get(info->inde_hash_table, ide);
    if (un != NULL) {
        ap_inode_put(indc->cur_inode);
        indc->cur_inode = container_of(un, struct ap_inode, ipc_path_hash);
        ap_inode_get(indc->cur_inode);
        return 0;
    }
    
    get = get_proc_inode(indc);
    if (get == -1)
        return get;
    
    indc->cur_inode->ipc_path_hash.ide.ide_c = path;
    indc->cur_inode->ipc_path_hash.ide.ide_type.ide_i = 0;
    indc->cur_inode->links++;
    add_inode_to_mt(indc->cur_inode, indc->cur_mtp);
    hash_union_insert(info->inde_hash_table, &indc->cur_inode->ipc_path_hash);
    return get;
}

static ssize_t proc_read(struct ap_file *file, char *buf, off_t off, size_t size)
{
    struct ap_inode *inode = file->relate_i;
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port = info->port_pair->far_port;
    struct ap_ipc_port *c_port = info->port_pair->local_port;
    
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    SHOW_TRASH_BAG;
    struct ap_msgbuf *msgbuf;
    struct ipc_inode_ide *iide;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    ssize_t send_s,recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    pthread_t thr_id= pthread_self();
    
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->off_set_p = iide->off_set_t = 0;
    iide->fd = file->ipc_fd;
    iide->ide_p.ide_type.pid = pid;
    iide->ide_t.ide_type.thr_id = thr_id;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t list[] = {buf_l};
    size_t ttlen;
    
    msgbuf = constr_req(iide, buf_l, list, 1, &ttlen);
    msgbuf->req.req_t.op_type = r;
    msgbuf->req.req_t.f_o_info.read_len = size;
    msgbuf->req.req_t.f_o_info.off_size = off;
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1)
        B_return(-1);
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1)
        B_return(-1);
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type <= 0) {
        errno = msgre->rep_t.err;
        B_return(msgre->rep_t.re_type);
    }
    
    memcpy(buf, msgre->re_struct, msgre->rep_t.read_n);
    ssize_t o = msgre->rep_t.re_type;
    B_return(o);
}

static ssize_t proc_write(struct ap_file *file, char *buf, off_t off, size_t size)
{
    struct ap_inode *inode = file->relate_i;
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    void *msg_buf;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    SHOW_TRASH_BAG;
    struct ap_msgbuf *msgbuf;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    struct ipc_inode_ide *iide;
    ssize_t send_s, recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    pthread_t thr_id = pthread_self();
 
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->fd = file->ipc_fd;
    iide->off_set_p = iide->off_set_t = 0;
    iide->ide_t.ide_type.thr_id = thr_id;
    iide->ide_p.ide_type.pid = pid;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    void *items[] = {
        iide,
        buf,
    };
    
    size_t list[] = {buf_l, size};
    size_t ttlen;
    
    msg_buf = collect_items(items, buf_l + size, list, 2);
    msgbuf = constr_req(msg_buf, buf_l + size, list, 2, &ttlen);
    msgbuf->req.req_t.op_type = w;
    msgbuf->req.req_t.f_o_info.wirte_len = size;
    msgbuf->req.req_t.f_o_info.off_size = off;
    TRASH_BAG_RAW_PUSH(msg_buf, free);
    
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    
    if (msgre->rep_t.re_type <= 0) {
        errno = msgre->rep_t.err;
        B_return(msgre->rep_t.re_type);
    }
    ssize_t o = msgre->rep_t.re_type;
    B_return(o);
}

static int proc_open(struct ap_file *file, struct ap_inode *inode, unsigned long flag)
{
    struct ap_ipc_port *s_port, *c_port;
    struct ap_ipc_info *info = get_ipc_info(inode);
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
    
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    SHOW_TRASH_BAG;
    struct ap_msgbuf *msgbuf;
    struct ipc_inode_ide *iide;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    ssize_t send_s, recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    pthread_t thr_id = pthread_self();
    
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    size_t list[] = {buf_l};
    
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->off_set_p = iide->off_set_t = 0;
    iide->ide_p.ide_type.pid = pid;
    iide->ide_t.ide_type.thr_id = thr_id;
    
    size_t ttlen;
    
    msgbuf = constr_req(iide, buf_l, list, 1, &ttlen);
    msgbuf->req.req_t.op_type = o;
    msgbuf->req.req_t.f_o_info.flags = flag;
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(msgre, free);
    
    file->ipc_fd = msgre->ipc_fd;
    int o = (int)msgre->rep_t.re_type;
    B_return(o);
}

static int proc_unlink(struct ap_inode *inode)
{
    struct ap_ipc_info *info = inode->parent->x_object;
    pthread_mutex_lock(&info->inde_hash_table->r_size_lock);
    info->inde_hash_table->r_size--;
    pthread_mutex_unlock(&info->inde_hash_table->r_size_lock);
    hash_union_delet(&inode->ipc_path_hash);
    if (info->disc && info->inde_hash_table->r_size == 0) {
        free(info->inde_hash_table);
        AP_INODE_FREE(inode->parent);
    }
    return 0;
}

static int proc_release(struct ap_file *file, struct ap_inode *inode)
{
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
    
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    SHOW_TRASH_BAG;
    struct ap_msgbuf *msgbuf;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    struct ipc_inode_ide *iide;
    ssize_t send_s, recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    pthread_t thr_id = pthread_self();
    
    size_t str_len = strlen(path);
    size_t buf_l = str_len + sizeof(*iide) + 2;
    size_t list[] = {buf_l};
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->fd = file->ipc_fd;
    iide->off_set_p = iide->off_set_t = 0;
    iide->ide_p.ide_type.pid = pid;
    iide->ide_t.ide_type.thr_id = thr_id;
    TRASH_BAG_RAW_PUSH(iide, free);
    size_t ttlen;
    
    msgbuf = constr_req(iide, buf_l, list, 1, &ttlen);
    msgbuf->req.req_t.op_type = c;
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    
    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        B_return(-1);
    }
    
    int o = (int)msgre->rep_t.re_type;
    B_return(o);
}
void proc_dir_release(void *cusor)
{
    DIR *dp = cusor;
    closedir(dp);
}

static ssize_t procfs_readdir
(struct ap_inode *inode, AP_DIR *dir, void *buff, size_t num)
{
    DIR *dp;
    struct dirent *dirp;
    struct ap_dirent *dir_tp = buff;
    struct ap_ipc_info_head *h_info = inode->x_object;
    const char *sever_name = h_info->sever_name;
    size_t i = 0;
	size_t d_len;
    if ((dp = dir->cursor) == NULL) {
        dp = opendir(AP_PROC_FILE);
        if (dp == NULL)
            return -1;
        dir->cursor = dp;
        dir->release = proc_dir_release;
    }
    for (i = 0; i < num; i++) {
        dirp = readdir(dp);
		d_len = strlen(dirp->d_name);
        if (dirp == NULL){
            dir->done = 1;
            break;
        }
        if (strcmp(dirp->d_name, ".") == 0 ||
            strcmp(dirp->d_name, "..") == 0 ||
            strncmp(sever_name, dirp->d_name, d_len) == 0) {
            i--;
            continue;
        }
        strncpy(dir_tp->name, dirp->d_name, d_len);
        dir_tp->name_l = d_len;
        dir_tp++;
    }
    
    return (i * sizeof(*dir_tp));
}

static ssize_t procff_readdir
(struct ap_inode *inode, AP_DIR *dir, void *buff, size_t num)
{
    DIR *dp;
    struct dirent *dirp;
    struct ap_dirent *dir_tp = buff;
    size_t i = 0;
    char *sever_n = inode->name;
    char full_p[AP_IPC_PATH_LEN];
    char *at_p;
	size_t d_len;
    const char *names[] = {
        AP_PROC_FILE,
        sever_n,
    };
    if ((dp = dir->cursor) == NULL) {
        memset(full_p, '\0', AP_IPC_PATH_LEN);
        path_names_cat(full_p, names, 2, "/");
        dp = opendir(full_p);
        if (dp == NULL)
            return -1;
        dir->cursor = dp;
        dir->release = proc_dir_release;
    }
    
    while ((dirp = readdir(dp)) != NULL && i < num) {
        if ((at_p = strchr(dirp->d_name, AT)) == NULL) {
            continue;
        }
		d_len = strlen(dirp->d_name);
        strncpy(dir_tp->name, dirp->d_name, d_len);
        dir_tp->name_l = d_len;
        dir_tp++;
        i++;
    }
    return (i * sizeof(*dir_tp));
}

static ssize_t proc_readdir
(struct ap_inode *inode, AP_DIR *dir, void *buff, size_t num)
{
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
   
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    struct ap_msgbuf *msgbuf;
    struct ipc_inode_ide *iide;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    ssize_t send_s, recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    pthread_t thr_id = pthread_self();
    
    SHOW_TRASH_BAG;
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    size_t lis[] = {buf_l};
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->off_set_p = iide->off_set_t = 0;
    iide->ide_p.ide_type.pid = pid;
    iide->ide_t.ide_type.thr_id = thr_id;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t ttlen;
    msgbuf = constr_req(iide, buf_l, lis, 1, &ttlen);
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    msgbuf->req.req_t.op_type = rdir;
    msgbuf->req.req_t.f_o_info.read_len = num;
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
   
    if (msgre->rep_t.re_type == -1 || msgre->struct_l == 0) {
        errno = msgre->rep_t.err;
        B_return(-1);
    }
    if (msgre->rep_t.read_n > 0) {
        memcpy(buff, msgre->re_struct, msgre->rep_t.read_n);
    }
    
    dir->done = msgre->rep_t.readdir_is_done;
    B_return(msgre->rep_t.read_n);
}

static off_t proc_llseek(struct ap_file *file, off_t off_size, int orign)
{
    struct ap_inode *inode = file->relate_i;
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
    ssize_t send_s, recv_s;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    SHOW_TRASH_BAG;
    struct ap_msgbuf *buf;
    void *rep;
    size_t buf_l;
    struct ipc_inode_ide *iide = get_iide_path(path, &buf_l);
    iide->fd = file->ipc_fd;
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t ttlen;
    size_t lis[] = {buf_l};
    buf = constr_req(iide, buf_l, lis, 1, &ttlen);
    buf->req.req_t.f_o_info.off_size = off_size;
    buf->req.req_t.f_o_info.orign = orign;
    buf->req.req_t.op_type = seek;
    TRASH_BAG_RAW_PUSH(buf, free);
    
    struct package_hint hint;
    send_s = s_port->ipc_ops->
    ipc_send(s_port, buf, ttlen, &hint);
    if (send_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, &rep, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(rep, free);
    struct ap_msgreply *msg_rep = rep;
    if (msg_rep->rep_t.re_type == -1) {
        errno = msg_rep->rep_t.err;
        B_return(-1);
    }
    file->off_size = msg_rep->rep_t.re_type;
    B_return(file->off_size);
}

static int proc_closedir(struct ap_inode *inode)
{
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
    
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    SHOW_TRASH_BAG;
    ssize_t send_s, recive_s;
    struct ap_msgbuf *msgbuf;
    struct ipc_inode_ide *iide;
    char *msg_reply;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 2;
    size_t lis[] = {buf_l};
    iide = Malloc_z(buf_l);
    path_cpy_add_root(iide->chrs, path, str_len);
    iide->off_set_p = iide->off_set_t = 0;
    iide->ide_p.ide_type.pid = getpid();
    iide->ide_t.ide_type.thr_id = pthread_self();
    TRASH_BAG_RAW_PUSH(iide, free);
    
    size_t ttlen;
    msgbuf = constr_req(iide, buf_l, lis, 1, &ttlen);
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    msgbuf->req.req_t.op_type = cdir;
    
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    recive_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recive_s == -1) {
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        B_return(-1);
    }
    B_return(0);
}

static int proc_destory(struct ap_inode *inode)
{
    struct ap_ipc_info *info = get_ipc_info(inode);
    struct ap_ipc_port *s_port, *c_port;
    s_port = info->port_pair->far_port;
    c_port = info->port_pair->local_port;
   
    int check = check_connect(info, inode);
    if (check == -1)
        return -1;
    
    SHOW_TRASH_BAG;
    struct ipc_inode_ide *iide;
    struct ap_msgbuf *msgbuf;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    ssize_t send_s, recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    
    size_t str_len = strlen(path);
    size_t buf_l = sizeof(*iide) + str_len + 1;
    size_t list[] = {buf_l};
    
    iide = Malloc_z(buf_l);
    strncpy(iide->chrs, path, str_len);
    iide->ide_p.ide_type.pid = pid;
    iide->off_set_p = 0;
    TRASH_BAG_RAW_PUSH(iide, free);

    size_t ttlen;
    msgbuf = constr_req(iide, buf_l, list, 1, &ttlen);
    msgbuf->req.req_t.op_type = d;
    TRASH_BAG_RAW_PUSH(msgbuf, free);
    
    struct package_hint hint;
    
    send_s = s_port->ipc_ops->
    ipc_send(s_port, msgbuf, ttlen, &hint);
    if (send_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(&hint, BAG_PAKAGE_HINT_FREE);
    recv_s = c_port->ipc_ops->
    ipc_recv(c_port, (void **)&msg_reply, AP_IPC_ONE_MSG_UNION, &hint, NULL);
    if (recv_s == -1){
        if (errno == ENETDOWN)
            handle_disc(inode, s_port, info->info_h->sever_name);
        B_return(-1);
    }
    
    TRASH_BAG_RAW_PUSH(msg_reply, free);
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;

    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        B_return(-1);
    }
    
    int o = (int)msgre->rep_t.re_type;
    B_return(o);
}

static struct ap_inode_operations procfs_inode_operations = {
    .get_inode = procfs_get_inode,
    .readdir = procfs_readdir,
};

static struct ap_inode_operations procff_inode_operations = {
    .get_inode = procff_get_inode,
    .readdir = procff_readdir,
};

static struct ap_inode_operations proc_inode_operations = {
    .get_inode = get_proc_inode,
    .unlink = proc_unlink,
    .find_inode = find_proc_inode,
    .destory = proc_destory,
    .readdir = proc_readdir,
    .closedir = proc_closedir,
};

static struct ap_inode_operations proc_sub_inode_operations = {
    .unlink = proc_unlink,
    .destory = proc_destory,
    .readdir = proc_readdir,
    .closedir = proc_closedir,
};

static struct ap_inode_operations proc_file_i_operations = {
    .destory = proc_destory,
};

static struct ap_file_operations proc_file_operations = {
    .read = proc_read,
    .write = proc_write,
    .release = proc_release,
    .open = proc_open,
    .llseek = proc_llseek,
};

int init_proc_fs()
{
    struct ap_file_system_type *proc_fsys = MALLOC_FILE_SYS_TYPE();
    proc_fsys->is_allow_mount = 0;
    proc_fsys->name = PROC_FILE_FS;
    proc_fsys->get_initial_inode = proc_get_initial_inode;
    return register_fsyst(proc_fsys);
}




