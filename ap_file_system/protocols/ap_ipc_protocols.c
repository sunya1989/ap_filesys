//
//  ap_ipc_protocols.c
//  ap_editor
//
//  Created by HU XUKAI on 15/8/5.
//  Copyright (c) 2015年 HU XUKAI.<goingonhxk@gmail.com>
//

#include <stdio.h>
#include <ipc_protocols.h>
#include <pthread.h>
#include "system_v.h"
struct ap_ipc_operations *ap_ipc_pro_ops[TYP_NUM] = {
    &system_v_ops,
};
pthread_mutex_t c_port_lock = PTHREAD_MUTEX_INITIALIZER;
struct ap_ipc_port *ipc_c_ports[TYP_NUM];

struct ap_ipc_port *get_ipc_c_port(enum connet_typ typ, const char *path)
{
    pthread_mutex_lock(&c_port_lock);
    if (ipc_c_ports[typ] != NULL) {
        pthread_mutex_unlock(&c_port_lock);
        return ipc_c_ports[typ];
    }
    struct ap_ipc_port *port = ap_ipc_pro_ops[typ]->ipc_get_port(path, 0772);
    if (port == NULL) {
        pthread_mutex_unlock(&c_port_lock);
        return NULL;
    }
    ipc_c_ports[typ] = port;
    pthread_mutex_unlock(&c_port_lock);
    return port;
}
