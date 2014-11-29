//
//  ger_fs.h
//  ap_file_system
//
//  Created by sunya on 14/11/29.
//  Copyright (c) 2014年 sunya. All rights reserved.
//
#ifndef ap_file_system_ger_fs_h
#define ap_file_system_ger_fs_h
#include <stdio.h>
#include <pthread.h>
#include "list.h"
#include "ger_file_p"

struct ger_file_struct{
    char *name;
    file_state state;
    int fd;
    FILE *file_stream;
};

struct ger_dir{
    char *name;
    pthread_mutex_t ch_lock;
    struct list_head children;
    struct list_head child;
};

#endif
