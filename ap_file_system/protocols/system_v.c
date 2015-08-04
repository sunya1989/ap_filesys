//
//  system_v.c
//  ap_editor
//
//  Created by sunya on 15/8/2.
//  Copyright (c) 2015年 sunya. All rights reserved.
//
#include <ap_string.h>
#include "system_v.h"

struct msg_recv_hint{
    unsigned long ch_n;
};

struct msg_connect_info{
    pthread_mutex_t channel_lock;
    unsigned long channel_n;
    const char *sever_name;
    pid_t pid;
    int msgid;
    key_t key;
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
        if (dl>0) {
            c_p += AP_MSGSEG_LEN;
        }
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

static inline unsigned long get_channel(struct msg_connect_info *info)
{
    unsigned long ch_n;
    pthread_mutex_lock(&info->channel_lock);
    ch_n = info->channel_n;
    info->channel_n++;
    pthread_mutex_unlock(&info->channel_lock);
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

int ipc_connect(struct ap_ipc_info *info)
{
    char **cut;
    const char *txt = info->cs_hint.hint_txt;
    const char *c_name = info->cs_hint.client_hint.s_name;
    char path[AP_IPC_PATH_LEN];
    cut = cut_str(txt, ':', AP_IPC_RECODE_NUM);
    if (msgget(atoi(cut[AP_IPC_KEY]), 0) == -1) {
        return -1;
    }
    const char *paths[] = {
        AP_PROC_FILE,
        c_name,
    };
    path_names_cat(path, paths, 2, "/");
    
    
}

static ssize_t v_ipc_send
(struct ap_ipc_info *info, void *buf, size_t len, struct package_hint *hint)
{
    struct msg_connect_info *s_info = info->cs_hint.sever_hint.hint_bob;
    struct msg_recv_hint *r_h = Mallocz(sizeof(*r_h));
    ssize_t send_s;
    if (hint != NULL) {
        r_h->ch_n = get_channel(s_info);
        hint->p_hint = r_h;
        hint->p_release = v_package_hint_release;
    }
    send_s = v_msgsnd(s_info->msgid, buf, len, r_h->ch_n, 0);
    return send_s;
}

static ssize_t v_ipc_recv
(struct ap_ipc_info *info, void **buf, size_t len, struct package_hint *hint)
{
    struct msg_connect_info *c_info = info->cs_hint.client_hint.hint_bob;
    unsigned long wait_ch = 0;
    if (hint->p_hint != NULL) {
        struct msg_recv_hint *r_h = hint->p_hint;
        wait_ch = r_h->ch_n;
    }
    ssize_t recv_s = v_msgrcv(c_info->msgid, buf, wait_ch);
    return recv_s;
}






















