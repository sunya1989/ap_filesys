//
//  system_v.c
//  ap_editor
//
//  Created by sunya on 15/8/2.
//  Copyright (c) 2015年 sunya. All rights reserved.
//
#include <ap_string.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include "system_v.h"
#define V_PATH_CAT "@SYSTEM_V"

struct msg_recv_hint{
    unsigned long ch_n;
};
struct v_head{
    pid_t pid;
    int msgid;
    key_t key;
};
struct v_port{
    struct v_head head;
    unsigned long channel;
    pthread_mutex_t ch_lock;
};

struct v_msgbuf{
    struct v_port port;
    char buf[0];
};

static inline struct v_port *MALLOC_V_PORT()
{
    struct v_port *v_port = Mallocz(sizeof(*v_port));
    pthread_mutex_init(&v_port->ch_lock, NULL);
    return v_port;
}

static inline void V_PORT_FREE(struct v_port *v_port)
{
    pthread_mutex_destroy(&v_port->ch_lock);
    free(v_port);
}

static void msgmemcpy(size_t seq, char *base, const char *data, size_t len)
{
    char *cp = base + (seq*AP_MSGSEG_LEN);
    memcpy(cp, data, len);
}

static int
v_msgsnd(int msgid, const void *buf, size_t len, unsigned long ch_n, int msgflg)
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
        if (dl>0) 
            c_p += AP_MSGSEG_LEN;
        seq++;
    }
    return 0;
}

static ssize_t v_msgrcv(int msgid, void **d_buf, unsigned long wait_seq)
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
            if (dl == 0)
                goto FINISH;
            
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

static key_t ap_ftok(pid_t pid, const char *ipc_path)
{
    int cr_s;
    key_t key;
    
    open(ipc_path, O_CREAT | O_TRUNC | O_RDWR);
    if (cr_s == -1)
        return -1;
    chmod(ipc_path, 0777);
    close(cr_s);
    key = ftok(ipc_path, (int)pid);
    return key;
}

static inline unsigned long get_channel(struct v_port *port)
{
    unsigned long ch_n;
    pthread_mutex_lock(&port->ch_lock);
    ch_n = port->channel;
    port->channel++;
    pthread_mutex_unlock(&port->ch_lock);
    return ch_n;
}

static void v_package_hint_release(struct package_hint *hint)
{
    free(hint->p_hint);
    return;
}

static ppair_t *v_ipc_connect(struct ap_ipc_port *port, const char *local_addr)
{
    pid_t pid;
    key_t key;
    int msgid;
    char **cut;
    struct v_port *v_port;
    struct ap_ipc_port *c_port;
    const char *txt = port->port_dis;
    cut = cut_str(txt, ':', AP_IPC_RECODE_NUM);
    ppair_t *pair = Mallocz(sizeof(*pair));
    
    pid = atoi(cut[AP_IPC_PID]);
    key = atoi(cut[AP_IPC_KEY]);
    msgid = atoi(cut[AP_IPC_MSGID]);
    
    if (msgget(key, 0) == -1)
        return NULL;
    
    if (ipc_c_ports[SYSTEM_V] == NULL) {
        c_port = get_ipc_c_port(SYSTEM_V, local_addr);
        if (c_port == NULL) {
            return NULL;
        }
    }
    
    v_port = MALLOC_V_PORT();
    v_port->head.key = key;
    v_port->head.msgid = msgid;
    v_port->head.pid = pid;
    v_port->channel = random();
    port->x_object = v_port;
    
    pair->far_port = port;
    pair->local_port = c_port;
    
    return pair;
}

static struct ap_ipc_port *v_ipc_get_port(const char *pathname, mode_t mode)
{
    pid_t pid = getpid();
    key_t key;
    int msgid;
    struct ap_ipc_port *port;
    struct v_port *v_port;
    char p[AP_IPC_PATH_LEN];
    char dis[AP_IPC_RECODE_LEN];
    memset(dis, '\0', AP_IPC_RECODE_LEN);
    memset(p, '\0', AP_IPC_PATH_LEN);
    strncpy(p, pathname, strlen(pathname));
    strcat(p, V_PATH_CAT);
    
