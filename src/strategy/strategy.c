/* 策略引擎实现 */
#include "strategy.h"

/* Buddy策略列表 */
static LIST_HEAD(buddy_strategy_list);
static struct buddy_strategy *current_buddy_strategy = NULL;
static DEFINE_SPINLOCK(buddy_strategy_lock);

/* SLUB策略列表 */
static LIST_HEAD(slub_strategy_list);
static struct slub_strategy *current_slub_strategy = NULL;
static DEFINE_SPINLOCK(slub_strategy_lock);

/**
 * 策略引擎初始化
 */
int smartmem_strategy_init(void)
{
    pr_info("smartmem: strategy engine initializing...\n");

    INIT_LIST_HEAD(&buddy_strategy_list);
    INIT_LIST_HEAD(&slub_strategy_list);

    pr_info("smartmem: strategy engine initialized\n");
    return 0;
}

/**
 * 策略引擎退出
 */
void smartmem_strategy_exit(void)
{
    pr_info("smartmem: strategy engine exiting...\n");

    /* 清理时会自动释放链表 */

    pr_info("smartmem: strategy engine exited\n");
}

/**
 * 注册Buddy策略
 */
int buddy_strategy_register(struct buddy_strategy *s)
{
    unsigned long flags;

    if (!s || !s->name[0]) {
        pr_err("smartmem: invalid buddy strategy\n");
        return -EINVAL;
    }

    spin_lock_irqsave(&buddy_strategy_lock, flags);
    list_add_tail(&s->list, &buddy_strategy_list);
    if (!current_buddy_strategy) {
        current_buddy_strategy = s;
        s->enabled = true;
    }
    spin_unlock_irqrestore(&buddy_strategy_lock, flags);

    pr_info("smartmem: registered buddy startegy: %s\n", s->name);
    return 0;
}

/**
 * 注销Buddy策略
 */
int buddy_strategy_unregister(struct buddy_strategy *s)
{
    unsigned long flags;

    if (!s) {
        return -EINVAL;
    }

    spin_lock_irqsave(&buddy_strategy_lock, flags);
    if (current_buddy_strategy == s) {
        current_buddy_strategy = NULL;
    }
    list_del(&s->list);
    s->enabled = false;
    spin_unlock_irqrestore(&buddy_strategy_lock, flags);

    pr_info("smartmem: unregistered buddy strategy: %s\n", s->name);
    return 0;
}

/**
 * 获取当前Buddy策略
 */
struct buddy_strategy *buddy_strategy_get_current(void)
{
    struct buddy_strategy *s;
    unsigned long flags;

    spin_lock_irqsave(&buddy_strategy_lock, flags);
    s = current_buddy_strategy;
    spin_unlock_irqrestore(&buddy_strategy_lock, flags);

    return s;
}

/**
 * 切换到指定Buddy策略
 */
int buddy_strategy_switch(const char *name)
{
    struct buddy_strategy *s, *target = NULL;
    unsigned long flags;

    if (!name) {
        return -EINVAL;
    }

    /* 查找目标策略 */
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

    /* 切换策略 */
    if (current_buddy_strategy) {
        current_buddy_strategy->enabled = false;
    }
    current_buddy_strategy = target;
    target->enabled = true;
    spin_unlock_irqrestore(&buddy_strategy_lock, flags);

    pr_info("smartmem: switched to buddy strategy: %s\n", name);
    return 0;
}

/* SLUB策略相关函数 */

/**
 * 注册SLUB策略 
 */
int slub_strategy_register(struct slub_strategy *s)
{
    unsigned long flags;

    if (!s || !s->name[0]) {
        pr_err("smartmem: invalid slub strategy\n");
        return -EINVAL;
    }

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

/**
 * 注销SLUB策略
 */
int slub_strategy_unregister(struct slub_strategy *s)
{
    unsigned long flags;

    if (!s) {
        return -EINVAL;
    }

    spin_lock_irqsave(&slub_strategy_lock, flags);
    if (current_slub_strategy == s) {
        current_slub_strategy = NULL;
    }
    list_del(&s->list);
    s->enabled = false;
    spin_unlock_irqrestore(&slub_strategy_lock, flags);

    pr_info("smartmem: unregistered slub strategy: %s\n", s->name);
    return 0;
}

/**
 * 获取当前SLUB策略
 */
struct slub_strategy *slub_strategy_get_current(void)
{
    struct slub_strategy *s;
    unsigned long flags;

    spin_lock_irqsave(&slub_strategy_lock, flags);
    s = current_slub_strategy;
    spin_unlock_irqrestore(&slub_strategy_lock, flags);

    return s;
}

/**
 * 切换到指定SLUB策略
 */
int slub_strategy_switch(const char *name)
{
    struct slub_strategy *s, *target = NULL;
    unsigned long flags;

    if (!name) {
        return -EINVAL;
    }

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

    if (current_slub_strategy) {
        current_slub_strategy->enabled = false;
    }
    current_slub_strategy = target;
    target->enabled = true;
    spin_unlock_irqrestore(&slub_strategy_lock, flags);

    pr_info("smartmem: switched to slub strategy: %s\n", name);
    return 0;
}
