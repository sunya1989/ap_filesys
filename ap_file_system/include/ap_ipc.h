//
//  Header.h
//  ap_editor
//
//  Created by sunya on 15/4/23.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_ap_ipc_h
#define ap_file_system_ap_ipc_h
#include <list.h>
#include <fcntl.h>
#include <errno.h>
#include <ap_hash.h>

#define AP_IPC_PATH_CIL "/tmp/ap_procs/%ld_0"
#define AP_IPC_PATH_SER "/tmp/ap_procs/%ld_1"
#define AP_PROC_FILE "/tmp/ap_procs"
#define AP_PROC_DISC_F "/tmp/ap_proc_disc"
#define AP_IPC_PATH_LEN 512
#define AP_IPC_RECODE_LEN 100


/* our record locking macros */
#define	read_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define	readw_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define	write_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define	writew_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#define	un_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)
#define	is_read_lockable(fd, offset, whence, len) \
lock_test(fd, F_RDLCK, offset, whence, len)
#define	is_write_lockable(fd, offset, whence, len) \
lock_test(fd, F_WRLCK, offset, whence, len)
/* end unpipch */

#define	Read_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define	Readw_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define	Write_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define	Writew_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#define	Un_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)
#define	Is_read_lockable(fd, offset, whence, len) \
Lock_test(fd, F_RDLCK, offset, whence, len)
#define	Is_write_lockable(fd, offset, whence, len) \
Lock_test(fd, F_WRLCK, offset, whence, len)

int lock_reg(int, int, int, off_t, int, off_t);
void Lock_reg(int, int, int, off_t, int, off_t);

#define AP_CONNECT_T 0
#define AP_IPC_PID 1
#define AP_IPC_KEY 2
#define AP_IPC_MSGID 3
#define AP_IPC_RECODE_NUM 4
struct ap_ipc_operations;
enum connet_typ{
    SYSTEM_V = 0,
    TYP_NUM,
};

struct ipc_sock{
    char *sever_name;
    pid_t pid;
    int msgid;
    key_t key;
};

static inline struct ipc_sock *MALLOC_IPC_SOCK()
{
    struct ipc_sock *ipc_s = Mallocx(sizeof(*ipc_s));
    ipc_s->sever_name = NULL;
    return ipc_s;
}

struct package_hint{
    void *p_hint;
    void (*p_release)(struct package_hint *);
};

struct ap_ipc_info_head;
struct ap_ipc_port{
    struct ap_ipc_operations *ipc_ops;
    const char *port_dis;
    void *x_object;
};

struct ap_ipc_info{
    struct ap_ipc_info_head *info_h;
    struct ap_ipc_port *s_port;
    struct ipc_sock sock;
    int disc;
    enum connet_typ s_t;                /*connect type of the severend*/
    struct ap_hash *inde_hash_table;
};

struct ap_ipc_info_head{
    void *ipc_heads[TYP_NUM];
    struct ap_ipc_port *cs_port;
    pthread_mutex_t typ_lock;
    enum connet_typ c_t[TYP_NUM];       /*connect types that clientend possess*/
};

static inline struct ap_ipc_info_head *MALLOC_IPC_INFO_HEAD()
{
    struct ap_ipc_info_head *h_info;
    h_info = Mallocz(sizeof(*h_info));
    pthread_mutex_init(&h_info->typ_lock, NULL);
    return h_info;
}

static inline void IPC_INFO_HEAD_FREE(struct ap_ipc_info_head *h_info)
{
    pthread_mutex_destroy(&h_info->typ_lock);
    free(h_info);
}

static inline struct ap_ipc_info *MALLOC_IPC_INFO()
{
    struct ap_ipc_info *info = Mallocz(sizeof(*info));
    info->inde_hash_table = MALLOC_IPC_HASH(AP_IPC_INODE_HASH_LEN);
    return info;
}

struct ap_ipc_operations{
    struct ap_ipc_port *(*ipc_get_port)(const char *);
    int (*ipc_connect)(struct ap_ipc_port *);
    ssize_t (*ipc_send)
    (struct ap_ipc_port *, void *, size_t, struct package_hint *);
    ssize_t (*ipc_recv)
    (struct ap_ipc_port *, void **, size_t, struct package_hint *, struct ap_ipc_port *);
    int (*ipc_close)(struct ap_ipc_port *);
};


#endif
