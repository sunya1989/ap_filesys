//
//  ap_string.h
//  ap_editor
//
//  Created by HU XUKAI on 15/8/3.
//  Copyright (c) 2014年 HU XUKAI.<goingonhxk@gmail.com>
//

#ifndef ap_file_system_ap_string_h
#define ap_file_system_ap_string_h
#include <stdio.h>
extern char **cut_str(const char *s, char d, size_t len);
extern char *path_name_cat(char *dest, const char *src, size_t len, char *d);
extern char *path_names_cat(char *dest, const char **src, int num, char *d);
extern char *path_cpy_add_root(char * dest, const char *src, size_t len);
extern char *string_copy(const char *c, char *p);
#endif
