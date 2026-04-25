/* hooks/vma_hook.c - VMA 管理 Hook 实现 */

#include "vma_hook.h"
#include "stats.h"
#include <linux/kprobes.h>
#include <linux/mm.h>

/* Hook 实例 */
static struct smartmem_hook vma_hook_inst = {
    .name = "vma",
    .type = SMARTMEM_HOOK_TYPE_VMA,
    .enabled = false,
    .list = LIST_HEAD_INIT(vma_hook_inst.list),
};

// kprobe结构
static struct kprobe kp_mmap;
static struct kprobe kp_munmap;
static struct kprobe kp_mremap;

// mmap 入口处理
static int mmap_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined (CONFIG_X86_64)
    unsigned long addr = (unsigned long)regs->di;
    unsigned long len = (unsigned long)regs->si;
    unsigned long prot = (unsigned long)regs->dx;
	// unsigned long flags = (unsigned long)regs->r10;
#else
    return 0;
#endif

    /* TODO: 调用策略引擎 */
	/* vma_pre_mmap(addr, len, prot, flags); */

	pr_debug("smartmem: mmap addr=%lx, len=%lx, prot=%lx\n", addr, len, prot);

	return 0;
}

// munmap 入口处理
static int munmap_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
	unsigned long addr = (unsigned long)regs->di;
	size_t len = (size_t)regs->si;
#else
	return 0;
#endif

	/* TODO: 调用策略引擎 */
	/* vma_pre_munmap(addr, len); */

	pr_debug("smartmem: munmap addr=%lx, len=%zx\n", addr, len);

	return 0;
}

// mremap 入口处理
static int mremap_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
	/* __x64_sys_mremap: regs->di 是 pt_regs 指针，参数从中取 */
	struct pt_regs *real_regs = (struct pt_regs *)regs->di;
	unsigned long old_addr = real_regs->di;
	// unsigned long old_len = real_regs->si;
	// unsigned long new_len = real_regs->dx;
	// unsigned long flags = real_regs->r10;
	unsigned long new_addr = real_regs->r8;
#else
	return 0;
#endif

	/* TODO: 调用策略引擎 */
	/* vma_pre_mremap(old_addr, old_len, new_len, flags, new_addr); */

	pr_debug("smartmem: mremap old=%lx, new=%lx\n", old_addr, new_addr);

	return 0;
}

/**
 * VMA Hook 初始化
 */
int vma_hook_init(void)
{
    int ret;

    pr_info("smartmem: vma hook initializing...\n");
    
    // 初始化 mmap kprobe
    memset(&kp_mmap, 0, sizeof(kp_mmap));
    kp_mmap.symbol_name = "do_mmap";
    kp_mmap.pre_handler = mmap_entry;
    ret = register_kprobe(&kp_mmap);
    if (ret) {
        pr_warn("smartmem: failed to register mmap kprobe: %d (optional)\n", ret);
		/* 尝试备用符号 */
		kp_mmap.symbol_name = "vm_mmap";
		ret = register_kprobe(&kp_mmap);
		if (ret) {
			pr_warn("smartmem: failed to register vm_mmap kprobe: %d (optional)\n", ret);
		}
    }
    
    // 初始化 munmap kprobe
    memset(&kp_munmap, 0, sizeof(kp_munmap));
    kp_munmap.symbol_name = "do_munmap";
	kp_munmap.pre_handler = munmap_entry;
	ret = register_kprobe(&kp_munmap);
	if (ret) {
		pr_warn("smartmem: failed to register munmap kprobe: %d (optional)\n", ret);
	}

    /* 初始化 mremap kprobe */
	memset(&kp_mremap, 0, sizeof(kp_mremap));
	kp_mremap.symbol_name = "__x64_sys_mremap";
	kp_mremap.pre_handler = mremap_entry;
	ret = register_kprobe(&kp_mremap);
	if (ret) {
		pr_warn("smartmem: failed to register mremap kprobe: %d (optional)\n", ret);
	}

    pr_info("smartmem: vma hook initialized\n");
    return 0;
}

/**
 * VMA Hook 退出
 */
void vma_hook_exit(void)
{
    pr_info("smartmem: vma hook exiting...\n");

    unregister_kprobe(&kp_mremap);
	unregister_kprobe(&kp_munmap);
	unregister_kprobe(&kp_mmap);

    pr_info("smartmem: vma hook exited\n");
}

/**
 * 启用 VMA Hook
 */
int vma_hook_enable(void)
{
    pr_info("smartmem: vma hook enabled\n");
    return 0;
}

/**
 * 禁用 VMA Hook
 */
int vma_hook_disable(void)
{
    pr_info("smartmem: vma hook disabled\n");
    return 0;
}