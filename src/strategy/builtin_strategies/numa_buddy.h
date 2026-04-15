/* NUMA感知Buddy策略 */

#ifndef _NUMA_BUDDY_H
#define _NUMA_BUDDY_H

#include "strategy.h"

/* NUMA Buddy 策略接口 */
struct buddy_strategy *numa_buddy_strategy_create(void);
void numa_buddy_strategy_destroy(struct buddy_strategy *s);

#endif /* _NUMA_BUDDY_H */