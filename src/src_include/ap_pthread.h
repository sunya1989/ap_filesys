//
//  ap_pthread.h
//  ap_file_system
//
//  Created by HU XUKAI on 14/11/18.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//

#ifndef ap_file_system_ap_pthread_h
#define ap_file_system_ap_pthread_h
#include <stdio.h>
#include <ap_fs.h>

extern pthread_key_t file_thread_key;

struct ap_file_pthread{
    char path[FULL_PATH_LEN];
    struct ap_inode *m_wd, *c_wd;
};

static inline void AP_FILE_THREAD_INIT(struct ap_file_pthread *fpth){
    struct ap_inode *tmp = f_root.f_root_inode;
    fpth->c_wd = tmp->real_inode;
    fpth->m_wd = tmp->real_inode;
    ap_inode_get(tmp->real_inode);
}

#endif
