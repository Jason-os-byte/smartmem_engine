/* strategy.c - 策略引擎实现 */

#include "strategy.h"

extern int numa_buddy_init(void);
extern void numa_buddy_exit(void);
extern int adaptive_slub_init(void);
extern void adaptive_slub_exit(void);
extern int multi_gen_lru_init(void);
extern void multi_gen_lru_exit(void);
extern int numa_balance_init(void);
extern void numa_balance_exit(void);

/* ============ Buddy 策略管理 ============ */

static LIST_HEAD(buddy_strategy_list);
static struct buddy_strategy *current_buddy_strategy = NULL;
static DEFINE_SPINLOCK(buddy_strategy_lock);

int buddy_strategy_register(struct buddy_strategy *s)
{
	unsigned long flags;
	struct buddy_strategy *tmp;

	if (!s || !s->name[0])
		return -EINVAL;

	spin_lock_irqsave(&buddy_strategy_lock, flags);
	list_for_each_entry(tmp, &buddy_strategy_list, list) {
		if (strcmp(tmp->name, s->name) == 0) {
			spin_unlock_irqrestore(&buddy_strategy_lock, flags);
			pr_err("smartmem: buddy strategy '%s' already exists\n", s->name);
			return -EEXIST;
		}
	}
	list_add_tail(&s->list, &buddy_strategy_list);
	if (!current_buddy_strategy) {
		current_buddy_strategy = s;
		s->enabled = true;
	}
	spin_unlock_irqrestore(&buddy_strategy_lock, flags);

	pr_info("smartmem: registered buddy strategy: %s\n", s->name);
	return 0;
}

int buddy_strategy_unregister(struct buddy_strategy *s)
{
	unsigned long flags;

	if (!s)
		return -EINVAL;

	spin_lock_irqsave(&buddy_strategy_lock, flags);
	if (current_buddy_strategy == s)
		current_buddy_strategy = NULL;
	list_del(&s->list);
	s->enabled = false;
	spin_unlock_irqrestore(&buddy_strategy_lock, flags);

	pr_info("smartmem: unregistered buddy strategy: %s\n", s->name);
	return 0;
}

struct buddy_strategy *buddy_strategy_get_current(void)
{
	struct buddy_strategy *s;
	unsigned long flags;

	spin_lock_irqsave(&buddy_strategy_lock, flags);
	s = current_buddy_strategy;
	spin_unlock_irqrestore(&buddy_strategy_lock, flags);

	return s;
}

int buddy_strategy_switch(const char *name)
{
	struct buddy_strategy *s, *target = NULL;
	unsigned long flags;

	if (!name)
		return -EINVAL;

	spin_lock_irqsave(&buddy_strategy_lock, flags);
	list_for_each_entry(s, &buddy_strategy_list, list) {
		if (strcmp(s->name, name) == 0) {
			target = s;
			break;
		}
	}
	if (!target) {
		spin_unlock_irqrestore(&buddy_strategy_lock, flags);
		pr_err("smartmem: buddy strategy '%s' not found\n", name);
		return -ENOENT;
	}
	if (current_buddy_strategy)
		current_buddy_strategy->enabled = false;
	current_buddy_strategy = target;
	target->enabled = true;
	spin_unlock_irqrestore(&buddy_strategy_lock, flags);

	pr_info("smartmem: switched to buddy strategy: %s\n", name);
	return 0;
}

/* ============ SLUB 策略管理 ============ */

static LIST_HEAD(slub_strategy_list);
static struct slub_strategy *current_slub_strategy = NULL;
static DEFINE_SPINLOCK(slub_strategy_lock);

int slub_strategy_register(struct slub_strategy *s)
{
	unsigned long flags;

	if (!s || !s->name[0])
		return -EINVAL;

	spin_lock_irqsave(&slub_strategy_lock, flags);
	list_add_tail(&s->list, &slub_strategy_list);
	if (!current_slub_strategy) {
		current_slub_strategy = s;
		s->enabled = true;
	}
	spin_unlock_irqrestore(&slub_strategy_lock, flags);

	pr_info("smartmem: registered slub strategy: %s\n", s->name);
	return 0;
}

