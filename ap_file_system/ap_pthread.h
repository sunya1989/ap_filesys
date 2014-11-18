//
//  ap_pthread.h
//  ap_file_system
//
//  Created by sunya on 14/11/18.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#ifndef ap_file_system_ap_pthread_h
#define ap_file_system_ap_pthread_h
#include"ap_fs.h"

pthread_key_t file_thread_key;

struct ap_file_pthread{
    struct ap_inode_indicator *m_wd, *c_wd;
};

static inline AP_FILE_THREAD_INIT(struct ap_file_pthread *fpth){
    fpth->c_wd = fpth->m_wd = NULL;
}

#endif
