//
//  proc_fs.h
//  ap_editor
//
//  Created by HU XUKAI on 15/3/27.
//  Copyright (c) 2015年 HU XUKAI.<goingonhxk@gmail.com>
//

#ifndef ap_file_system_proc_fs_h
#define ap_file_system_proc_fs_h
#define PROC_FILE_FS "proc"
#define AT '@'

struct proc_mount_info{
    enum connet_typ typ;
    const char *sever_name;
};
extern int init_proc_fs();

#endif