int slub_strategy_unregister(struct slub_strategy *s)
{
	unsigned long flags;

	if (!s)
		return -EINVAL;

	spin_lock_irqsave(&slub_strategy_lock, flags);
	if (current_slub_strategy == s)
		current_slub_strategy = NULL;
	list_del(&s->list);
	s->enabled = false;
	spin_unlock_irqrestore(&slub_strategy_lock, flags);

	pr_info("smartmem: unregistered slub strategy: %s\n", s->name);
	return 0;
}

struct slub_strategy *slub_strategy_get_current(void)
{
	struct slub_strategy *s;
	unsigned long flags;

	spin_lock_irqsave(&slub_strategy_lock, flags);
	s = current_slub_strategy;
	spin_unlock_irqrestore(&slub_strategy_lock, flags);

	return s;
}

int slub_strategy_switch(const char *name)
{
	struct slub_strategy *s, *target = NULL;
	unsigned long flags;

	if (!name)
		return -EINVAL;

	spin_lock_irqsave(&slub_strategy_lock, flags);
	list_for_each_entry(s, &slub_strategy_list, list) {
		if (strcmp(s->name, name) == 0) {
			target = s;
			break;
		}
	}
	if (!target) {
		spin_unlock_irqrestore(&slub_strategy_lock, flags);
		pr_err("smartmem: slub strategy '%s' not found\n", name);
		return -ENOENT;
	}
	if (current_slub_strategy)
		current_slub_strategy->enabled = false;
	current_slub_strategy = target;
	target->enabled = true;
	spin_unlock_irqrestore(&slub_strategy_lock, flags);

	pr_info("smartmem: switched to slub strategy: %s\n", name);
	return 0;
}

/* ============ LRU 策略管理 ============ */

static LIST_HEAD(lru_strategy_list);
static struct lru_strategy *current_lru_strategy = NULL;
static DEFINE_SPINLOCK(lru_strategy_lock);

int lru_strategy_register(struct lru_strategy *s)
{
	unsigned long flags;

	if (!s || !s->name[0])
		return -EINVAL;

	spin_lock_irqsave(&lru_strategy_lock, flags);
	list_add_tail(&s->list, &lru_strategy_list);
	if (!current_lru_strategy) {
		current_lru_strategy = s;
		s->enabled = true;
	}
	spin_unlock_irqrestore(&lru_strategy_lock, flags);

	pr_info("smartmem: registered lru strategy: %s\n", s->name);
	return 0;
}

int lru_strategy_unregister(struct lru_strategy *s)
{
	unsigned long flags;

	if (!s)
		return -EINVAL;

	spin_lock_irqsave(&lru_strategy_lock, flags);
	if (current_lru_strategy == s)
		current_lru_strategy = NULL;
	list_del(&s->list);
	s->enabled = false;
	spin_unlock_irqrestore(&lru_strategy_lock, flags);

	pr_info("smartmem: unregistered lru strategy: %s\n", s->name);
	return 0;
}

struct lru_strategy *lru_strategy_get_current(void)
{
	struct lru_strategy *s;
	unsigned long flags;

	spin_lock_irqsave(&lru_strategy_lock, flags);
	s = current_lru_strategy;
	spin_unlock_irqrestore(&lru_strategy_lock, flags);

	return s;
}

int lru_strategy_switch(const char *name)
{
	struct lru_strategy *s, *target = NULL;
	unsigned long flags;

	if (!name)
		return -EINVAL;

	spin_lock_irqsave(&lru_strategy_lock, flags);
	list_for_each_entry(s, &lru_strategy_list, list) {
		if (strcmp(s->name, name) == 0) {
			target = s;
			break;
		}
	}
	if (!target) {
		spin_unlock_irqrestore(&lru_strategy_lock, flags);
		pr_err("smartmem: lru strategy '%s' not found\n", name);
		return -ENOENT;
	}
	if (current_lru_strategy)
		current_lru_strategy->enabled = false;
	current_lru_strategy = target;
	target->enabled = true;
	spin_unlock_irqrestore(&lru_strategy_lock, flags);

	pr_info("smartmem: switched to lru strategy: %s\n", name);
	return 0;
}

/* ============ NUMA 策略管理 ============ */

