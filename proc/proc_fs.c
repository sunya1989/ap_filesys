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
#include "proc_fs.h"
#define IPC_PATH 0
#define PROC_NAME 1
#define IPC_KEY 2
#define MSG_LEN (sizeof(struct ap_msgbuf) - sizeof(long))
static int main_msid;
static key_t main_key;

pthread_mutex_t channel_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long channel_n;

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

static ssize_t ap_msgrcv(int msgid, char **d_buf, unsigned long wait_seq)
{
    struct ap_msgbuf buf;
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
            rv = dl = buf.len_t;
            if (dl == 0) {
                goto FINISH;
            }
            *d_buf = Malloc(dl);
            c_p = *d_buf;
        }
        data_l = buf.data_len;
        memcpy(c_p, buf.mchar, data_l);
        dl -= data_l;
        c_p += (data_l -1);
    } while (dl>0);
    
FINISH:
    return rv;
}

static int ap_msgsnd(int msgid, const void *buf, size_t len, int flag, unsigned long ch_n)
{
    const char *c_p = buf;
    size_t dl = len;
    int msgsnd_s;
    struct ap_msgbuf msg_buf;
    
    while (dl > 0) {
        
        msgsnd_s = msgsnd(msgid, c_p, MSG_LEN, 0);
        if (msgsnd_s == -1) {
            perror("msgsnd failed\n");
            exit(1);
        }
        dl -= MSG_LEN;
        if (dl>0) {
            c_p += MSG_LEN - 1;
        }
    }
}

static void client_g(int msgid, struct ap_msgbuf *buf, unsigned long ch_n)
{
    
}


void *ap_proc_sever(void *arg)
{
    ssize_t recv_s;
    int *msgid = arg;
    struct ap_msgbuf buf;
    unsigned long ch_n;
    op_type_t type;
    while (1) {
        recv_s = msgrcv(*msgid, &buf, MSG_LEN, 0, 0);
        type = buf.op_type;
        ch_n = buf.ch_n;
        switch (type) {
            case g:
                client_g(*msgid, &buf, ch_n);
                break;
            case w:
                break;
            case r:
                break;
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
    
    pid = getpid();
    for (int i=0; i<2; i++) {
        snprintf(ipc_path, AP_IPC_PATH_LEN, "/tmp/ap_procs/%ld_%d",(long)pid,i);
        key[i] = ap_ftok(pid,ipc_path);
        msgid[i] = msgget(key[i], IPC_CREAT);
        if (msgid[i] == -1) {
            perror("msgget error");
            exit(1);
        }
    }
    
    main_msid = msgid[1];
    main_key = key[1];
    int thr_cr_s = pthread_create(&thr_n, NULL, ap_proc_sever, &msgid[0]);
    if (thr_cr_s != 0) {
        perror("ap_ipc_sever failed\n");
        exit(1);
    }
    
    snprintf(ap_ipc_path, AP_IPC_PATH_LEN, "/tmp/ap_procs/%ld:%s:%ld\n",
             (long)pid,name,(long)key[0]);
    
    path_len = strlen(ap_ipc_path);
    ap_ipc_kick_start(fd, ap_ipc_path, path_len);
    return NULL;
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
}

static int get_proc_inode(struct ap_inode_indicator *indc)
{
    size_t str_len;
    struct ap_msgbuf buf;
    unsigned long ch_n;
    
    struct ap_ipc_info *info;
    *indc->cur_slash = '/';
    info = (struct ap_ipc_info *)indc->cur_inode->x_object;
    
    str_len = strlen(indc->the_name);
    buf.data_len = (int)str_len;
    buf.mtype = g;
    buf.op_type = g;
    bzero(buf.mchar, MY_DATA_LEN);
    pthread_mutex_lock(&channel_lock);
    ch_n = channel_n;
    buf.ch_n = channel_n;
    buf.key = main_key;
    channel_n++;
    pthread_mutex_unlock(&channel_lock);
    strncpy(buf.mchar, indc->the_name, str_len);
    
    int sent_s = msgsnd(info->msgid, &buf, MSG_LEN, 0);
    if (sent_s == -1) {
        perror("msgsent failed\n");
        exit(1);
    }
    ssize_t recv_s = msgrcv(main_msid, &buf, MSG_LEN, ch_n, 0);
    if (recv_s == -1) {
        perror("msgrcv failed\n");
        exit(1);
    }
    //initialize inode
    return 0;
}









