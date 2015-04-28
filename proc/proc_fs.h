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
    int msgid;
};

typedef enum op_type{
    g = 0,
    w,
    r,
}op_type_t;

struct ap_msgbuf{
    long mtype;
    op_type_t op_type;
    key_t key;
    int len_t;
    int data_len;
    unsigned long ch_n;
    char mchar[MY_DATA_LEN];
};

#endif
