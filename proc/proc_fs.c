//
//  proc_fs.c
//  ap_editor
//
//  Created by sunya on 15/4/21.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <ap_fs.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/msg.h>
#include <envelop.h>
#include <ap_file.h>
#include <ap_pthread.h>
#include <errno.h>
#include <ap_hash.h>
#include <stdlib.h>
#include <list.h>
#include <sys/stat.h>
#include <convert.h>
#include <dirent.h>
#include <time.h>
#include "proc_fs.h"
#define IPC_PATH 0
#define PROC_NAME 1
#define IPC_KEY 2
#define MSG_LEN (sizeof(struct ap_msgseg) - sizeof(long))
static int client_msid;
static key_t client_key;

key_t key[2];
int msgid[2];

struct ipc_holder_hash ipc_hold_table;

pthread_mutex_t channel_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long channel_n;
static int ap_msgsnd(int msgid, const void *buf, size_t len, unsigned long ch_n, int msgflg);
static char *path_name_cat(char *dest, const char *src, size_t len, char *d);
static char *path_names_cat(char *dest, const char **src, int num, char *d);
static struct ap_inode_operations procfs_inode_operations;
static struct ap_inode_operations procff_inode_operations;
static struct ap_inode_operations proc_inode_operations;
static struct ap_inode_operations proc_file_i_operations;
static struct ap_file_operations proc_file_operations;

static void ap_msg_send_err(errno_t err, int re_type, int msgid, long msgtyp)
{
    struct ap_msgreply re;
    re.rep_t.err = EINVAL;
    re.struct_l = 0;
    re.rep_t.re_type = -1;
    ap_msgsnd(msgid, &re, sizeof(re), msgtyp,0);
}

static char **pull_req(struct ap_msgreq *req)
{
    size_t data_len;
    char *p;
    char *cp;
    char **msg_req = Mallocx(sizeof(char *) *req->index_lenth);
    size_t *list = (size_t *)req->req_detail;
    char *detail = (char *)(list + req->index_lenth);
    p = detail;
    for (int i = 0; i<req->index_lenth; i++) {
        data_len = list[i];
        cp = Mallocz(data_len + 1);
        memcpy(cp, p, data_len);
        msg_req[i] = cp;
        p += data_len;
    }
    return msg_req;
}

static char *path_name_cat(char *dest, const char *src, size_t len, char *d)
{
    strncat(dest, d, 1);
    strncat(dest, src, len);
    return dest;
}

static char *path_names_cat(char *dest, const char **src, int num, char *d)
{
    size_t strl = 0;
    size_t strl1 = 0;
    for (int i = 0; i < num; i++) {
        strl1 = strlen(src[i]);
        strl += strl1;
        path_name_cat(dest, src[i], strl, d);
    }
    return dest + strl + (num-1);
}


static struct ap_msgbuf *constr_req(const char *buf, size_t buf_len, size_t list[], int lis_len)
{
    struct ap_msgbuf *msgbuf =
    (struct ap_msgbuf *)Mallocz(sizeof(struct ap_msgbuf) + buf_len + sizeof(size_t)*lis_len);
    msgbuf->req.index_lenth = lis_len;
    size_t *si = (size_t *)msgbuf->req.req_detail;
    char *cp = (char *)(si + lis_len);
    for (int i = 0; i < lis_len; i++) {
        si[i] = list[i];
    }
    memcpy(cp, buf, buf_len);
    return msgbuf;
}

static char *collect_items(const char **items, size_t buf_len, size_t list[], int lis_len)
{
    char *buf = Mallocz(buf_len);
    char *cp = buf;
    for (int i = 0; i<lis_len; i++) {
        memcpy(cp, items[i], list[i]);
        cp += list[i];
    }
    return buf;
}

static inline unsigned long get_channel()
{
    unsigned long ch_n;
    pthread_mutex_lock(&channel_lock);
    ch_n = channel_n;
    channel_n++;
    pthread_mutex_unlock(&channel_lock);
    return ch_n;
}

static void recode_proc_dis(struct ipc_sock *s)
{
    time_t time_n;
    int fd;
    const char *sever_name = s->sever_name;
    char *p_p;
    
    fd = open(AP_PROC_DISC_F, O_CREAT | O_TRUNC | O_RDWR, 0777);
    if (fd == -1) {
        return;
    }
    
    const char *path[] ={
        AP_PROC_FILE,
        sever_name,
    };
    char full_path[AP_IPC_PATH_LEN];
    memset(full_path, '\0', AP_IPC_PATH_LEN);
    p_p = path_names_cat(full_path, path, 2, "/");
    time_n = time(NULL);
    snprintf(p_p, AP_IPC_PATH_LEN, ":%d:%d:%ld\n",s->pid, s->msgid, time_n);
    Write_lock(fd, 0, SEEK_SET, 0);
    lseek(fd, 0, SEEK_END);
    write(fd, full_path, AP_IPC_PATH_LEN);
    Un_lock(fd, 0, SEEK_SET, 0);
    return;
    
}

