/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */

#ifndef ap_file_system_ap_ipc_h
#define ap_file_system_ap_ipc_h
#include <list.h>
#include <fcntl.h>
#include <errno.h>
#include <ap_hash.h>
#include <bag.h>
#include <ap_types.h>

#define AP_PROC_FILE "/tmp/ap_procs"
#define AP_PROC_DISC_F "/tmp/ap_proc_disc"
#define AP_IPC_PATH_LEN 512
#define AP_IPC_RECODE_LEN 100

/* our record locking macros */
#define	read_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define	readw_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define	write_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define	writew_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#define	un_lock(fd, offset, whence, len) \
lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)
#define	is_read_lockable(fd, offset, whence, len) \
lock_test(fd, F_RDLCK, offset, whence, len)
#define	is_write_lockable(fd, offset, whence, len) \
lock_test(fd, F_WRLCK, offset, whence, len)
/* end unpipch */

#define	Read_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define	Readw_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define	Write_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define	Writew_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#define	Un_lock(fd, offset, whence, len) \
Lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)
#define	Is_read_lockable(fd, offset, whence, len) \
Lock_test(fd, F_RDLCK, offset, whence, len)
#define	Is_write_lockable(fd, offset, whence, len) \
Lock_test(fd, F_WRLCK, offset, whence, len)

int lock_reg(int, int, int, off_t, int, off_t);
void Lock_reg(int, int, int, off_t, int, off_t);

#define AP_C_T 0
#define AP_IPC_PID 1
#define AP_IPC_KEY 2
#define AP_IPC_MSGID 3
#define AP_IPC_RECODE_NUM 4
#define AP_IPC_ONE_MSG_UNION -1
struct ap_ipc_operations;


extern  struct ap_ipc_port *ipc_c_ports[IPC_TYP_NUM];
struct package_hint{
    void *p_hint;
    void (*p_release)(struct package_hint *);
};

BAG_DEFINE_FREE(PAKAGE_HINT_FREE);
static inline void PAKAGE_HINT_FREE(struct package_hint *h)
{
    if (h->p_release == NULL) {
        return;
    }
    h->p_release(h);
}


struct ap_ipc_info_head;

typedef struct ap_ipc_port_pair{
    struct ap_ipc_port *local_port;
    struct ap_ipc_port *far_port;
}ppair_t;

struct ap_ipc_port{
    struct ap_ipc_operations *ipc_ops;
    struct ap_ipc_port *local_port;
    const char *port_dis;
    void *x_object;
};

typedef enum permission_ide{
    user = 0,
    gruop,
    other,
}pmide_t;

static inline struct ap_ipc_port *MALLOC_IPC_PORT()
{
    struct ap_ipc_port *port = Malloc_z(sizeof(*port));
    return port;
}

static inline void IPC_PORT_FREE(struct ap_ipc_port *port)
{
    free((void *)port->port_dis);
    free(port);
}

struct ap_ipc_info{
    struct ap_ipc_info_head *info_h;
    ppair_t *port_pair;
    pmide_t pm_id;
    int disc;
    enum connet_typ c_t;                /*connect type of the severend*/
    struct ap_hash *inde_hash_table;
};

struct ap_ipc_info_head{
    struct ap_ipc_port *s_port;         /*sever port*/
    const char *sever_name; 
};

static inline struct ap_ipc_info_head *MALLOC_IPC_INFO_HEAD()
{
    struct ap_ipc_info_head *h_info;
    h_info = Malloc_z(sizeof(*h_info));
    return h_info;
}

static inline void IPC_INFO_HEAD_FREE(struct ap_ipc_info_head *h_info)
{
    free(h_info);
}

static inline struct ap_ipc_info *MALLOC_IPC_INFO()
{
    struct ap_ipc_info *info = Malloc_z(sizeof(*info));
    info->inde_hash_table = MALLOC_IPC_HASH(AP_IPC_INODE_HASH_LEN);
    return info;
}

struct ap_ipc_operations{
    struct ap_ipc_port *(*ipc_get_port)(const char *, mode_t);
    ppair_t *(*ipc_connect)(struct ap_ipc_port *, const char *);
    ssize_t (*ipc_send)
    (struct ap_ipc_port *, void *, size_t, struct package_hint *);
    ssize_t (*ipc_recv)
    (struct ap_ipc_port *, void **, size_t, struct package_hint *, struct ap_ipc_port *);
    int (*ipc_lisen)(struct ap_ipc_port *);
    int (*ipc_close)(struct ap_ipc_port *);
    int (*ipc_probe)(struct ap_ipc_port *);
};

extern struct ap_ipc_port *get_ipc_c_port(enum connet_typ typ, const char *path);

#endif
