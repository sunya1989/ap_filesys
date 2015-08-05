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

struct msg_recv_hint{
    unsigned long ch_n;
};

struct v_head{
    pid_t pid;
    int msgid;
    key_t key;
};

struct v_port{
    union{
        struct v_head c_head;
        struct{
            struct v_head head;
            pthread_mutex_t channel_lock;
            unsigned long channel_n;
        }channel;
    };
};

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

static struct v_head *v_creat_file(const char *ipc_path, mode_t mode)
{
    pid_t pid = getpid();
    errno = 0;
    key_t key;
    int msgid;
    int mkd = mkdir(ipc_path, 0755);
    if (mkd == -1 && errno != EEXIST) {
        return NULL;
    }
    struct v_head *head = Mallocz(sizeof(*head));
    key = ap_ftok(pid,ipc_path);
    if (key == -1) {
        errno = EBADF;
        return NULL;
    }
    msgid = msgget(key, IPC_CREAT | mode);
    if (msgid == -1) {
        errno = EBADF;
        return NULL;
    }
    head->key = key;
    head->msgid = msgid;
    return head;
}

static inline unsigned long get_channel(struct v_port *info)
{
    unsigned long ch_n;
    pthread_mutex_lock(&info->channel.channel_lock);
    ch_n = info->channel.channel_n;
    info->channel.channel_n++;
    pthread_mutex_unlock(&info->channel.channel_lock);
    return ch_n;
}

static void v_package_hint_release(struct package_hint *hint)
{
    free(hint->p_hint);
    return;
}

static char **cut_str(const char *s, char d, size_t len)
{
    char *s_p,*head;
    size_t p_l = strlen(s);
    char *tm_path = Mallocz(p_l+1);
    strncpy(tm_path, s, p_l);
    head = tm_path;
    char **cut = Mallocz(sizeof(char*)*len);
    int i = 0;
    
    while (i < AP_IPC_RECODE_NUM) {
        s_p = strchr(head, d);
        size_t str_len;
        if (s_p != NULL) {
            *s_p = '\0';
        }
        str_len = strlen(head);
        char *item = Mallocz(str_len+1);
        strncpy(item, head, str_len);
        *(item + str_len) = '\0';
        cut[i] = item;
        head = ++s_p;
        i++;
    }
    free(tm_path);
    return cut;
}

static int v_ipc_connect(struct ap_ipc_port *port)
{
    pid_t pid;
    key_t key;
    int msgid;
    char **cut;
    struct v_port *v_port;
    const char *txt = port->port_dis;
    cut = cut_str(txt, ':', AP_IPC_RECODE_NUM);
    
    pid = atoi(cut[AP_IPC_PID]);
    key = atoi(cut[AP_IPC_KEY]);
    msgid = atoi(cut[AP_IPC_MSGID]);
    
    if (msgget(key, 0) == -1)
        return -1;
    
    v_port = Mallocz(sizeof(*v_port));
    v_port->channel.head.key = key;
    v_port->channel.head.msgid = msgid;
    v_port->channel.head.pid = pid;
    port->x_object = v_port;
    
    return 0;
}

static int ipc_get_port(const char *pathname)
{
    if (<#condition#>) {
        <#statements#>
    }
    
}

static ssize_t v_ipc_send
(struct ap_ipc_port *port, void *buf, size_t len, struct package_hint *hint)
{
    struct v_port *s_port = port->x_object;
    struct msg_recv_hint *r_h = Mallocz(sizeof(*r_h));
    ssize_t send_s;
    if (hint != NULL) {
        r_h->ch_n = get_channel(s_port);
        hint->p_hint = r_h;
        hint->p_release = v_package_hint_release;
    }
    send_s = v_msgsnd(s_port->channel.head.msgid, buf, len, r_h->ch_n, 0);
    return send_s;
}

static ssize_t v_ipc_recv
(struct ap_ipc_port *port, void **buf, size_t len, struct package_hint *hint)
{
    struct v_port *c_port = port->x_object;
    unsigned long wait_ch = 0;
    if (hint->p_hint != NULL) {
        struct msg_recv_hint *r_h = hint->p_hint;
        wait_ch = r_h->ch_n;
    }
    ssize_t recv_s = v_msgrcv(c_port->c_head->msgid, buf, wait_ch);
    return recv_s;
}
