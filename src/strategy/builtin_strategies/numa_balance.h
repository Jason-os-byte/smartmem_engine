/* numa_balance.h - NUMA 负载均衡策略 */

#ifndef _NUMA_BALANCE_H
#define _NUMA_BALANCE_H

#include "../strategy.h"

/* 创建/销毁接口 */
struct numa_strategy *numa_balance_strategy_create(void);
void numa_balance_strategy_destroy(struct numa_strategy *s);

/* init/exit 接口 */
int numa_balance_init(void);
void numa_balance_exit(void);

/* 统计接口 */
void numa_balance_get_stats(u64 *migrations, u64 *imbalance_detect, u64 *balance);
void numa_balance_reset_stats(void);

#endif /* _NUMA_BALANCE_H */
