# BPF 程序构建系统

CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= $(shell which bpftool 2>/dev/null || /usr/sbin/bpftool)
KERNEL_HEADERS ?= /lib/modules/$(shell uname -r)/build

BPF_OBJ = smartmem.bpf.o

CFLAGS = -g -O2 -target bpf
CFLAGS += -D__TARGET_ARCH_x86
CFLAGS += -I$(KERNEL_HEADERS)/include
CFLAGS += -I$(KERNEL_HEADERS)/include/uapi
CFLAGS += -I$(KERNEL_HEADERS)/arch/x86/include
CFLAGS += -I.

.PHONY: all clean vmlinux skeleton

all: vmlinux $(BPF_OBJ)

vmlinux:
	@if [ ! -f vmlinux.h ]; then \
		echo "Generating vmlinux.h..."; \
		$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h; \
	fi

$(BPF_OBJ): smartmem.bpf.c vmlinux.h
	$(CLANG) $(CFLAGS) -c $< -o $@
	$(LLVM_STRIP) -g $@ 2>/dev/null || true

skeleton: $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > smartmem.skel.h
	@echo "Generated smartmem.skel.h"
	@echo "Copy to tools/ and compile: gcc -o memstat tools/memstat.c -lbpf -lelf -lz"

clean:
	rm -f $(BPF_OBJ) vmlinux.h smartmem.skel.h
