/* 自动调优 */
#ifndef _SMARTMEM_AUTO_TUNE_H
#define _SMARTMEM_AUTO_TUNE_H

#include "optimization.h"

/* 自动调优接口 */
int auto_tune_init(void);
void auto_tune_exit(void);
int auto_tune_start(void);
int auto_tune_stop(void);

#endif /* _SMARTMEM_AUTO_TUNE_H */