static void handle_disc(struct ap_inode *inde, struct ipc_sock *s)
{
    recode_proc_dis(s);
    if (inde == NULL) {
        return;
    }
    struct ap_inode *parent = inde->parent;
    struct ap_ipc_info *info = parent->x_object;
    if (parent->parent == NULL) {
        return;
    }
    inode_del_child(parent->parent, parent);
    parent->parent = NULL;
    info->disc = 1;
}

static int ap_ipc_kick_start(char *sever_name)
{
    channel_n = random();
    pid_t pid = getpid();
    FILE *fs;
   
    char path[AP_IPC_PATH_LEN];
    memset(path, '\0', AP_IPC_PATH_LEN);
    char msgid_key[AP_IPC_RECODE_LEN];
    memset(path, '\0', AP_IPC_RECODE_LEN);

    char *p_p = path;
    size_t strl0 = sizeof(AP_PROC_FILE);
    size_t strl1 = sizeof(sever_name);
    size_t w_n;
    
    memset(p_p, '\0', AP_IPC_PATH_LEN);
    strncpy(p_p, AP_PROC_FILE, strl0);
    path_name_cat(p_p, sever_name, strl1,"/");
    p_p += strl1 + strl0 + 1;
    *p_p = '/';
    p_p++;
    
    snprintf(p_p, AP_IPC_PATH_LEN, "%s_%ld",sever_name,(long)pid);
    fs = fopen(path, "r+b");
    if (fs == NULL) {
        return -1;
    }
    //pid:msgid:key
    snprintf(msgid_key, AP_IPC_RECODE_LEN, "%ld:%d:%d\n", (long)pid, msgid[0], key[0]);
    w_n = fputs(msgid_key, fs);
    if (w_n < strlen(msgid_key)) {
        unlink(path);
        return -1;
    }
    fclose(fs);
    return 0;
}

static key_t ap_ftok(pid_t pid, char *ipc_path)
{
    int cr_s;
    key_t key;
    
    open(ipc_path, O_CREAT | O_TRUNC | O_RDWR);
    if (cr_s == -1) {
        return -1;
    }
    chmod(ipc_path, 0777);
    close(cr_s);
    key = ftok(ipc_path, (int)pid);
    return key;
}

static void msgmemcpy(size_t seq, char *base, const char *data, size_t len)
{
    char *cp = base + (seq*AP_MSGSEG_LEN);
    memcpy(cp, data, len);
}

static ssize_t ap_msgrcv(int msgid, char **d_buf, unsigned long wait_seq)
{
    struct ap_msgseg buf;
    char *cc_p = NULL;
    char *c_p = NULL;
    ssize_t rv = 0;
    ssize_t recv_s;
    int dl = -1;
    int data_l = 0;
    
    do {
        recv_s = msgrcv(msgid, (void *)&buf, MSG_LEN, wait_seq, 0);
        if (recv_s == -1) {
            perror("rcv failed");
            return -1;
        }
        if (dl == -1) {
            rv = dl = (int)buf.len_t;
            if (dl == 0) {
                goto FINISH;
            }
            *d_buf = Mallocx(dl);
            cc_p = c_p = *d_buf;
        }
        data_l = buf.data_len;
        msgmemcpy(buf.seq, cc_p, buf.segc, data_l);
        dl -= data_l;
    } while (dl>0);
    
FINISH:
    return rv;
}

static int ap_msgsnd(int msgid, const void *buf, size_t len, unsigned long ch_n, int msgflg)
{
    const char *c_p = buf;
    size_t dl = len;
    int msgsnd_s;
    int seq = 0;
    struct ap_msgseg seg;
    seg.mtype = ch_n;
    seg.len_t = len;
    while (dl > 0) {
        seg.seq = seq;
        memcpy(seg.segc, c_p, AP_MSGSEG_LEN);
        seg.data_len = dl< AP_MSGSEG_LEN ? (int)dl : AP_MSGSEG_LEN;
        msgsnd_s = msgsnd(msgid, &seg, sizeof(struct ap_msgseg), msgflg);
        if (msgsnd_s == -1) {
            errno = EBADF;
            return -1;
        }
        
        dl -= AP_MSGSEG_LEN;
        if (dl>0) {
            c_p += AP_MSGSEG_LEN;
        }
        seq++;
    }
    return 0;
}

