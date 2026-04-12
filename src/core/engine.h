/* core/engine.h - 引擎核心模块 */

#ifndef _SMARTMEM_ENGINE_H
#define _SMARTMEM_ENGINE_H

#include "smartmem.h"

/**
 * 引擎核心结构
 */
struct smartmem_engine {
    /* 配置 */
    void *config;

    /* 统计 */
    void *stats;

    /* 模块状态 */
    bool initialized;
};

/* 引擎接口 */
int smartmem_engine_init(void);
void smartmem_engine_exit(void);
int smartmem_engine_enable(void);
int smartmem_engine_disable(void);

#endif /* _SMARTMEM_ENGINE_H */