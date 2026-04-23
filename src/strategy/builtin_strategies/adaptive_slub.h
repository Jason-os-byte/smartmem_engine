/* 自适应SLUB策略 */

#ifndef _ADAPTIVE_SLUB_H
#define _ADAPTIVE_SLUB_H

#include "strategy.h"

void adaptive_slub_exit(void);
int adaptive_slub_init(void);
void adaptive_slub_reset_stats(void);
void adaptive_slub_get_stats(u64 *alloc, u64 *free, u64 *hit, u64 *miss, u64 *tune);
struct slub_strategy *adaptive_slub_strategy_create(void);
void adaptive_slub_strategy_destroy(struct slub_strategy *s);

#endif /* _ADAPTIVE_SLUB_H */