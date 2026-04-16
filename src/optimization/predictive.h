/* 预测模型 */

#ifndef _SMARTMEM_PREDICTIVE_H
#define _SMARTMEM_PREDICTIVE_H

#include "smartmem.h"

/* 预测模型接口 */
int predictive_init(void);
void predictive_exit(void);
int predictive_start(void);
int predictive_stop(void);

#endif /* _SMARTMEM_PREDICTIVE_H */