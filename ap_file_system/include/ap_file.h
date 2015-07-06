#ifndef ap_file_system_ap_file_h
#define ap_file_system_ap_file_h
extern int ap_open(const char *path, int flags);
extern int ap_mount(void *m_info, char *file_system, const char *path);
extern int ap_mount2(char *file_system, const char *path);
extern int ap_file_thread_init();
extern int ap_close(int fd);
extern int ap_mkdir(char *path, unsigned long mode);
extern int ap_unlink(const char *path);
extern int ap_link(const char *l_path, const char *t_path);
extern int ap_rmdir(const char *path);
extern int ap_chdir(const char *path);
extern int ap_unmount(const char *path);
extern ssize_t ap_read(int fd, void *buf, size_t len);
extern ssize_t ap_write(int fd, void *buf, size_t len);
extern off_t ap_lseek(int fd, off_t ops, int origin);
#endif