#ifndef ap_file_system_ap_file_h
#define ap_file_system_ap_file_h

extern int ap_open(char *path, int flags);
extern int ap_mount(void *mount_info, char *file_system, char *path);
extern int ap_file_thread_init();

