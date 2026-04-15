/* NUMA负载均衡策略 */
#ifndef _NUMA_BALANCE_H
#define _NUMA_BALANCE_H

#include "strategy.h"

/* NUMA负载均衡策略接口 */
struct numa_strategy *numa_balance_strategy_create(void);
void numa_balance_strategy_destroy(struct numa_strategy *s);

#endif /* _NUMA_BALANCE_H */