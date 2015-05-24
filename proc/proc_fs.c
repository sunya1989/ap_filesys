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
#include <convert.h>
#include "proc_fs.h"

#define IPC_PATH 0
#define PROC_NAME 1
#define IPC_KEY 2
#define MSG_LEN (sizeof(struct ap_msgseg) - sizeof(long))
static int client_msid;
static key_t client_key;

struct ipc_holder_hash ipc_lock_table;

pthread_mutex_t channel_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long channel_n;
static int ap_msgsnd(int msgid, const void *buf, size_t len, unsigned long ch_n, int msgflg);

static void ap_msg_send_err(errno_t err, int re_type, int msgid, long msgtyp)
{
    struct ap_msgreply re;
    re.err = EINVAL;
    re.struct_l = 0;
    re.re_type = -1;
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

static struct ap_msgbuf *constr_req(char *buf, size_t len, size_t list[], int lis_len)
{
    struct ap_msgbuf *msgbuf = (struct ap_msgbuf *)Mallocz(sizeof(*buf) + len + sizeof(size_t)*lis_len);
    msgbuf->req.index_lenth = lis_len;
    size_t *si = (size_t *)msgbuf->req.req_detail;
    char *cp = (char *)(si + lis_len);
    for (int i = 0; i < lis_len; i++) {
        si[i] = list[i];
    }
    memcpy(cp, buf, len);
    return msgbuf;
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

static int ap_ipc_kick_start(int fd, char *path, size_t len)
{
    channel_n = random();
    
    Write_lock(fd, 0, SEEK_END, len);
    off_t curoff = lseek(fd, 0, SEEK_END);
    ssize_t w_l = write(fd, path, len);
    if (w_l < len) {
        printf("ipc_path write failed\n");
        exit(1);
    }
    Un_lock(fd, curoff, SEEK_SET, len);
    return 1;
}

static key_t ap_ftok(pid_t pid, char *ipc_path)
{
    int cr_s;
    key_t key;
    cr_s = creat(ipc_path, 777);
    if (cr_s != 0) {
        perror("ap_ipc_file creation failed\n");
        exit(1);
    }
    
    key = ftok(ipc_path, (int)pid);
    return key;
}
static void ap_msgmemcpy(size_t seq, char *base, const char *data, size_t len)
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
        recv_s = msgrcv(msgid, &buf, MSG_LEN, wait_seq, 0);
        if (recv_s == -1) {
            perror("msgcv failed\n");
            exit(1);
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
        ap_msgmemcpy(buf.seq, cc_p, buf.segc, data_l);
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
            perror("msgsnd failed\n");
            exit(1);
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
    *(path + str_len) = '\0';
    struct ap_msgreply re;
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct ap_inode_indicator *strc_cp;
    size_t len;
    
    indic = MALLOC_INODE_INDICATOR();
    struct ap_file_pthread *ap_fpthr = pthread_getspecific(file_thread_key);
    int set = initial_indicator(path, indic, ap_fpthr);
    if (set == -1) {
        re.err = EINVAL;
        re.re_type = -1;
        re.struct_l = 0;
        ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n,0);
        return;
    }
    
    len = sizeof(struct ap_msgreply) + sizeof(*indic) + sizeof(struct ap_inode);
    
    struct ap_msgreply *re_m = Mallocx(len);
    int get = walk_path(indic);
    
    re_m->re_type = get;
    re_m->struct_l = 1;
    re_m->err = errno;
    strc_cp = (struct ap_inode_indicator*)re_m->re_struct;
    *strc_cp = *indic;
    
    if (!get) {
        hl = MALLOC_HOLDER();
        ihl = MALLOC_IPC_INODE_HOLDER();
        char *ide_c = Mallocx(str_len);
        strncpy(ide_c, path, str_len);
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
    size_t str_len = strlen(req_d[0]);
    *(path + str_len) = '\0';
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply *re;
    struct hash_union *un;
    ssize_t read_n;
    size_t len;
    char *read_buf = Mallocz(buf->req.req_t.read_len);
    
    ide.ide_c = path;
    *(ide.ide_c + str_len) = '\0';
    ide.ide_i = buf->pid;
    hl = ipc_holer_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    un = hash_union_get(ihl->ipc_inode_hash, hl->ide);
    if (un == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    inde = ihl->inde;
    file = container_of(un, struct ap_file, f_hash_union);
    read_n = inde->f_ops->read(file, read_buf, buf->req.req_t.off_size, buf->req.req_t.read_len);
    if (read_n < 0) {
        ap_msg_send_err(errno, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    len = sizeof(struct ap_msgreply) + read_n;
    
    re = Mallocz(len);
    re->re_type = read_n;
    re->struct_l = 1;
    memcpy(re->re_struct, read_buf, read_n);
    ap_msgsnd(buf->msgid, re, len, buf->ch_n,0);
    free(re);
    return;
}

static void client_w(struct ap_msgbuf *buf, char **req_d)
{
    
    struct hash_identity ide;
    char *path = req_d[0];
    size_t str_len = strlen(req_d[0]);
    *(path + str_len) = '\0';
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ap_msgreply re;
    struct hash_union *un;
    ssize_t write_n;
    
    ide.ide_c = path;
    *(ide.ide_c + str_len) = '\0';
    ide.ide_i = buf->pid;
    hl = ipc_holer_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    un = hash_union_get(ihl->ipc_inode_hash, hl->ide);
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
    
    re.re_type = write_n;
    re.struct_l = 0;
    ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n,0);
    return;
}

static void client_o(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    size_t str_len = strlen(req_d[0]);
    *(path + str_len) = '\0';
    struct ap_msgreply re;
    struct holder *hl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct ipc_inode_holder *ihl;
    int hash_n;
    
    ide.ide_c = path;
    *(ide.ide_c + str_len) = '\0';
    ide.ide_i = buf->pid;
    hl = ipc_holer_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    
    file = AP_FILE_MALLOC();
    AP_FILE_INIT(file);
    
    file->f_ops = inde->f_ops;
    file->relate_i = inde;
    ap_inode_get(inde);
    
    if (ihl->ipc_inode_hash == NULL) {
        ihl->ipc_inode_hash = MALLOC_IPC_HASH(AP_IPC_FILE_HASH_LEN);
    }
    hash_n = hl->hash_n % AP_IPC_FILE_HASH_LEN;
    file->f_hash_union.ide = hl->ide;
    hash_union_insert(ihl->ipc_inode_hash, &file->f_hash_union);
    
    re.re_type = 0;
    re.struct_l = 0;
    ap_msgsnd(buf->msgid, &re, sizeof(re), buf->ch_n,0);
}

static void client_c(struct ap_msgbuf *buf, char **req_d)
{
    struct hash_identity ide;
    char *path = req_d[0];
    size_t str_len = strlen(req_d[0]);
    *(path + str_len) = '\0';
    struct holder *hl;
    struct ipc_inode_holder *ihl;
    struct ap_inode *inde;
    struct ap_file *file;
    struct hash_union *un;
    
    ide.ide_c = path;
    *(ide.ide_c + str_len) = '\0';
    ide.ide_i = buf->pid;
    hl = ipc_holer_hash_get(ide, 0);
    if (hl == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    ihl = hl->x_object;
    inde = ihl->inde;
    un = hash_union_get(ihl->ipc_inode_hash, hl->ide);
    if (un == NULL) {
        ap_msg_send_err(EINVAL, -1, buf->msgid, buf->ch_n);
        return;
    }
    
    inde = ihl->inde;
    file = container_of(un, struct ap_file, f_hash_union);

    
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
                client_g(msg_buf,req_detail);
                break;
            case w:
                client_w(msg_buf,req_detail);
                break;
            case r:
                client_r(msg_buf,req_detail);
                break;
            case o:
                client_o(msg_buf,req_detail);
            default:
                printf("ap_msg wrong type\n");
                exit(1);
        }
    }
}

static struct ap_inode *proc_get_initial_inode(struct ap_file_system_type *fsyst, void *x_object)
{
    int fd;
    pid_t pid;
    key_t key[2];
    int msgid[2];
    size_t path_len;
    pthread_t thr_n;
    
    char ap_ipc_path[AP_IPC_PATH_LEN];
    char ipc_path[AP_IPC_PATH_LEN];
    char *name = (char *)x_object;
    fd = open(AP_PROC_FILE, O_CREAT, 777);
    if (fd < 0) {
        perror("open failed\n");
        exit(1);
    }
    //key[0]作为服务器端 key[1]作为客户端
    pid = getpid();
    for (int i=0; i<2; i++) {
        snprintf(ipc_path, AP_IPC_PATH_LEN, "/tmp/ap_procs/%ld_%d",(long)pid,i);
        key[i] = ap_ftok(pid,ipc_path);
        msgid[i] = Msgget(key[i], IPC_CREAT);
    }
    
    client_msid = msgid[1];
    client_key = key[1];
    int thr_cr_s = pthread_create(&thr_n, NULL, ap_proc_sever, &msgid[0]);
    if (thr_cr_s != 0) {
        perror("ap_ipc_sever failed\n");
        exit(1);
    }
    
    snprintf(ap_ipc_path, AP_IPC_PATH_LEN, "/tmp/ap_procs/%ld:%s:%ld\n",(long)pid,name,(long)key[0]);
    
    path_len = strlen(ap_ipc_path);
    ap_ipc_kick_start(fd, ap_ipc_path, path_len);
    //initialize inode
}

static char **proc_path_analy(char *path_s, char *path_d)
{
    char *indic,*head;
    head = path_s;
    indic = strchr(path_s, ':');
    size_t str_len;
    if (indic == NULL) {
        printf("proc_file was wrong\n");
        exit(1);
    }
    indic = '\0';
    if (strcmp(path_d, path_s) != 0){
        return NULL;
    }
    str_len = strlen(path_d);
    
    char **analy_r = malloc(sizeof(char*)*3);
    if (analy_r == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    
    char *path_name = malloc(str_len+1);
    if (path_name == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    
    strncpy(path_name, path_d, str_len);
    *(path_name + str_len) = '\0';
    *analy_r = path_name;
    
    head = indic++;
    indic = strchr(head, ':');
    if (indic == NULL) {
        printf("proc_file was wrong\n");
        exit(1);
    }
    
    str_len = strlen(head);
    char *proc_name = malloc(str_len + 1);
    if (proc_name == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    strncpy(proc_name, head, str_len);
    *(proc_name + str_len) = '\0';
    *(analy_r + 1) = proc_name;
    head = indic++;
    
    if (strchr(head, ':') != NULL) {
        printf("proc_file was wrong\n");
        exit(1);
    }
    
    str_len = strlen(head);
    char *key = malloc(str_len + 1);
    if (key == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    
    strncpy(key, head, str_len);
    *(key + str_len) = '\0';
    *(analy_r + 2) = key;
    return analy_r;
}

static int proc_get_inode(struct ap_inode_indicator *indc)
{
    int fd;
    FILE *fs;
    char **cut = NULL;
    char ap_ipc_path[AP_IPC_PATH_LEN];
    
    fd = open(AP_PROC_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open failed\n");
        exit(1);
    }
    fs = fdopen(fd, "r");
    if (fs == NULL) {
        perror("open stream failed\n");
        exit(1);
    }
    Read_lock(fd, 0, SEEK_SET, 0);
    while (fgets(ap_ipc_path, AP_IPC_PATH_LEN, fs) != NULL) {
        cut = proc_path_analy(ap_ipc_path, indc->the_name);
        if (cut != NULL) {
            break;
        }
    }
    Un_lock(fd, 0, SEEK_SET, 0);
    
    if (ferror(fs)) {
        perror("f_stream erro");
        exit(1);
    }
    if (cut == NULL) {
        return -1;
    }
    //initialize inode
}




static int get_proc_inode(struct ap_inode_indicator *indc)
{
    size_t str_len;
    struct ap_msgbuf *buf;
    unsigned long ch_n;
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
    
    msgid = msgget(info->sock.key, 0);
    if (msgid == -1) {
        errno = ENOENT;
        return -1;
    }
    int sent_s = ap_msgsnd(msgid, buf, sizeof(struct ap_msgbuf) + str_len, 0, 0);
    if (sent_s == -1) {
        errno = ENOENT;
        return -1;
    }
    ssize_t recv_s = ap_msgrcv(client_msid, &msg_reply, ch_n);
    if (recv_s == -1) {
        errno = ENOENT;
        return -1;
    }
    //initialize inode
    struct ap_msgreply *msgre = (struct ap_msgreply *)msg_reply;
    if (msgre->re_type != 0 && !msgre->struct_l) {
        errno = msgre->err;
        return (int)msgre->re_type;
    }

    f_indc = (struct ap_inode_indicator*)msgre->re_struct;
    indc->p_state = f_indc->p_state;
    if(msgre->re_type){
        errno = msgre->err;
        return (int)msgre->re_type;
    }
    
    f_ind = (struct ap_inode *)(f_indc + 1);
    cur_ind = MALLOC_AP_INODE();
    cur_ind->is_dir = f_ind->is_dir;
    cur_ind->parent = indc->cur_inode;
    
    sock = Mallocx(sizeof(*sock));
    *sock = info->sock;
    cur_ind->x_object = sock;
    ap_inode_get(cur_ind);
    indc->cur_inode = cur_ind;
    
    free(msg_reply);
    free(buf);
    return (int)msgre->re_type;
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
    strncpy(path, ide.ide_c, strl);
    *(path + strl) = '\0';
    
    indc->cur_inode->ipc_path_hash.ide.ide_c = path;
    indc->cur_inode->ipc_path_hash.ide.ide_i = 0;
    hash_union_insert(info->inde_hash_table, &indc->cur_inode->ipc_path_hash);
    return get;
}

static ssize_t proc_read(struct ap_file *file, char *buf, off_t off, size_t size)
{
    struct ap_inode *inde = file->relate_i;
    struct ipc_sock *sock = (struct ipc_sock *)inde->x_object;
    int msgid;
    struct ap_msgbuf *msgbuf;
    char *path = inde->ipc_path_hash.ide.ide_c;
    pid_t pid = getpid();
    msgid = msgget(sock->key, 0);
    if (msgid == -1) {
        errno = ENOEXEC;
        hash_union_delet(&inde->ipc_path_hash);
        return -1;
    }
    
    size_t str_len = strlen(path);
    size_t list[] = {str_len};
    msgbuf = constr_req(path, str_len, list, 1);
    msgbuf->req.req_t.op_type = r;
    msgbuf->req.req_t.read_len = size;
    msgbuf->req.req_t.off_size = off;
    msgbuf->pid = pid;
    msgbuf->ch_n = get_channel();
    msgbuf->key = client_key;
    ap_msgsnd(sock->key, msgbuf, sizeof(struct ap_msgbuf) + str_len, msgbuf->ch_n, 0);
    
    
    
    
    
    
}









