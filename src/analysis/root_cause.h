/* 根因分析 */

#ifndef _SMARTMEM_ROOT_CAUSE_H
#define _SMARTMEM_ROOT_CAUSE_H

#include "analysis.h"

/* 根因分析接口 */
int root_cause_init(void);
void root_cause_exit(void);
int root_cause_update(void);

#endif /* _SMARTMEM_ROOT_CAUSE_H */