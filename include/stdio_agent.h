//
//  stdio_agent.h
//  ap_file_system
//
//  Created by HU XUKAI on 14/12/26.
//  Copyright (c) 2014年 HU XUKAI.
//

#ifndef __ap_file_system__stdio_agent__
#define __ap_file_system__stdio_agent__

#include <stdio.h>
#include <unistd.h>
#include <ap_fsys/ger_fs.h>

enum file_state{
    g_fileno = 0,
    g_stream,
};

struct std_age{
    struct ger_stem_node stem;
    char *target_file;
    int fd;
    FILE *fs;
    enum file_state state;
};

struct std_age_dir{
    const char *target_dir;
    struct ger_stem_node stem;
};

extern void STD_AGE_INIT(struct std_age *age, char *tarf, enum file_state state);
extern void STD_AGE_DIR_INIT(struct std_age_dir *age_dir, const char *tard);

static inline struct std_age *MALLOC_STD_AGE(char *tarf, enum file_state state)
{
    struct std_age *sa;
    sa = malloc(sizeof(*sa));
    if (sa == NULL) {
        perror("malloc std_age failed\n");
        exit(1);
    }
    
    STD_AGE_INIT(sa,tarf,state);
    return sa;
}

static inline struct std_age_dir *MALLOC_STD_AGE_DIR(const char *tard)
{
    struct std_age_dir *sa_dir;
    sa_dir = malloc(sizeof(*sa_dir));
    if (sa_dir == NULL) {
        perror("malloc std_age_dir failed\n");
        exit(1);
    }
    
    STD_AGE_DIR_INIT(sa_dir, tard);
    return sa_dir;
}

static inline void STD_AGE_FREE(struct std_age *sa)
{
    if (sa->fs != NULL) {
        fclose(sa->fs);
    }else if(sa->fd != -1){
        close(sa->fd);
    }
    COUNTER_FREE(&sa->stem.stem_inuse);
    pthread_mutex_destroy(&sa->stem.ch_lock);
    free(sa);
}

static inline void STD_AGE_DIR_FREE(struct std_age_dir *sa_dir)
{
    COUNTER_FREE(&sa_dir->stem.stem_inuse);
    pthread_mutex_destroy(&sa_dir->stem.ch_lock);
    free(sa_dir);
}

#endif /* defined(__ap_file_system__stdio_agent__) */

