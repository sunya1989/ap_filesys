//
//  ap_string.h
//  ap_editor
//
//  Created by sunya on 15/8/3.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_ap_string_h
#define ap_file_system_ap_string_h
#include <stdio.h>
extern char **cut_str(const char *s, char d, size_t len);
extern char *path_name_cat(char *dest, const char *src, size_t len, char *d);
extern char *path_names_cat(char *dest, const char **src, int num, char *d);
#endif
