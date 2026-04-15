/* 多代LRU策略 */
#ifndef _MULTI_GEN_LRU_H
#define _MULTI_GEN_LRU_H

#include "strategy.h"

/* 多代LRU策略接口 */
struct lru_strategy *multi_gen_lru_strategy_create(void);
void multi_gen_lru_strategy_destroy(struct lru_strategy *s);

#endif /* _MULTI_GEN_LRU_H */