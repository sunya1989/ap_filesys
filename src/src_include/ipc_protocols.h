/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_ipc_protocols_h
#define ap_file_system_ipc_protocols_h
#include <ap_ipc.h>
typedef enum op_type{
    g = 0,
    w,
    r,
    rdir,
    cdir,
    seek,
    o,
    c,
    d,
}op_type_t;

struct ap_msgrep_type{
    int err;
    ssize_t re_type;
    size_t read_n;
    size_t write_n;
    int readdir_is_done;
};

struct ap_msgreply{
    struct ap_msgrep_type rep_t;
    int ipc_fd;
    int struct_l;
    char re_struct[0];
};

struct ap_msgreq_type{
    op_type_t op_type;
    struct ipc_file_op_info{
        pmide_t pm_ide;
        unsigned long flags;
        size_t read_len;
        size_t wirte_len;
        off_t off_size;
        int orign;
    }f_o_info;
};

struct ap_msgreq{
    struct ap_msgreq_type req_t;
    int index_lenth; //the lenth of the list of the lenth of every single req_detail
    char req_detail[0];
};

struct ap_msgbuf{
    struct ap_msgreq req;
};

extern struct ap_ipc_operations *ap_ipc_pro_ops[IPC_TYP_NUM];
#endif
