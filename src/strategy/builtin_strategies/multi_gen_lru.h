/* multi_gen_lru.h - 多代 LRU 策略 */

#ifndef _MULTI_GEN_LRU_H
#define _MULTI_GEN_LRU_H

#include "strategy.h"

/* 接口 */
struct lru_strategy *multi_gen_lru_strategy_create(void);
void multi_gen_lru_strategy_destroy(struct lru_strategy *s);

void mglru_get_stats(u64 *young_accessed, u64 *old_accessed,
		     u64 *promote, u64 *demote, u64 *evict);
void mglru_reset_stats(void);

/* init/exit 接口 */
int multi_gen_lru_init(void);
void multi_gen_lru_exit(void);

#endif /* _MULTI_GEN_LRU_H */
