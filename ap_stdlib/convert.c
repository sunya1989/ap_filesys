//
//  convert.c
//  ap_editor
//
//  Created by sunya on 15/4/30.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include "convert.h"
#include "envelop.h"
#include <stdlib.h>
#include <errno.h>
char *ultoa(unsigned long value, char *string, int radix)
{
    char tmp[33];
    char *tp = tmp;
    long i;
    unsigned long v = value;
    char *sp;
    
    if (radix > 36 || radix <= 1)
    {
        errno = EDOM;
        return 0;
    }
    
    while (v || tp == tmp)
    {
        i = v % radix;
        v = v / radix;
        if (i < 10)
            *tp++ = i+'0';
        else
            *tp++ = i + 'a' - 10;
    }
    if (string == NULL)
        string = (char *)malloc((tp-tmp)+1);
    sp = string;
    
    while (tp > tmp)
        *sp++ = *--tp;
    *sp = 0;
    return string;
}

char *itoa(int num, char*str, int radix)
{
    char index[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned unum;/*中间变量*/
    int i = 0,j,k;
    /*确定unum的值*/
    if(radix==10 && num<0)/*十进制负数*/
    {
        unum=(unsigned)-num;
        str[i++]='-';
    }
    else unum = (unsigned)num;/*其他情况*/
    /*转换*/
    do{
        str[i++] = index[unum%(unsigned)radix];
        unum /= radix;
    }while(unum);
    str[i] = '\0';
    /*逆序*/
    if(str[0] == '-')k = 1;/*十进制负数*/
    else k = 0;
    char temp;
    for(j = k; j<=(i-1)/2; j++)
    {
        temp = str[j];
        str[j] = str[i-1+k-j];
        str[i-1+k-j] = temp;
    }
    return str;
}

void *collect_items(void **items, size_t buf_len, size_t list[], int lis_len)
{
    char *buf = Mallocz(buf_len);
    char *cp = buf;
    for (int i = 0; i<lis_len; i++) {
        memcpy(cp, items[i], list[i]);
        cp += list[i];
    }
    return buf;
}


