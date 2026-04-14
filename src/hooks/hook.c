/* Hook 管理框架实现 */
#include "hook.h"

/* Hook 链表 */
static LIST_HEAD(hook_list);
static DEFINE_SPINLOCK(hook_lock);

/**
 * Hook 管理初始化
 */
int smartmem_hook_init(void)
{
    pr_info("smartmem: hook manager initializing...\n");

    INIT_LIST_HEAD(&hook_list);

    pr_info("smartmem: hook manager initialized\n");
    return 0;
}

/**
 * Hook 管理退出
 */
void smartmem_hook_exit(void)
{
    pr_info("smartmem: hook manager exiting...\n");

    /* 链表应该已经清空 */

    pr_info("smartmem: hook manager exited\n");
}

/**
 * 注册 Hook
 */
int smartmem_hook_register(struct smartmem_hook *hook)
{
    unsigned long flags;

    if (!hook || !hook->ops.init || !hook->ops.exit) {
        pr_err("smartmem: invalid hook\n");
        return -EINVAL;
    }

    spin_lock_irqsave(&hook_lock, flags);
    list_add_tail(&hook->list, &hook_list);
    spin_unlock_irqrestore(&hook_lock, flags);

    pr_info("smartmem: registered hook %s\n", hook->name);
    return 0;
}

/**
 * 注销 Hook
 */
int smartmem_hook_unregister(struct smartmem_hook *hook)
{
    unsigned long flags;

    if (!hook) {
        pr_err("smartmem: invalid hook\n");
        return -EINVAL;
    }

    spin_lock_irqsave(&hook_lock, flags);
    list_del(&hook->list);
    spin_unlock_irqrestore(&hook_lock, flags);

    pr_info("smartmem: unregistered hook %s\n", hook->name);
    return 0;
}

/**
 * 启用指定类型的 Hook
 */
int smartmem_hook_enable(enum smartmem_hook_type type)
{
    struct smartmem_hook *hook;
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&hook_lock, flags);
    list_for_each_entry(hook, &hook_list, list) {
        if (hook->type == type && hook->ops.enable) {
            ret = hook->ops.enable();
            if (ret == 0) {
                hook->enabled = true;
                pr_info("smartmem: enabled hook %s\n", hook->name);
            }
            break;
        }
    }
    spin_unlock_irqrestore(&hook_lock, flags);

    return 0;
}

/**
 * 禁用指定类型的 Hook
 */
int smartmem_hook_disable(enum smartmem_hook_type type)
{
    struct smartmem_hook *hook;
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&hook_lock, flags);
    list_for_each_entry(hook, &hook_list, list) {
        if (hook->type == type && hook->ops.disable) {
            ret = hook->ops.disable();
            if (ret == 0) {
                hook->enabled = false;
                pr_info("smartmem: disabled hook %s\n", hook->name);
            }
            break;
        }
    }
    spin_unlock_irqrestore(&hook_lock, flags);

    return ret;
}

