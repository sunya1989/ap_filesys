//
//  lib_export.c
//  ap_tester
//

#include <stdio.h>
#include <lib_export.h>
#include <export.h>

void ex_puts(char *s)
{
	puts(s);
}

EXPORT_SYMBOL(ex_puts);