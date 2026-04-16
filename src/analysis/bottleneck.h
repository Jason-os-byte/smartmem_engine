/* 瓶颈分析 */

#ifndef _SMARTMEM_BOTTLENECK_H
#define _SMARTMEM_BOTTLENECK_H

#include "analysis.h"

/* 瓶颈分析接口 */
int bottleneck_init(void);
void bottleneck_exit(void);
int bottleneck_update(void);

#endif /* _SMARTMEM_BOTTLENECK_H */