//
//  convert.h
//  ap_editor
//
//  Created by sunya on 15/4/30.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_convert_h
#define ap_file_system_convert_h

#include <stdio.h>
extern char *itoa(int num, char*str, int radix);
extern char *ultoa(unsigned long value, char *string, int radix);
char *collect_items(const char **items, size_t buf_len, size_t list[], int lis_len);

#endif /* defined(__ap_editor__convert__) */
