/* numa_hook.c - NUMA 管理 Hook 实现 */

#include "numa_hook.h"
#include "stats.h"
#include "strategy.h"
#include <linux/kprobes.h>
#include <linux/migrate.h>

/* Hook 实例 */
static struct smartmem_hook numa_hook_inst = {
	.name = "numa",
	.type = SMARTMEM_HOOK_TYPE_NUMA,
	.enabled = false,
	.list = LIST_HEAD_INIT(numa_hook_inst.list),
};

/* kprobe 结构 */
static struct kprobe kp_migrate_pages;
static struct kprobe kp_migrate_misplaced_page;

/**
 * migrate_pages 入口处理
 */
static int migrate_pages_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
    unsigned long npages = (unsigned long)regs->si;
    int mode = (int)regs->r8;
#else
    return 0;
#endif

    /* 通知 NUMA 策略发生页面迁移 */
    // numa_pages_migrating(npages, mode);

    pr_debug("smartmem: migrate_pages npages=%lu, mode=%d\n", npages, mode);

    return 0;
}

/**
 * migrate_misplaced_folio 入口处理
 */
static int migrate_misplaced_page_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
    struct folio *folio = (struct folio *)regs->di;
#else
    return 0;
#endif

    /* 通知 NUMA 策略发现错位页面 */
    // numa_misplaced_page(folio);

    pr_debug("smartmem: migrate_misplaced_folio pfn=%lu\n", folio_pfn(folio));

    return 0;
}

/**
 * NUMA Hook 初始化
 */
int numa_hook_init(void)
{
	int ret;

	pr_info("smartmem: numa hook initializing...\n");

	/* 初始化 migrate_pages kprobe */
	memset(&kp_migrate_pages, 0, sizeof(kp_migrate_pages));
	kp_migrate_pages.symbol_name = "migrate_pages";
	kp_migrate_pages.pre_handler = migrate_pages_entry;
	ret = register_kprobe(&kp_migrate_pages);
	if (ret) {
		pr_warn("smartmem: failed to register migrate_pages kprobe: %d (optional)\n", ret);
	}

	/* 初始化 migrate_misplaced_page kprobe */
	memset(&kp_migrate_misplaced_page, 0, sizeof(kp_migrate_misplaced_page));
	kp_migrate_misplaced_page.symbol_name = "migrate_misplaced_folio";
	kp_migrate_misplaced_page.pre_handler = migrate_misplaced_page_entry;
	ret = register_kprobe(&kp_migrate_misplaced_page);
	if (ret) {
		pr_warn("smartmem: failed to register migrate_misplaced_page kprobe: %d (optional)\n", ret);
	}

	pr_info("smartmem: numa hook initialized\n");
	return 0;
}

/**
 * NUMA Hook 退出
 */
void numa_hook_exit(void)
{
	pr_info("smartmem: numa hook exiting...\n");

	unregister_kprobe(&kp_migrate_misplaced_page);
	unregister_kprobe(&kp_migrate_pages);

	pr_info("smartmem: numa hook exited\n");
}

/**
 * 启用 NUMA Hook
 */
int numa_hook_enable(void)
{
    int ret;

    pr_info("smartmem: numa hook enabling...\n");

    ret = enable_kprobe(&kp_migrate_pages);
    if (ret) {
        pr_warn("smartmem: failed to enable migrate_pages kprobe: %d\n", ret);
        return ret;
    }

    ret = enable_kprobe(&kp_migrate_misplaced_page);
    if (ret) {
        pr_warn("smartmem: failed to enable migrate_misplaced_page kprobe: %d\n", ret);
        disable_kprobe(&kp_migrate_pages);
        return ret;
    }

    pr_info("smartmem: numa hook enabled\n");
    return 0;
}

/**
 * 禁用 NUMA Hook
 */
int numa_hook_disable(void)
{
    pr_info("smartmem: numa hook disabling...\n");

    disable_kprobe(&kp_migrate_pages);
    disable_kprobe(&kp_migrate_misplaced_page);

    pr_info("smartmem: numa hook disabled\n");
    return 0;
}