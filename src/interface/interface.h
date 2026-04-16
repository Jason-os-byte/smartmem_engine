/* 接口层 */
#ifndef _SMARTMEM_INTERFACE_H
#define _SMARTMEM_INTERFACE_H

#include "smartmem.h"

/* 接口层使能 */
struct smartmem_interface {
    bool proc_enabled;
    bool debugfs_enabled;
    bool netlink_enabled;
    bool initialized;
};

/* 接口层接口 */
int smartmem_interface_init(void);
void smartmem_interface_exit(void);

#endif /* _SMARTMEM_INTERFACE_H */