static void client_g(struct ap_msgbuf *buf, char **req_d)
{
    struct ap_inode_indicator *indic;
    char *path = req_d[0];
    size_t str_len = strlen(req_d[0]);
    struct ap_msgreply re;
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct ap_inode_indicator *strc_cp;
    size_t len;
    
    indic = MALLOC_INODE_INDICATOR();
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    int set = initial_indicator(path, indic, ap_fpthr);
    if (set == -1) {
        re.rep_t.err = EINVAL;
        re.rep_t.re_type = -1;
        re.struct_l = 0;
        ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n,0);
        return;
    }
    
    len = sizeof(struct ap_msgreply) + sizeof(*indic) + sizeof(struct ap_inode);
    
    struct ap_msgreply *re_m = Mallocx(len);
    int get = walk_path(indic);
    
    re_m->rep_t.re_type = get;
    re_m->struct_l = 1;
    re_m->rep_t.err = errno;
    strc_cp = (struct ap_inode_indicator*)re_m->re_struct;
    *strc_cp = *indic;
    
    if (!get) {
        hl = MALLOC_HOLDER();
        ihl = MALLOC_IPC_INODE_HOLDER();
        char *ide_c = Mallocx(str_len);
        memcpy(ide_c, path, str_len);
        hl->ide.ide_c = ide_c;
        hl->ide.ide_i = buf->pid;
        hl->ipc_get = inode_ipc_get;
        hl->ipc_put = inode_ipc_put;
        ihl->inde = indic->cur_inode;
        hl->x_object = ihl;
        ipc_holder_hash_insert(hl);
        *((struct ap_inode *)(strc_cp + 1)) = *indic->cur_inode;
    }
    
    ap_msgsnd(buf->msgid, re_m, len, buf->ch_n,0);
    AP_INODE_INICATOR_FREE(indic);
    free(re_m);
    return;
}

