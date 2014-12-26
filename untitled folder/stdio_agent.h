//
//  stdio_agent.h
//  ap_file_system
//
//  Created by sunya on 14/12/26.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#ifndef __ap_file_system__stdio_agent__
#define __ap_file_system__stdio_agent__

#include <stdio.h>
#include "ger_fs.h"

enum file_state{
    g_fileno = 0,
    g_stream,
};

struct std_age{
    struct ger_stem stem;
    char *target_file;
    int fd;
    FILE *fs;
    enum file_state state;
};

static inline struct std_age *MALLOC_STD_AGE(char *tarf, enum file_state state)
{
    struct std_age *sa;
    sa = malloc(sizeof(*sa));
    if (sa == NULL) {
        perror("malloc std_age failed\n");
        exit(1);
    }
    
    STEM_INIT(&sa->stem);
    
    sa->target_file = tarf;
    sa->state = state;
    return sa;
}

#endif /* defined(__ap_file_system__stdio_agent__) */

