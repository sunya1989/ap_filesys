//
//  ap_file_thread.c
//  ap_file_system
//
//  Created by HU XUKAI on 14/11/18.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//

#include <stdlib.h>
#include <ap_pthread.h>
#include <ap_file.h>
#include <bag.h>

pthread_key_t file_thread_key;

pthread_once_t thread_once = PTHREAD_ONCE_INIT;
static void thread_file_destory(void *my_data)
{
    free(my_data);
}

static void ap_at_exit()
{
    BAG_EXCUTE(&global_bag);
}

static void thread_init()
{
    int err = pthread_key_create(&file_thread_key, thread_file_destory);
    if (err) {
        perror("ap_thread_init failed\n");
        exit(1);
    }
    
    atexit(ap_at_exit);
}

int ap_file_thread_init()
{
    struct ap_file_pthread *file_pthr;
    
    pthread_once(&thread_once, thread_init);
    file_pthr = Malloc_z(sizeof(*file_pthr));
    
    AP_FILE_THREAD_INIT(file_pthr);
    
    int set = pthread_setspecific(file_thread_key, file_pthr);
    if (set) {
        perror("ap_thread set failes\n");
        exit(1);
    }
    return 0;
}