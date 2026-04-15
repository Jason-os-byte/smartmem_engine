/* 自适应SLUB策略 */

#ifndef _ADAPTIVE_SLUB_H
#define _ADAPTIVE_SLUB_H

#include "strategy.h"

struct slub_strategy *adaptive_slub_strategy_create(void);
void adaptive_slub_strategy_destroy(struct slub_strategy *s);

#endif /* _ADAPTIVE_SLUB_H */