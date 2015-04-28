//
//  Header.h
//  ap_editor
//
//  Created by sunya on 15/4/23.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_ap_ipc_h
#define ap_file_system_ap_ipc_h

#define AP_IPC_PATH_CIL "/tmp/ap_procs/%ld_0"
#define AP_IPC_PATH_SER "/tmp/ap_procs/%ld_1"
#define AP_IPC_PATH_LEN 512


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

#endif
