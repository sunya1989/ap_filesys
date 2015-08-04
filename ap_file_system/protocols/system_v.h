//
//  system_v.h
//  ap_editor
//
//  Created by sunya on 15/8/2.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_system_v_h
#define ap_file_system_system_v_h

#include <stdio.h>
#include <ap_hash.h>
#include <ipc_protcols.h>
#define MY_DATA_LEN 2048
#define AP_MSGSEG_LEN 2048
#define AP_PNOEXIST 0
#define MSG_LEN (sizeof(struct ap_msgseg) - sizeof(long))
extern struct ap_ipc_operations sys_tem_v_ops;

struct ap_msgseg{
    long mtype;
    size_t seq;
    size_t len_t; //the lenth of whole message
    int data_len; //the lenth of single segment
    char segc[AP_MSGSEG_LEN];
};
#endif