static void client_r(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    char *f_idec = req_d[1];
    struct holder *hl;
    struct hash_identity f_ide;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply *re;
    struct hash_union *un;
    ssize_t read_n;
    size_t len;
    char *read_buf = Mallocz(buf->req.req_t.read_len);
    
    ide.ide_c = path;
    ide.ide_i = buf->pid;
    hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    
    f_ide.ide_c = f_idec;
    f_ide.ide_i = hl->ide.ide_i;
    
    un = hash_union_get(ihl->ipc_file_hash, f_ide);
    if (un == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    inde = ihl->inde;
    file = container_of(un, struct ap_file, f_hash_union);
    read_n = inde->f_ops->read(file, read_buf, buf->req.req_t.off_size, buf->req.req_t.read_len);
    if (read_n <= 0) {
        ap_msg_send_err(errno, (int)read_n, buf->msgid, buf->ch_n);
        return;
    }
    
    len = sizeof(struct ap_msgreply) + read_n;
    
    re = Mallocz(len);
    re->rep_t.re_type = read_n;
    re->rep_t.read_n = read_n;
    re->struct_l = 1;
    memcpy(re->re_struct, read_buf, read_n);
    ap_msgsnd(buf->msgid, re, len, buf->ch_n, 0);
    free(re);
    return;
}

static void client_w(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    char *f_idec = req_d[1];
    struct holder *hl;
    struct hash_identity f_ide;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply re;
    struct hash_union *un;
    ssize_t write_n;
    
    ide.ide_c = path;
    ide.ide_i = buf->pid;
    hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    
    f_ide.ide_c = f_idec;
    f_ide.ide_i = hl->ide.ide_i;
    
    un = hash_union_get(ihl->ipc_file_hash, f_ide);
    if (un == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    inde = ihl->inde;
    file = container_of(un, struct ap_file, f_hash_union);
    write_n = inde->f_ops->read(file, req_d[1], buf->req.req_t.off_size, buf->req.req_t.wirte_len);
    if (write_n < 0) {
        ap_msg_send_err(errno, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    re.rep_t.re_type = write_n;
    re.rep_t.write_n = write_n;
    re.struct_l = 0;
    ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n,0);
    return;
}

static void client_o(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    char *f_idec = req_d[1];
    size_t str_len = strlen(req_d[1]);
    struct ap_msgreply re;
    struct holder *hl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ipc_inode_holder *ihl;
    
    ide.ide_c = path;
    ide.ide_i = buf->pid;
    hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    
    file = AP_FILE_MALLOC();
    AP_FILE_INIT(file);
    
    if (inde->f_ops->open != NULL) {
       int open_s = inde->f_ops->open(file, inde, buf->req.req_t.flags);
        if (open_s == -1) {
            ap_msg_send_err(errno, -1, buf->msgid, buf->ch_n);
            AP_FILE_FREE(file);
            return;
        }
    }
    
    file->f_ops = inde->f_ops;
    file->relate_i = inde;
    ap_inode_get(inde);
    
    if (ihl->ipc_file_hash == NULL) {
        ihl->ipc_file_hash = MALLOC_IPC_HASH(AP_IPC_FILE_HASH_LEN);
    }
    
    char *cp_idec = Mallocz(str_len + 1);
    memcpy(cp_idec, f_idec, str_len);
    
    file->f_hash_union.ide.ide_i = hl->ide.ide_i;
    file->f_hash_union.ide.ide_c = cp_idec;
    hash_union_insert(ihl->ipc_file_hash, &file->f_hash_union);
    
    re.rep_t.re_type = 0;
    re.struct_l = 0;
    ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n, 0);
}

static void client_c(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    char *f_idec = req_d[1];
    struct holder *hl;
    struct hash_identity f_ide;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct hash_union *un;
    
    ide.ide_c = path;
    ide.ide_i = buf->pid;
    hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    
    f_ide.ide_c = f_idec;
    f_ide.ide_i = hl->ide.ide_i;
    
    un = hash_union_get(ihl->ipc_file_hash, f_ide);
    if (un == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    file = container_of(un, struct ap_file, f_hash_union);
    if (file->f_ops->release != NULL) {
       int rs = file->f_ops->release(file, inde);
        if (rs == -1) {
            ap_msg_send_err(errno, -1, buf->msgid, buf->ch_n);
            return;
        }
    }
    AP_FILE_FREE(file);
}

static void client_d(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct holder_table_union *un;
    
    ide.ide_c = path;
    ide.ide_i = buf->pid;
    hl = ipc_holder_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    pthread_mutex_lock(&un->table_lock);
    list_del(&hl->hash_lis);
    pthread_mutex_unlock(&un->table_lock);
    
    ihl = hl->x_object;
    
    IHOLDER_FREE(ihl);
    HOLDER_FREE(hl);
}

void *ap_proc_sever(void *arg)
{
    ssize_t recv_s;
    int *msgid = arg;
    char *buf;
    char **req_detail;
    struct ap_msgbuf *msg_buf;
    op_type_t type;
    ap_file_thread_init();
    while (1) {
        recv_s = ap_msgrcv(*msgid, &buf, 0);
        msg_buf = (struct ap_msgbuf *)buf;
        type = msg_buf->req.req_t.op_type;
        req_detail = pull_req(&msg_buf->req);
        msg_buf->msgid = msgget(msg_buf->key, 0);
        if (msg_buf->msgid == -1) {
            continue;
        }
        switch (type) {
            case g:
                client_g(msg_buf, req_detail);
                break;
            case w:
                client_w(msg_buf, req_detail);
                break;
            case r:
                client_r(msg_buf, req_detail);
                break;
            case o:
                client_o(msg_buf, req_detail);
                break;
            case c:
                client_c(msg_buf, req_detail);
                break;
            case d:
                client_d(msg_buf, req_detail);
                break;
            default:
                printf("ap_msg wrong type\n");
                exit(1);
        }
    }
}

static int proc_creat_file(const char *sever_name)
{
    pid_t  pid = getpid();
    size_t strl0 = strlen(AP_PROC_FILE);
    size_t strl1 = strlen(sever_name);
    char ipc_path[AP_IPC_PATH_LEN];
    char *path_p = ipc_path;
    memset(ipc_path, '\0', AP_IPC_PATH_LEN);
    
    strncpy(path_p, AP_PROC_FILE, strl0);
    strncat(path_p, "/", 1);
    strncat(path_p, sever_name, strl1);
    path_p += (strl0 + strl1 + 1);
    *path_p = '/';
    path_p++;
    
    errno = 0;
    int mkd = mkdir(path_p, 0755);
    if (mkd == -1 && errno != EEXIST) {
        return -1;
    }
    
    for (int i = 0; i <= 2; i++) {
        snprintf(path_p, AP_IPC_PATH_LEN, "%ld_%d",(long)pid, i);
        key[i] = ap_ftok(pid,ipc_path);
        if (key[i] == -1) {
            errno = EBADF;
            return -1;
        }
        msgid[i] = msgget(key[i], IPC_CREAT | IPC_W | IPC_R);
        if (msgid[i] == -1) {
            errno = EBADF;
            return -1;
        }
    }
    return 0;
}

static struct ap_inode *proc_get_initial_inode(struct ap_file_system_type *fsyst, void *x_object)
{
    pthread_t thr_n;
    struct ap_inode *init_inode;
    
    proc_creat_file(x_object);
    client_msid = msgid[1];
    client_key = key[1];
    int thr_cr_s = pthread_create(&thr_n, NULL, ap_proc_sever, &msgid[0]);
    if (thr_cr_s != 0) {
        msgid[0] = msgid[1] = 0;
        key[0] = key[1] = 0;
        return NULL;
    }
    ap_ipc_kick_start(x_object);
    //initialize inode
    init_inode = MALLOC_AP_INODE();
    init_inode->is_dir = 1;
    init_inode->i_ops = &procfs_inode_operations;
    return init_inode;
}

static char **cut_str(const char *s, int d, size_t len)
{
    const char *s_p,*head;
    size_t p_l = strlen(s);
    char *tm_path = Mallocz(p_l+1);
    strncpy(tm_path, s, p_l);
    head = tm_path;
    char **cut = Mallocz(sizeof(char*)*len);
    int i = 0;
    
    while ((s_p = strchr(head, d) )!= NULL) {
        s_p = strchr(head, d);
        size_t str_len;
        s_p = '\0';
        str_len = strlen(head);
        char *item = Mallocz(str_len+1);
        strncpy(item, head, str_len);
        *(item + str_len) = '\0';
        cut[i] = item;
        head = s_p++;
        i++;
    }
    free(tm_path);
    return cut;
}

static int procfs_get_inode(struct ap_inode_indicator *indc)
{
    DIR *dp;
    struct dirent *dirp;
    char *path;
    const char *sever_name = indc->the_name;
    size_t strl = strlen(sever_name);
    struct ap_inode *inode;
    
    char ap_ipc_path[AP_IPC_PATH_LEN];
    memset(ap_ipc_path, '\0', AP_IPC_PATH_LEN);
    char msgid_key[AP_IPC_RECODE_LEN];
    memset(msgid_key, '\0', AP_IPC_RECODE_LEN);
    
    strncpy(ap_ipc_path, AP_PROC_FILE, sizeof(AP_PROC_FILE));
    dp = opendir(ap_ipc_path);
    if (dp == NULL) {
        return -1;
    }
    
    while ((dirp = readdir(dp)) != NULL) {
        path = dirp->d_name;
        if (strncmp(path, ".", 1) == 0 ||
            strncmp(path, "..", 2) == 0) {
            continue;
        }
        if (strncmp(path, sever_name, strl) == 0) {
            inode = MALLOC_AP_INODE();
            inode->name = Mallocz(strl +1);
            inode->is_dir = 1;
            inode->parent = indc->cur_inode;
            inode->mount_inode = indc->cur_inode->mount_inode;
            strncpy(inode->name, sever_name, strl);
            inode->i_ops = &procff_inode_operations;
            ap_inode_put(indc->cur_inode);
            ap_inode_get(inode);
            indc->cur_inode = inode;
            return 0;
        }
    }
    closedir(dp);
    return -1;
}

static int procff_get_inode(struct ap_inode_indicator *indc)
{
    FILE *fs;
    char **cut;
    struct ap_ipc_info *info;
    struct ap_inode *inode;
    const char *sever_name = indc->cur_inode->name;
    size_t strl0 = strlen(indc->cur_inode->name);
    size_t strl1 = strlen(indc->the_name);
    char full_p[AP_IPC_PATH_LEN];
    char sever_file[AP_IPC_RECODE_LEN];
    memset(full_p, '\0', AP_IPC_PATH_LEN);
    memset(sever_file, '\0', AP_IPC_RECODE_LEN);
    strncpy(sever_file, indc->the_name, strl0);
    
    const char *paths[] = {
        AP_PROC_FILE,
        sever_name,
        sever_file,
    };
    path_names_cat(full_p, paths, 3,"/");
    fs = fopen(full_p, "r+b");
    if (fs == NULL) {
        return -1;
    }
    memset(sever_file, '\0', AP_IPC_RECODE_LEN);
    fgets(sever_file, AP_IPC_RECODE_LEN, fs);
    cut = cut_str(sever_file, ':', AP_IPC_RECODE_NUM);
    
    if (msgget(atoi(cut[AP_IPC_KEY]), 0) == -1) {
        pid_t pid = atoi(cut[AP_IPC_PID]);
        int msgid = atoi(cut[AP_IPC_MSGID]);
        struct ipc_sock s = {
            .sever_name = (char *)sever_name,
            .pid = pid,
            .msgid = msgid,
        };
        handle_disc(NULL, &s);
        return -1;
    }
    
    info = MALLOC_IPC_INFO();
    inode = MALLOC_AP_INODE();
    info->sock.key = atoi(cut[AP_IPC_KEY]);
    info->sock.msgid = atoi(cut[AP_IPC_MSGID]);
    info->sock.pid = atoi(cut[AP_IPC_PID]);
    info->sock.sever_name = Mallocz(strl0 + 1);
    strncpy(info->sock.sever_name, sever_name, strl0);
    
    inode->name = Mallocz(strl1 + 1);
    strncpy(inode->name, indc->the_name, strl1);
    inode->parent = indc->cur_inode;
    inode->mount_inode = indc->cur_inode->mount_inode;
    inode->i_ops = &proc_inode_operations;
    inode->x_object = info;
    ap_inode_put(indc->cur_inode);
    ap_inode_get(inode);
    indc->cur_inode = inode;
    return 0;
}

static int get_proc_inode(struct ap_inode_indicator *indc)
{
    size_t str_len;
    struct ap_msgbuf *buf;
    int msgid;
    char *msg_reply;
    struct ap_inode *cur_ind, *f_ind;
    struct ap_inode_indicator *f_indc = NULL;
    struct ipc_sock *sock;
    struct ap_ipc_info *info;
    
    pid_t pid = getpid();
    *indc->cur_slash = '/';
    info = (struct ap_ipc_info *)indc->cur_inode->x_object;
    
    str_len = strlen(indc->the_name);
    size_t si[] = {str_len};
    buf = constr_req(indc->the_name, str_len, si, 1);
    buf->req.req_t.op_type = g;
    buf->pid = pid;
    buf->ch_n = get_channel();
    buf->key = client_key;
    msgid = info->sock.msgid;

    int sent_s = ap_msgsnd(msgid, buf, sizeof(struct ap_msgbuf) + str_len, 0, 0);
    if (sent_s == -1) {
        errno = ENOENT;
        return -1;
    }
    ssize_t recv_s = ap_msgrcv(client_msid, &msg_reply, buf->ch_n);
    if (recv_s == -1) {
        errno = ENOENT;
        return -1;
    }
    //initialize inode
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
    if (msgre->rep_t.re_type != 0 && !msgre->struct_l) {
        errno = msgre->rep_t.err;
        return (int)msgre->rep_t.re_type;
    }

    f_indc = (struct ap_inode_indicator*)msgre->re_struct;
    if(msgre->rep_t.re_type){
        errno = msgre->rep_t.err;
        return (int)msgre->rep_t.re_type;
    }
    
    f_ind = (struct ap_inode *)(f_indc + 1);
    cur_ind = MALLOC_AP_INODE();
    cur_ind->is_dir = f_ind->is_dir;
    cur_ind->parent = indc->cur_inode;
    
    sock = Mallocx(sizeof(*sock));
    *sock = info->sock;
    cur_ind->x_object = sock;
    
    if (f_ind->is_dir == 1) {
        cur_ind->i_ops = &proc_file_i_operations;
    }else{
        cur_ind->f_ops = &proc_file_operations;
    }
    ap_inode_put(indc->cur_inode);
    ap_inode_get(cur_ind);
    indc->cur_inode = cur_ind;
    
    free(msg_reply);
    free(buf);
    return (int)msgre->rep_t.re_type;
}

static int find_proc_inode(struct ap_inode_indicator *indc)
{
    struct hash_union *un;
    struct hash_identity ide;
    struct ap_ipc_info *info;
    char *path;
    size_t strl;
    int get = 0;
    
    *indc->cur_slash = '/';
    info = (struct ap_ipc_info *)indc->cur_inode->x_object;
    ide.ide_c = indc->the_name;
    ide.ide_i = 0;
    un = hash_union_get(info->inde_hash_table, ide);
    if (un != NULL) {
        indc->cur_inode = container_of(un, struct ap_inode, ipc_path_hash);
        return 0;
    }
    
    get = get_proc_inode(indc);
    if (get) {
        return get;
    }
    
    strl = strlen(ide.ide_c);
    path = Mallocx(strl);
    memcpy(path, ide.ide_c, strl);
    *(path + strl) = '\0';
    
    indc->cur_inode->ipc_path_hash.ide.ide_c = path;
    indc->cur_inode->ipc_path_hash.ide.ide_i = 0;
    indc->cur_inode->links++;
    hash_union_insert(info->inde_hash_table, &indc->cur_inode->ipc_path_hash);
    return get;
}

static ssize_t proc_read(struct ap_file *file, char *buf, off_t off, size_t size)
{
    struct ap_inode *inde = file->relate_i;
    struct ipc_sock *sock = (struct ipc_sock *)inde->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    const char *path = inde->ipc_path_hash.ide.ide_c;
    char *f_idec = file->f_idec;
    int send_s;
    ssize_t recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    msgid = sock->msgid;
    if (msgget(sock->key, 0) == -1) {
        errno = EBADF;
        handle_disc(inde, sock);
        return -1;
    }
    
    size_t str_len = strlen(path);
    size_t str_len_f = strlen(f_idec);
    size_t t_str_len = str_len + str_len_f;
    size_t list[] = {str_len,str_len_f};
    const char *items[] = {path,f_idec};
    char *send_buf = collect_items(items, t_str_len, list, 2);
    
    msgbuf = constr_req(send_buf, t_str_len, list, 2);
    msgbuf->req.req_t.op_type = r;
    msgbuf->req.req_t.read_len = size;
    msgbuf->req.req_t.off_size = off;
    msgbuf->pid = pid;
    msgbuf->ch_n = get_channel();
    msgbuf->key = client_key;
    
    send_s = ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + t_str_len, msgbuf->ch_n, 0);
    if (send_s == -1) {
        return -1;
    }
    
    recv_s = ap_msgrcv(client_msid, &msg_reply, msgbuf->ch_n);
    if (recv_s == -1) {
        return -1;
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type <= 0) {
        errno = msgre->rep_t.err;
        return msgre->rep_t.re_type;
    }
    
    memcpy(buf, msgre->re_struct, msgre->rep_t.read_n);
    ssize_t o = msgre->rep_t.re_type;
    free(send_buf);
    free(msgbuf);
    free(msgre);
    return o;
}

static ssize_t proc_write(struct ap_file *file, char *buf, off_t off, size_t size)
{
    struct ap_inode *inde = file->relate_i;
    struct ipc_sock *sock = (struct ipc_sock *)inde->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    const char *path = inde->ipc_path_hash.ide.ide_c;
    char *f_idec = file->f_idec;
    int send_s;
    ssize_t recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    msgid = sock->msgid;
    if (msgget(sock->key, 0) == -1) {
        errno = EBADF;
        handle_disc(inde,sock);
        return -1;
    }
 
    size_t str_len = strlen(path);
    size_t str_len_f = strlen(f_idec);
    size_t t_str_len = str_len + str_len_f;
    size_t list[] = {str_len,str_len_f};
    const char *items[] = {path,f_idec};
    char *send_buf = collect_items(items, t_str_len, list, 2);
    
    msgbuf = constr_req(send_buf, t_str_len, list, 2);
    msgbuf->req.req_t.op_type = w;
    msgbuf->req.req_t.wirte_len = size;
    msgbuf->req.req_t.off_size = off;
    msgbuf->pid = pid;
    msgbuf->ch_n = get_channel();
    msgbuf->key = client_key;
    
    send_s = ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + t_str_len, msgbuf->ch_n, 0);
    if (send_s == -1) {
        return -1;
    }
    
    recv_s = ap_msgrcv(client_msid, &msg_reply, msgbuf->ch_n);
    if (recv_s == -1) {
        return -1;
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type <= 0) {
        errno = msgre->rep_t.err;
        return msgre->rep_t.re_type;
    }
    ssize_t o = msgre->rep_t.re_type;
    free(msgbuf);
    free(msgre);
    return o;
}

static int proc_open(struct ap_file *file, struct ap_inode *inde, unsigned long flag)
{
    struct ipc_sock *sock = (struct ipc_sock *)inde->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    const char *path = inde->ipc_path_hash.ide.ide_c;
    int send_s;
    ssize_t recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    msgid = sock->msgid;
    if (msgget(sock->key, 0) == -1) {
        errno = EBADF;
        handle_disc(inde,sock);
        return -1;
    }
    
    char *rand_c = ultoa(rand() % _OPEN_MAX, NULL, 10);
    size_t str_len = strlen(path);
    size_t str_len_r = strlen(rand_c);
    char *f_idec = Mallocz(str_len + str_len_r);
    strncat(f_idec, path, str_len);
    strncat(f_idec, rand_c, str_len_r);
    size_t list[] = {str_len,str_len + str_len_r};
    const  char *items[] = {path, f_idec};
    char *buf = collect_items(items, 2*str_len + str_len_r, list, 2);
    
    msgbuf = constr_req(buf, 2*str_len + str_len_r, list, 2);
    msgbuf->req.req_t.op_type = o;
    msgbuf->pid = pid;
    msgbuf->ch_n = get_channel();
    msgbuf->key = client_key;
    
    send_s = ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + 2*str_len + str_len_r, msgbuf->ch_n, 0);
    if (send_s == -1) {
        return -1;
    }
    
    recv_s = ap_msgrcv(client_msid, &msg_reply, msgbuf->ch_n);
    if (recv_s == -1) {
        return -1;
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        return -1;
    }
    file->f_idec = f_idec;
    int o = (int)msgre->rep_t.re_type;

    free(rand_c);
    free(buf);
    free(msgbuf);
    free(msg_reply);
    return o;
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
    struct ipc_sock *sock = (struct ipc_sock *)inode->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    char *f_idec = file->f_idec;
    int send_s;
    ssize_t recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    msgid = sock->msgid;
    if (msgget(sock->key, 0) == -1) {
        errno = EBADF;
        handle_disc(inode,sock);
        return -1;
    }
    
    size_t str_len = strlen(path);
    size_t str_len_f = strlen(f_idec);
    size_t t_str_len = str_len_f + str_len;
    size_t list[] = {str_len,str_len_f};
    const char *items[] = {path,f_idec};
    char *send_buf = collect_items(items, t_str_len, list, 2);
    
    msgbuf = constr_req(send_buf, t_str_len, list, 2);
    msgbuf->req.req_t.op_type = c;
    msgbuf->pid = pid;
    msgbuf->ch_n = get_channel();
    msgbuf->key = client_key;
    
    send_s = ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + t_str_len, msgbuf->ch_n, 0);
    if (send_s == -1) {
        return -1;
    }
    
    recv_s = ap_msgrcv(client_msid, &msg_reply, msgbuf->ch_n);
    if (recv_s == -1) {
        return -1;
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;
    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        return -1;
    }
    
    int o = (int)msgre->rep_t.re_type;
    free(msgbuf);
    free(msgre);
    return o;
}

