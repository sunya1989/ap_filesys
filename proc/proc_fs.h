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
#define AP_PROC_FILE "/var/lock/ap_procs"
#define MY_DATA_LEN 2048
#define AP_PNOEXIST 0

struct ap_ipc_info{
    struct ipc_sock sock;
    struct ap_hash *inde_hash_table;
};

typedef enum op_type{
    g = 0,
    w,
    r,
    o,
    lock_op,
}op_type_t;

struct ap_msgreq_type{
    op_type_t op_type;
    size_t read_len;
    size_t wirte_len;
    off_t off_size;
};

struct ap_msgbuf{
    long mtype;
    struct ap_msgreq_type req_t;
    key_t key;
    pid_t pid;
    
    size_t len_t; //the lenth of whole message
    int data_len; //the lenth of single segment
    
    unsigned long ch_n;
    char mchar[MY_DATA_LEN];
};

struct ap_msgreply{
    int re_type;
    errno_t err;
    int struct_l;
    char re_struct[0];
};

#endif
