//
//  proc_fs.h
//  ap_editor
//
//  Created by sunya on 15/3/27.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_proc_fs_h
#define ap_file_system_proc_fs_h
#include <ap_ipc.h>
#define AP_PROC_FILE "/tmp/ap_procs"
#define AP_PROC_DISC_F "/tmp/ap_proc_disc"
#define MY_DATA_LEN 2048
#define AP_MSGSEG_LEN 2048
#define AP_PNOEXIST 0
#define PROC_FILE_FS "proc"

struct ap_ipc_info{
    struct ipc_sock sock;
    int disc;
    struct ap_hash *inde_hash_table;
};

static inline struct ap_ipc_info *MALLOC_IPC_INFO()
{
    struct ap_ipc_info *info = Mallocz(sizeof(*info));
    info->inde_hash_table = MALLOC_IPC_HASH(AP_IPC_INODE_HASH_LEN);
    return info;
}

typedef enum op_type{
    g = 0,
    w,
    r,
    rdir,
    cdir,
    o,
    c,
    d,
}op_type_t;

struct ap_msgreq_type{
    op_type_t op_type;
    unsigned long flags;
    size_t read_len;
    size_t wirte_len;
    off_t off_size;
};

struct ap_msgrep_type{
    errno_t err;
    ssize_t re_type;
    size_t read_n;
    size_t write_n;
};

struct ap_msgreq{
    struct ap_msgreq_type req_t;
    int index_lenth; //the lenth of the list of the lenth of every single req_detail
    char req_detail[0];
};

struct ap_msgbuf{
    key_t key;
    pid_t pid;
    int msgid;
    unsigned long ch_n;
    struct ap_msgreq req;
};

struct ap_msgseg{
    long mtype;
    size_t seq;
    size_t len_t; //the lenth of whole message
    int data_len; //the lenth of single segment
    char segc[AP_MSGSEG_LEN];
};

struct ap_msgreply{
    struct ap_msgrep_type rep_t;
    int ipc_fd;
    int struct_l;
    char re_struct[0];
};

extern int init_proc_fs();

#endif
