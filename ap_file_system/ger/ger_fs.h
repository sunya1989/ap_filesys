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
struct counter;

enum file_state{
    g_fileno = 0,
    g_stream,
};

struct ger_con_file{
    char *path;
    char *name;
    char *target_file;
    struct counter *in_use;
    enum file_state state;
    struct ger_con_file *ger_con_next;
};

extern struct ger_con_file *MALLOC_GER_CON();

#endif
