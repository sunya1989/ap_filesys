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
#include "proc_fs.h"
#define IPC_PATH 0
#define PROC_NAME 1
#define IPC_KEY 2
#define MSG_LEN (sizeof(struct ap_msgbuf) - sizeof(long))

pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long seq;

static int ap_ipc_kick_start(int fd, char *path, size_t len)
{
    seq = random();
    
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


static key_t ap_ftok(pid_t pid)
{
    int cr_s;
    key_t key;
    char ipc_path[AP_IPC_PATH_LEN];
    snprintf(ipc_path,AP_IPC_PATH_LEN, AP_IPC_PATH,(long)pid);
    cr_s = creat(ipc_path, 777);
    if (cr_s != 0) {
        perror("ap_ipc_file creation failed\n");
        exit(1);
    }
    
    key = ftok(ipc_path, (int)pid);
    return key;
}

void *ap_proc_sever(void *arg)
{
    return NULL;
}

static struct ap_inode *proc_get_initial_inode(struct ap_file_system_type *fsyst, void *x_object)
{
    int fd;
    pid_t pid;
    key_t key;
    size_t path_len;
    pthread_t thr_n;
    
    char ap_ipc_path[AP_IPC_PATH_LEN];
    char *name = (char *)x_object;
    fd = open(AP_PROC_FILE, O_CREAT, 777);
    if (fd < 0) {
        perror("open failed\n");
        exit(1);
    }
    
    pid = getpid();
    key = ap_ftok(pid);
    int thr_cr_s = pthread_create(&thr_n, NULL, ap_proc_sever, &key);
    if (thr_cr_s != 0) {
        perror("ap_ipc_sever failed\n");
        exit(1);
    }
    
    snprintf(ap_ipc_path, AP_IPC_PATH_LEN, "/tmp/ap_procs/%ld:%s:%ld\n", (long)pid,name,(long)key);
    path_len = strlen(ap_ipc_path);
    
    int msgget_s = msgget(key, IPC_CREAT);
    if (msgget_s == -1) {
        perror("msgget error");
        exit(1);
    }
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
    return 0;
}

static int get_proc_inode(struct ap_inode_indicator *indc)
{
    size_t str_len;
    struct ap_msgbuf buf;
    unsigned long wait_seq;
    
    struct ap_ipc_info *info;
    *indc->cur_slash = '/';
    info = (struct ap_ipc_info *)indc->cur_inode->x_object;
    
    str_len = strlen(indc->the_name);
    buf.data_len = (int)str_len;
    buf.mtype = g;
    buf.op_type = g;
    bzero(buf.mchar, MY_DATA_LEN);
    pthread_mutex_lock(&seq_lock);
    wait_seq = seq;
    buf.sqe = seq;
    seq++;
    pthread_mutex_unlock(&seq_lock);
    strncpy(buf.mchar, indc->the_name, str_len);
    
    int sent_s = msgsnd(info->key, &buf, MSG_LEN, 0);
    if (sent_s == -1) {
        perror("msgsent failed\n");
        exit(1);
    }
    ssize_t recv_s = msgrcv(info->key, &buf, MSG_LEN, wait_seq, 0);
    if (recv_s == -1) {
        perror("msgrcv failed\n");
        exit(1);
    }
    //initialize inode
    return 0;
}