static int proc_destory(struct ap_inode *inode)
{
    struct ipc_sock *sock = (struct ipc_sock *)inode->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    const char *path = inode->ipc_path_hash.ide.ide_c;
    int send_s;
    ssize_t recv_s;
    char *msg_reply;
    pid_t pid = getpid();
    msgid = sock->msgid;
    
    size_t str_len = strlen(path);
    size_t list[] = {str_len};
    const char *items[] = {path};
    char *send_buf = collect_items(items, str_len, list, 1);
    
    msgbuf = constr_req(send_buf, str_len, list, 1);
    msgbuf->req.req_t.op_type = d;
    msgbuf->key = client_key;
    msgbuf->ch_n = get_channel();
    msgbuf->pid = pid;
    
    send_s = ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + str_len, msgbuf->ch_n, 0);
    if (send_s == -1) {
        return -1;
    }
    
    recv_s = ap_msgrcv(client_msid, &msg_reply, msgbuf->ch_n);
    if (recv_s == -1) {
        return -1;
    }
    
    struct ap_msgreply *msgre = (struct ap_msgreply*)msg_reply;

    if (msgre->rep_t.re_type == -1) {
        errno = msgre->rep_t.err;
        return -1;
    }
    
    int o = (int)msgre->rep_t.re_type;
    free(msgbuf);
    free(msgre);
    free(sock);
    return o;
}

static struct ap_inode_operations procfs_inode_operations = {
    .get_inode = procfs_get_inode,
};

static struct ap_inode_operations procff_inode_operations = {
    .get_inode = procff_get_inode,
};

static struct ap_inode_operations proc_inode_operations = {
    .get_inode = get_proc_inode,
    .unlink = proc_unlink,
    .find_inode = find_proc_inode,
    .destory = proc_destory,
};

static struct ap_inode_operations proc_file_i_operations = {
    .destory = proc_destory,
};

static struct ap_file_operations proc_file_operations = {
    .read = proc_read,
    .write = proc_write,
    .release = proc_release,
    .open = proc_open,
};

int init_proc_fs()
{
    struct ap_file_system_type *proc_fsys = MALLOC_FILE_SYS_TYPE();
    proc_fsys->name = PROC_FILE_FS;
    
    proc_fsys->get_initial_inode = proc_get_initial_inode;
    return register_fsyst(proc_fsys);
}




