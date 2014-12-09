//
//  ger_file_p.h
//  ap_file_system
//
//  Created by sunya on 14/11/29.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#ifndef ap_file_system_ger_file_p_h
#define ap_file_system_ger_file_p_h
#include <pthread.h>
#include "list.h"
#include "ger_fs.h"


struct ger_file_struct{
    char *name;
    enum file_state state;
    int fd;
    FILE *file_stream;
};

struct ger_dir{
    char *name;
    pthread_mutex_t ch_lock;
    struct list_head children;
    struct list_head child;
};


struct ger_raw_hash_table{
    
};


#endif
