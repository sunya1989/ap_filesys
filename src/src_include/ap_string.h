/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_ap_string_h
#define ap_file_system_ap_string_h
#include <stdio.h>
extern char **cut_str(const char *s, char d, size_t len);
extern char *path_name_cat(char *dest, const char *src, size_t len, char *d);
extern char *path_names_cat(char *dest, const char **src, int num, char *d);
extern char *path_cpy_add_root(char * dest, const char *src, size_t len);
extern char *string_copy(const char *c, char *p);

static inline int strstarts(const char *str, const char *prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}
#endif
