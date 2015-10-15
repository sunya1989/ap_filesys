/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */

#ifndef ap_file_system_convert_h
#define ap_file_system_convert_h

#include <stdio.h>
extern char *itoa(int num, char*str, int radix);
extern char *ultoa(unsigned long value, char *string, int radix);
void *collect_items(void **items, size_t buf_len, size_t list[], int lis_len);

#endif /* defined(__ap_editor__convert__) */
