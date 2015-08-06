//
//  ap_ipc_protocols.c
//  ap_editor
//
//  Created by sunya on 15/8/5.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <ipc_protocols.h>
#include "system_v.h"
struct ap_ipc_operations *ap_ipc_pro_ops[TYP_NUM] = {
    &system_v_ops,
};