/* Buddy 分配 Hook 实现 */

#include "buddy_hook.h"
#include "stats.h"

/* Hook 实例 */
static  struct smartmem_hook buddy_hook_inst = {
    .name = "buddy",
    .type = SMARTMEM_HOOK_TYPE_BUDDY,
    .enabled = false,
    .list = LIST_HEAD_INIT(buddy_hook_inst.list),
};

/* kprobe 结构 */
static struct kprobe kp_alloc_pages;
static struct kretprobe krp_alloc_pages;
static struct kprobe kp_free_pages;

/* per-CPU 参数缓存 */
static DEFINE_PER_CPU(struct buddy_alloc_args, buddy_args);

/**
 * alloc_pages 入口处理
 */
static int alloc_pages_entry(struct kprobe *p, struct pt_regs *regs)
{
    struct buddy_alloc_args *args;
    int order;
    gfp_t gfp_mask;

    /* 获取参数 （x86-64约定） */
#if defined(CONFIG_X86_64)
    gfp_mask = (gfp_t)regs->di; // 第一个参数
    order = (int)regs->si; // 第二个参数
#else
    /* 其他架构处理 */
    return 0;
#endif

    /* 保存到 per-CPU缓存 */
    args = this_cpu_ptr(&buddy_args);
    args->order = order;
    args->gfp_mask = gfp_mask;
    args->start_time = ktime_get();
    args->active = true;

    return 0;
}

/**
 * alloc_pages 返回处理
 */
static int alloc_pages_return(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct buddy_alloc_args *args;
    struct page *page;
    ktime_t end_time;
    s64 delta_ns;

    /* 获取返回值 - 分配的页面 */
    page = (struct page *)regs_return_value(regs);

    /* 获取 per-CPU 缓存 */
    args = this_cpu_ptr(&buddy_args);
    if (!args->active) {
        return 0;
    }
    args->active = false;

    /* 计算耗时 */
    end_time = ktime_get();
    delta_ns = ktime_to_ns(ktime_sub(end_time, args->start_time));

    /* 更新统计 */
    smartmem_stats_buddy_alloc_inc();

    /* TODO: 调用策略引擎 */
    if (delta_ns > 1000000) {
        pr_debug("smartmem: slow buddy alloc: order=%d, gfp=%x, time=%lldns\n",
                    args->order, args->gfp_mask, delta_ns);       
    }

    return 0;
}

/**
 * free_pages 入口处理
 */
static int free_pages_entry(struct kprobe *p, struct pt_regs *regs)
{
    struct page *page;
    int order;

#if defined(CONFIG_X86_64)
    page = (struct page *)regs->di; // 第一个参数
    order = (int)regs->si; //第二个参数 
#else
    return 0;
#endif

    /* TODO: 调用策略引擎 */
    /* buddy_pre_free(page, order); */

    /* 更新统计 */
    smartmem_stats_buddy_free_inc();

    return 0;
}

/**
 * Buddy Hook 初始化
 */
int buddy_hook_init(void)
{
    int ret;

    pr_info("smartmem: buddy hook initializing...\n");

    /* 初始化 kprobe */
    memset(&kp_alloc_pages, 0, sizeof(kp_alloc_pages));
    kp_alloc_pages.symbol_name = "__alloc_pages_noprof";
    kp_alloc_pages.pre_handler = alloc_pages_entry;
    ret = register_kprobe(&kp_alloc_pages);
    if (ret) {
        pr_err("smartmem: failed to register alloc_pages kprobe: %d\n", ret);
        return ret;
    }

    /* 初始化kretprobe */
    memset(&krp_alloc_pages, 0, sizeof(krp_alloc_pages));
    krp_alloc_pages.kp.symbol_name = "__alloc_pages_noprof";
    krp_alloc_pages.handler = alloc_pages_return;
    ret = register_kretprobe(&krp_alloc_pages);
    if (ret) {
        pr_err("smartmem: failed to register alloc_pages kretprobe: %d\n", ret);
        unregister_kprobe(&kp_alloc_pages);
        return ret;
    }

    /* 初始化 free_pages kprobe */
    memset(&kp_free_pages, 0, sizeof(kp_free_pages));
    kp_free_pages.symbol_name = "__free_pages";
    kp_free_pages.pre_handler = free_pages_entry;
    ret = register_kprobe(&kp_free_pages);
    if (ret) {
        pr_warn("smartmem: failed to register free_pages kprobe: %d (optional)\n", ret);
        /* 非致命错误，继续 */
    }

    pr_info("smartmem: buddy hook initialized\n");
    return 0;
}

/**
 * Buddy Hook 退出
 */
void buddy_hook_exit(void)
{
    pr_info("smartmem: buddy hook exiting...\n");

    unregister_kprobe(&kp_free_pages);
    unregister_kretprobe(&krp_alloc_pages);
    unregister_kprobe(&kp_alloc_pages);

    pr_info("smartmem: buddy hook exited\n");
}

/**
 * 启用 Buddy Hook
 */
int buddy_hook_enable(void)
{
    pr_info("smartmem: buddy hook enabled\n");

    /* TODO: 启用 kprobe/kretprobe */

    return 0;
}

/**
 * 禁用 Buddy Hook
 */
int buddy_hook_disable(void)
{
    pr_info("smartmem: buddy hook disabled\n");
    /* TODO: 禁用 kprobe/kretprobe */
    return 0;
}
