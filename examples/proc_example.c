//
//  main.c
//  proc_user1
//
//  Created by sunya on 15/9/1.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ap_fsys/ap_types.h>
#include <ap_fsys/ap_file.h>
#include <ap_fsys/stdio_agent.h>
#include <ap_fsys/proc_fs.h>
int main(int argc, const char * argv[]) {
    /*initiate ger and proc file system*/
    ap_file_thread_init();
    init_fs_ger();
    init_proc_fs();
    int mount_s;
    struct std_age_dir *root_dir;
    /*crate test directory*/
    int mk_s = mkdir("/tmp/ap_test_proc", S_IRWXU | S_IRWXG | S_IRWXO);
    if (mk_s == -1) {
        perror("mkdir failed\n");
        exit(1);
    }
    
    /*fill the test-file*/
    char pre_write_buf[] = "test_proc\n";
    int fd = open("/tmp/ap_test_proc/proc_test", O_CREAT | O_RDWR);
    write(fd, pre_write_buf, sizeof(pre_write_buf));
    chmod("/tmp/ap_test_proc/proc_test", S_IRWXU | S_IRWXG | S_IRWXO);
    close(fd);
    
    /*import /tmp/ap_test_proc into this process as root directory "/"*/
    root_dir = MALLOC_STD_AGE_DIR("/tmp/ap_test_proc");
    root_dir->stem.stem_name = "/";
    root_dir->stem.stem_mode = 0777;
    
    /*mount ger file system*/
    mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
    if (mount_s == -1)
        perror("mount failed\n");
    
    /*mount proc file system*/
    struct proc_mount_info m_info;
    m_info.typ = SYSTEM_V;
    m_info.sever_name = "proc_user1";
    mount_s = ap_mount(&m_info, PROC_FILE_FS, "/procs");
    
    if (mount_s == -1)
        perror("mount failed\n");
    
    /*wait other process to acess*/
    sleep(1000000000);
    exit(0);
}