static LIST_HEAD(numa_strategy_list);
static struct numa_strategy *current_numa_strategy = NULL;
static DEFINE_SPINLOCK(numa_strategy_lock);

int numa_strategy_register(struct numa_strategy *s)
{
	unsigned long flags;

	if (!s || !s->name[0])
		return -EINVAL;

	spin_lock_irqsave(&numa_strategy_lock, flags);
	list_add_tail(&s->list, &numa_strategy_list);
	if (!current_numa_strategy) {
		current_numa_strategy = s;
		s->enabled = true;
	}
	spin_unlock_irqrestore(&numa_strategy_lock, flags);

	pr_info("smartmem: registered numa strategy: %s\n", s->name);
	return 0;
}

int numa_strategy_unregister(struct numa_strategy *s)
{
	unsigned long flags;

	if (!s)
		return -EINVAL;

	spin_lock_irqsave(&numa_strategy_lock, flags);
	if (current_numa_strategy == s)
		current_numa_strategy = NULL;
	list_del(&s->list);
	s->enabled = false;
	spin_unlock_irqrestore(&numa_strategy_lock, flags);

	pr_info("smartmem: unregistered numa strategy: %s\n", s->name);
	return 0;
}

struct numa_strategy *numa_strategy_get_current(void)
{
	struct numa_strategy *s;
	unsigned long flags;

	spin_lock_irqsave(&numa_strategy_lock, flags);
	s = current_numa_strategy;
	spin_unlock_irqrestore(&numa_strategy_lock, flags);

	return s;
}

int numa_strategy_switch(const char *name)
{
	struct numa_strategy *s, *target = NULL;
	unsigned long flags;

	if (!name)
		return -EINVAL;

	spin_lock_irqsave(&numa_strategy_lock, flags);
	list_for_each_entry(s, &numa_strategy_list, list) {
		if (strcmp(s->name, name) == 0) {
			target = s;
			break;
		}
	}
	if (!target) {
		spin_unlock_irqrestore(&numa_strategy_lock, flags);
		pr_err("smartmem: numa strategy '%s' not found\n", name);
		return -ENOENT;
	}
	if (current_numa_strategy)
		current_numa_strategy->enabled = false;
	current_numa_strategy = target;
	target->enabled = true;
	spin_unlock_irqrestore(&numa_strategy_lock, flags);

	pr_info("smartmem: switched to numa strategy: %s\n", name);
	return 0;
}

/* ============ 策略引擎初始化/退出 ============ */

int smartmem_strategy_init(void)
{
	int ret;

	pr_info("smartmem: strategy engine initializing...\n");

	INIT_LIST_HEAD(&buddy_strategy_list);
	INIT_LIST_HEAD(&slub_strategy_list);
	INIT_LIST_HEAD(&lru_strategy_list);
	INIT_LIST_HEAD(&numa_strategy_list);

	/* 注册所有内置策略 */
	ret = numa_buddy_init();
	if (ret) {
		pr_err("smartmem: numa_buddy init failed: %d\n", ret);
		goto err_buddy;
	}

	ret = adaptive_slub_init();
	if (ret) {
		pr_err("smartmem: adaptive_slub init failed: %d\n", ret);
		goto err_slub;
	}

	ret = multi_gen_lru_init();
	if (ret) {
		pr_err("smartmem: multi_gen_lru init failed: %d\n", ret);
		goto err_lru;
	}

	ret = numa_balance_init();
	if (ret) {
		pr_err("smartmem: numa_balance init failed: %d\n", ret);
		goto err_numa;
	}

	pr_info("smartmem: strategy engine initialized\n");
	return 0;

err_numa:
	multi_gen_lru_exit();
err_lru:
	adaptive_slub_exit();
err_slub:
	numa_buddy_exit();
err_buddy:
	return ret;
}

void smartmem_strategy_exit(void)
{
	pr_info("smartmem: strategy engine exiting...\n");

	numa_balance_exit();
	multi_gen_lru_exit();
	adaptive_slub_exit();
	numa_buddy_exit();

	current_buddy_strategy = NULL;
	current_slub_strategy = NULL;
	current_lru_strategy = NULL;
	current_numa_strategy = NULL;

	pr_info("smartmem: strategy engine exited\n");
}
