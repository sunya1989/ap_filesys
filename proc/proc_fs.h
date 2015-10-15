/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
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
