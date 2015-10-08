/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */

#ifndef ap_file_system_system_v_h
#define ap_file_system_system_v_h

#include <stdio.h>
#include <ap_hash.h>
#include <ipc_protocols.h>
#define AP_MSGSEG_LEN 1024
#define AP_PNOEXIST 0
#define MSG_LEN (sizeof(struct ap_msgseg) - sizeof(long))
extern struct ap_ipc_operations system_v_ops;
struct ap_msgseg{
    long mtype;
    size_t seq;
    size_t len_t; //the lenth of whole message
    int data_len; //the lenth of single segment
    char segc[AP_MSGSEG_LEN];
};
#endif
