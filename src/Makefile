# Makefile for SmartMemEngine

obj-m := smartmem.o

# 添加头文件搜索路径
ccflags-y += -I$(src)
ccflags-y += -I$(src)/core
ccflags-y += -I$(src)/hooks

smartmem-y := main.o
# 框架编译通过后取消注释下面的行
# 核心模块
smartmem-y += core/engine.o
smartmem-y += core/config.o
smartmem-y += core/stats.o

# Hook 模块
smartmem-y += hooks/hook.o
smartmem-y += hooks/buddy_hook.o
smartmem-y += hooks/slub_hook.o
smartmem-y += hooks/vma_hook.o
smartmem-y += hooks/lru_hook.o
smartmem-y += hooks/numa_hook.o

# smartmem-y += strategy/strategy.o
# smartmem-y += strategy/builtin_strategies/numa_buddy.o
# smartmem-y += strategy/builtin_strategies/adaptive_slub.o
# smartmem-y += strategy/builtin_strategies/multi_gen_lru.o
# smartmem-y += strategy/builtin_strategies/numa_balance.o
# smartmem-y += monitor/monitor.o
# smartmem-y += monitor/ebpf_monitor.o
# smartmem-y += monitor/trace_monitor.o
# smartmem-y += analysis/analysis.o
# smartmem-y += analysis/hotspot.o
# smartmem-y += analysis/bottleneck.o
# smartmem-y += analysis/root_cause.o
# smartmem-y += optimization/optimization.o
# smartmem-y += optimization/auto_tune.o
# smartmem-y += optimization/predictive.o
# smartmem-y += interface/interface.o
# smartmem-y += interface/proc.o
# smartmem-y += interface/debugfs.o

# 获取内核源码路径
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install