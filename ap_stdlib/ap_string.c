/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <string.h>
#include <stdio.h>
#include <envelop.h>
#include <ap_string.h>
char **cut_str(const char *s, char d, size_t len)
{
    char *s_p,*head;
    size_t p_l = strlen(s);
    char *tm_path = Malloc_z(p_l+1);
    strncpy(tm_path, s, p_l);
    head = tm_path;
    char **cut = Malloc_z(sizeof(char*)*len);
    int i = 0;
    
    while (i < len) {
        s_p = strchr(head, d);
        size_t str_len;
        if (s_p != NULL)
            *s_p = '\0';
        
        str_len = strlen(head);
        char *item = Malloc_z(str_len+1);
        strncpy(item, head, str_len);
        *(item + str_len) = '\0';
        cut[i] = item;
        head = ++s_p;
        i++;
    }
    free(tm_path);
    return cut;
}

char *path_name_cat(char *dest, const char *src, size_t len, char *d)
{
    if (strlen(dest)!=0) 
        strncat(dest, d, 1);
    
    strncat(dest, src, len);
    return dest;
}

char *path_names_cat(char *dest, const char **src, int num, char *d)
{
    size_t strl = 0;
    size_t strl1 = 0;
    for (int i = 0; i < num; i++) {
        strl1 = strlen(src[i]);
        strl += strl1;
        path_name_cat(dest, src[i], strl, d);
    }
    return dest + strl + (num-1);
}

char *path_cpy_add_root(char *dest, const char *src, size_t len)
{
    char *cp = dest;
    if (*src != '/') {
        *cp = '/';
        cp++;
    }
    strncpy(cp, src, len);
    return dest;
}

char *string_copy(const char *c, char *p)
{
    size_t strl = strlen(c);
    char *r = Malloc_z(strl + 1);
    strncpy(r, c, strl);
    return r;
}




