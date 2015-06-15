#ifndef ap_file_system_ap_file_h
#define ap_file_system_ap_file_h
extern int ap_open(char *path, int flags);
extern int ap_mount(void *m_info, char *file_system, char *path);
extern int ap_file_thread_init();
extern int ap_close(int fd);
extern int ap_mkdir(char *path, unsigned long mode);
extern int ap_unlik(char *path);
extern int ap_link(char *l_path, char *t_path);
extern int ap_rmdir(char *path);
extern int ap_chdir(char *path);
extern ssize_t ap_read(int fd, void *buf, size_t len);
extern ssize_t ap_write(int fd, void *buf, size_t len);
extern off_t ap_lseek(int fd, off_t ops, int origin);
#endif