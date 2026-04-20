/* SLUB 分配 Hook 实现 */
#include "slub_hook.h"
#include "stats.h"
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <linux/ktime.h>

/* Hook 实例 */
static struct smartmem_hook slub_hook_inst = {
    .name = "slub",
    .type = SMARTMEM_HOOK_TYPE_SLUB,
    .enabled = false,
    .list = LIST_HEAD_INIT(slub_hook_inst.list),
};

// kprobe结构
static struct kprobe kp_kmalloc;
static struct kretprobe krp_kmalloc;
static struct kprobe kp_kfree;

// per-CPU 参数缓存
static DEFINE_PER_CPU(struct slub_alloc_args, slub_alloc_args);

// kmalloc 入口处理
static int kmalloc_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
    size_t size = (size_t)regs->di; // 第一个参数
#else 
    return 0;
#endif

    struct slub_alloc_args *args = this_cpu_ptr(&slub_alloc_args);
    args->size = size;
    args->start_time = ktime_get();
    args->active = true;

    return 0;
}


/**
 * kmalloc 返回处理
 */
static int kmalloc_return(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	//void *ptr = (void *)regs_return_value(regs);

	struct slub_alloc_args *args = this_cpu_ptr(&slub_alloc_args);
	if (!args->active)
		return 0;
	args->active = false;

	/* 更新统计 */
	smartmem_stats_slub_alloc_inc();

	/* TODO: 调用策略引擎 */

	return 0;
}


/**
 * kfree 入口处理
 */
static int kfree_entry(struct kprobe *p, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
	//void *ptr = (void *)regs->di;  /* 第一个参数：ptr */
#else
	return 0;
#endif

	/* TODO: 调用策略引擎 */

	/* 更新统计 */
	smartmem_stats_slub_free_inc();

	return 0;
}


/**
 * SLUB Hook 初始化
 */
int slub_hook_init(void)
{
    int ret;

	pr_info("smartmem: slub hook initializing...\n");

	/* 初始化 kmalloc kprobe */
	memset(&kp_kmalloc, 0, sizeof(kp_kmalloc));
	kp_kmalloc.symbol_name = "__kmalloc_noprof";
	kp_kmalloc.pre_handler = kmalloc_entry;
	ret = register_kprobe(&kp_kmalloc);
	if (ret) {
		pr_err("smartmem: failed to register kmalloc kprobe: %d\n", ret);
		return ret;
	}

	/* 初始化 kmalloc kretprobe */
	memset(&krp_kmalloc, 0, sizeof(krp_kmalloc));
	krp_kmalloc.kp.symbol_name = "__kmalloc_noprof";
	krp_kmalloc.handler = kmalloc_return;
	ret = register_kretprobe(&krp_kmalloc);
	if (ret) {
		pr_err("smartmem: failed to register kmalloc kretprobe: %d\n", ret);
		unregister_kprobe(&kp_kmalloc);
		return ret;
	}

	/* 初始化 kfree kprobe */
	memset(&kp_kfree, 0, sizeof(kp_kfree));
	kp_kfree.symbol_name = "kfree";
	kp_kfree.pre_handler = kfree_entry;
	ret = register_kprobe(&kp_kfree);
	if (ret) {
		pr_warn("smartmem: failed to register kfree kprobe: %d (optional)\n", ret);
		/* 非致命错误，继续 */
	}

	pr_info("smartmem: slub hook initialized\n");
	return 0;
}

/**
 * SLUB Hook 退出
 */
void slub_hook_exit(void)
{
    pr_info("smartmem: slub hook exiting...\n");

    unregister_kprobe(&kp_kfree);
	unregister_kretprobe(&krp_kmalloc);
	unregister_kprobe(&kp_kmalloc);

    pr_info("smartmem: slub hook exited\n");
}

/**
 * 启用 SLUB Hook
 */
int slub_hook_enable(void)
{
    pr_info("smartmem: slub hook enabled\n");
    /* TODO: 启用 kprobe/kretprobe */
    return 0;
}

/**
 * 禁用 SLUB Hook
 */
int slub_hook_disable(void)
{
    pr_info("smartmem: slub hook disabled\n");
    /* TODO: 禁用 kprobe/kretprobe */
    return 0;
}





