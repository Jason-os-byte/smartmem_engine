/* tools/memstat.c - SmartMemEngine eBPF 加载器
 *
 * 编译步骤:
 *   1. cd src/bpf && make
 *   2. make skeleton
 *   3. cp smartmem.skel.h ../../tools/
 *   4. cd ../../tools && gcc -o memstat memstat.c -lbpf -lelf -lz
 *
 * 运行:
 *   sudo ./memstat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "smartmem.skel.h"

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

enum sm_event_type {
	SM_EVENT_BUDDY_ALLOC = 1,
	SM_EVENT_BUDDY_FREE,
	SM_EVENT_PAGE_FAULT,
};

struct sm_event {
	__u32 type;
	__u32 pid;
	__u64 timestamp;
	__u64 stack_id;
	union {
		struct {
			__u32 order;
			__u32 gfp_mask;
			__u64 latency_ns;
		} buddy_alloc;
		struct {
			__u32 order;
		} buddy_free;
	};
} __attribute__((packed));

static int handle_event(void *ctx, void *data, size_t len)
{
	struct sm_event *e = data;

	switch (e->type) {
	case SM_EVENT_BUDDY_ALLOC:
		printf("[BUDDY_ALLOC] pid=%-6u order=%-2u gfp=0x%-8x latency=%lluns\n",
		       e->pid, e->buddy_alloc.order,
		       e->buddy_alloc.gfp_mask,
		       e->buddy_alloc.latency_ns);
		break;
	case SM_EVENT_BUDDY_FREE:
		printf("[BUDDY_FREE ] pid=%-6u order=%-2u\n",
		       e->pid, e->buddy_free.order);
		break;
	default:
		printf("[UNKNOWN    ] pid=%-6u type=%u\n", e->pid, e->type);
		break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct smartmem_bpf *skel;
	struct ring_buffer *rb;
	int err;

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	skel = smartmem_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = smartmem_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF program: %d\n", err);
		goto cleanup;
	}

	err = smartmem_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF program: %d\n", err);
		goto cleanup;
	}

	rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
			      handle_event, NULL, NULL);
	if (!rb) {
		fprintf(stderr, "Failed to create ring buffer\n");
		goto cleanup;
	}

	printf("SmartMemEngine eBPF monitor started.\n");
	printf("Press Ctrl-C to stop.\n\n");

	while (!exiting) {
		err = ring_buffer__poll(rb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "Ring buffer poll error: %d\n", err);
			break;
		}
	}

	printf("\neBPF monitor stopped.\n");
	ring_buffer__free(rb);

cleanup:
	smartmem_bpf__destroy(skel);
	return err != 0;
}
