/* 热点识别 */
#ifndef _SMARTMEM_HOTSPOT_H
#define _SMARTMEM_HOTSPOT_H

#include "analysis.h"

/* 热点识别接口 */
int hotspot_init(void);
void hotspot_exit(void);
int hotspot_update(void);

#endif /* _SMARTMEM_HOTSPOT_H */