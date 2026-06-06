# Makefile for SmartMemEngine userspace tools

CC = gcc
CFLAGS = -Wall -Wextra -O2

# memtrace 是真正的 BPF loader：需要 libbpf + libelf + zlib
# 头文件来自 ../bpf/smartmem.skel.h (bpftool gen 产生)
MEMTRACE_INC    = -I../bpf
MEMTRACE_LIBS   = -lbpf -lelf -lz

TARGETS_BASIC   = memctl memstat memview
TARGETS_BPF     = memtrace
TARGETS         = $(TARGETS_BASIC) $(TARGETS_BPF)

.PHONY: all basic bpf clean install uninstall help

all: basic bpf

basic: $(TARGETS_BASIC)

bpf: $(TARGETS_BPF)

memctl: memctl.c
	$(CC) $(CFLAGS) -o $@ $<

memstat: memstat.c
	$(CC) $(CFLAGS) -o $@ $<

memview: memview.c
	$(CC) $(CFLAGS) -o $@ $<

# memtrace 依赖 ../bpf/smartmem.skel.h，需要先在 ../bpf 跑 `make skeleton`
memtrace: memtrace.c ../bpf/smartmem.skel.h
	$(CC) $(CFLAGS) $(MEMTRACE_INC) -o $@ $< $(MEMTRACE_LIBS)

../bpf/smartmem.skel.h:
	@echo "==> Generating BPF skeleton..."
	$(MAKE) -C ../bpf skeleton

clean:
	rm -f $(TARGETS)

install: all
	install -d /usr/local/bin
	install -m 755 memctl    /usr/local/bin/
	install -m 755 memstat   /usr/local/bin/
	install -m 755 memview   /usr/local/bin/
	-install -m 755 memtrace /usr/local/bin/ 2>/dev/null

uninstall:
	rm -f /usr/local/bin/memctl
	rm -f /usr/local/bin/memstat
	rm -f /usr/local/bin/memview
	rm -f /usr/local/bin/memtrace

help:
	@echo "Targets:"
	@echo "  all       — build everything (basic + memtrace BPF loader)"
	@echo "  basic     — memctl, memstat, memview (no extra deps)"
	@echo "  bpf       — memtrace (requires libbpf-devel, libelf-devel, zlib-devel)"
	@echo "  clean     — remove built binaries"
	@echo "  install   — install to /usr/local/bin"
