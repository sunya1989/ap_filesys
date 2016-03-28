#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include"m_lib_include.h"
#include"module_layout.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunya");
MODULE_DESCRIPTION("hello world");

static int __init task01_hello_world(void)
{

	const char *buff = "hello world\n";
	puts(buff);
	return 0;
}

static void __exit task01_exit(void)
{
    puts("fuck i'm leaving\n");
}

module_init(task01_hello_world);
module_exit(task01_exit);