    key = ap_ftok(pid, p);
    if (key == -1)
        return NULL;
    errno = 0;
    msgid = msgget(key, IPC_CREAT | mode);
    if (msgid == -1)
        return NULL;
    v_port = MALLOC_V_PORT();
    v_port->head.key = key;
    v_port->head.msgid = msgid;
    v_port->head.pid = pid;
    /*connect type:pid:key:msgid*/
    sprintf(dis, "%d:%ld:%d:%d",SYSTEM_V,(long)pid,key,msgid);
    size_t len = strlen(dis);
    char *p_dis = Mallocz(len + 1);
    strncpy(p_dis, dis, len);
    
    port = Mallocz(sizeof(*port));
    port->port_dis = p_dis;
    port->x_object = v_port;
    port->ipc_ops = ap_ipc_pro_ops[SYSTEM_V];
    return port;
}

static ssize_t v_ipc_send
(struct ap_ipc_port *port, void *buf, size_t len, struct package_hint *hint)
{
    struct ap_ipc_port *recv_port = ipc_c_ports[SYSTEM_V];
    if (recv_port == NULL) {
        return -1;
    }
    struct v_port *recv_v_port = recv_port->x_object;
    struct v_port *v_port = port->x_object;
    struct msg_recv_hint *r_h = Mallocz(sizeof(*r_h));
    size_t s_l = sizeof(struct v_msgbuf) + len;
    struct v_msgbuf *v_buf = Mallocz(s_l);
    memcpy(v_buf->buf, buf, len);
    ssize_t send_s;
    unsigned long ch_n = get_channel(recv_v_port);
    if (hint != NULL) {
        r_h->ch_n = ch_n;
        hint->p_hint = r_h;
        hint->p_release = v_package_hint_release;
    }
    
    v_buf->port.channel = ch_n;
    v_buf->port.head = recv_v_port->head;
    send_s = v_msgsnd(v_port->head.msgid, v_buf, s_l, v_port->channel, 0);
    return send_s;
}

static ssize_t v_ipc_recv
(struct ap_ipc_port *port, void **buf, size_t len, struct package_hint *hint, struct ap_ipc_port *p_port)
{
    struct v_port *c_port = port->x_object;
    unsigned long wait_ch = 0;
    struct v_port *v_port;
    if (hint != NULL) {
        struct msg_recv_hint *r_h = hint->p_hint;
        wait_ch = r_h->ch_n;
    }
    ssize_t recv_s = v_msgrcv(c_port->head.msgid, buf, wait_ch);
    if (recv_s == -1) {
        return -1;
    }
    struct v_msgbuf *v_buf = *buf;
    if (p_port != NULL) {
        v_port = p_port->x_object = MALLOC_V_PORT();
        v_port->head = v_buf->port.head;
        v_port->channel = v_buf->port.channel;
        p_port->ipc_ops = &system_v_ops;
    }
    size_t t_rv = recv_s - sizeof(*v_port);
    char *d_buf = Mallocz(t_rv);
    memcpy(d_buf, v_buf->buf, t_rv);
    free(*buf);
    *buf = d_buf;
    return t_rv;
}

static int v_ipc_close(struct ap_ipc_port *port)
{
    if (port == ipc_c_ports[SYSTEM_V]) {
        return 0;
    }
    
    struct v_port *v_port = port->x_object;
    V_PORT_FREE(v_port);
    return 0;
}

struct ap_ipc_operations system_v_ops = {
    .ipc_get_port = v_ipc_get_port,
    .ipc_connect = v_ipc_connect,
    .ipc_send = v_ipc_send,
    .ipc_recv = v_ipc_recv,
    .ipc_close = v_ipc_close,
};
