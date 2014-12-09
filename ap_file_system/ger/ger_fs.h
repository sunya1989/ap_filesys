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

enum file_state{
    g_fileno = 0,
    g_stream,
};

struct con_file{
    char *path;
    char *name;
    char *target_file;
};



#endif
