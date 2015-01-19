//
//  ap_file_thread.c
//  ap_file_system
//
//  Created by sunya on 14/11/18.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include <stdlib.h>
#include "ap_pthread.h"
#include "ap_file.h"

pthread_once_t thread_once = PTHREAD_ONCE_INIT;

static void thread_file_destory(void *my_data)
{
    free(my_data);
}

static void thread_init()
{
    int err = pthread_key_create(&file_thread_key, thread_file_destory);
    if (err) {
        perror("ap_thread_init failed\n");
        exit(1);
    }
}

int ap_file_thread_init()
{
    struct ap_file_pthread *file_pthr;
    
    pthread_once(&thread_once, thread_init);
    file_pthr = malloc(sizeof(*file_pthr));
    if (file_pthr == NULL) {
        perror("ap_thread_init failed\n");
        exit(1);
    }
    AP_FILE_THREAD_INIT(file_pthr);
    
    int set = pthread_setspecific(file_thread_key, file_pthr);
    if (set) {
        perror("ap_thread set failes\n");
        exit(1);
    }
    
    return 0;
}