/* LRU 管理 Hook 实现 */

#include "lru_hook.h"
#include "stats.h"
#include <linux/kprobes.h>
#include <linux/mm.h>

/* Hook 实例 */
static struct smartmem_hook lru_hook_inst = {
    .name = "lru",
    .type = SMARTMEM_HOOK_TYPE_LRU,
    .enabled = false,
    .list = LIST_HEAD_INIT(lru_hook_inst.list),
};

/* kprobe 结构 */
static struct kprobe kp_mark_page_accessed;
static struct kprobe kp_page_referenced;

/**
 * mark_page_accessed 入口处理
 */
static int mark_page_accessed_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
	struct page *page = (struct page *)regs->di;
#else
	return 0;
#endif

	/* 更新 LRU 访问统计 */
	/* TODO: 实现 LRU 页面访问跟踪 */

	pr_debug("smartmem: page accessed, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * folio_referenced 入口处理
 */
static int page_referenced_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
	struct folio *folio = (struct folio *)regs->di;
	int is_locked = (int)regs->si;
#else
	return 0;
#endif

	/* 更新页面引用统计 */
	/* TODO: 实现页面引用跟踪 */

	pr_debug("smartmem: folio referenced, pfn=%lu, locked=%d\n",
		 folio_pfn(folio), is_locked);

	return 0;
}

/**
 * LRU Hook 初始化
 */
int lru_hook_init(void)
{
    int ret;

    pr_info("smartmem: lru hook initializing...\n");

    /* 初始化 mark_page_accessed kprobe */
	memset(&kp_mark_page_accessed, 0, sizeof(kp_mark_page_accessed));
	kp_mark_page_accessed.symbol_name = "mark_page_accessed";
	kp_mark_page_accessed.pre_handler = mark_page_accessed_entry;
	ret = register_kprobe(&kp_mark_page_accessed);
	if (ret) {
		pr_warn("smartmem: failed to register mark_page_accessed kprobe: %d (optional)\n", ret);
	}

    /* 初始化 page_referenced kprobe */
	memset(&kp_page_referenced, 0, sizeof(kp_page_referenced));
	kp_page_referenced.symbol_name = "folio_referenced";
	kp_page_referenced.pre_handler = page_referenced_entry;
	ret = register_kprobe(&kp_page_referenced);
	if (ret) {
		pr_warn("smartmem: failed to register page_referenced kprobe: %d (optional)\n", ret);
	}

    pr_info("smartmem: lru hook initialized\n");
    return 0;
}

/**
 * LRU Hook 退出
 */
void lru_hook_exit(void)
{
    pr_info("smartmem: lru hook exiting...\n");

    unregister_kprobe(&kp_page_referenced);
	unregister_kprobe(&kp_mark_page_accessed);

    pr_info("smartmem: lru hook exited\n");
}

/**
 * 启用 LRU Hook
 */
int lru_hook_enable(void)
{
    pr_info("smartmem: lru hook enabled\n");
    return 0;
}

/**
 * 禁用 LRU Hook
 */
int lru_hook_disable(void)
{
    pr_info("smartmem: lru hook disabled\n");
    return 0;
}
