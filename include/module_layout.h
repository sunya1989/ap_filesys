//
//  module_layout.h
//  ap_tester
//
//  Created by sunya on 16/2/14.
//  Copyright © 2016年 sunya. All rights reserved.
//

#ifndef module_layout_h
#define module_layout_h

struct module kernel_module;
struct module_indic{
	void *module;
	void *init;
	void *exit;
	void *name
};

struct module_indic __ap_module_indic __attribute__((section(".ap_module_indicator"))) = {
	.module = &kernel_module,
	.init = &kernel_module.init,
	.exit = &kernel_module.exit,
	.name = &kernel_module.name,
};

#endif /* module_layout_h */
