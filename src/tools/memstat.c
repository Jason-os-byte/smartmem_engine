/* memstat - SmartMemEngine 统计查看工具 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PROC_BASE "/proc/smartmem"

static void usage(void)
{
    printf("Usage: memstat <command>\n\n");
    printf("Commands:\n");
    printf("  memstat stats        Show allocation statistics\n");
    printf("  memstat policies     Show active policies\n");
    printf("  memstat hotspots     Show top allocation hotspots\n");
    printf("  memstat bottlenecks  Show detected bottlenecks\n");
    printf("  memstat rootcauses   Show root cause analysis\n");
    printf("  memstat autotune     Show auto-tune status and history\n");
    printf("  memstat prediction   Show memory prediction\n");
    printf("  memstat all          Show all information\n");
    printf("  memstat summary      Show one-line summary\n");
}

static int read_proc_file(const char *path)
{
    FILE *fp;
    char buf[4096];
    size_t n;

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        fwrite(buf, 1, n, stdout);

    fclose(fp);
    return 0;
}

static int read_proc_named(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", PROC_BASE, name);
    return read_proc_file(path);
}

/* 一行摘要 */
static int cmd_summary(void)
{
    FILE *fp;
    char path[256];
    char line[256];
    unsigned long buddy_alloc = 0, buddy_free = 0;
    unsigned long numa_local = 0, numa_remote = 0;

    /* 从 stats 文件中解析关键数据 */
    snprintf(path, sizeof(path), "%s/stats", PROC_BASE);
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot read stats: %s\n", strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "  alloc_count: %lu", &buddy_alloc);
        sscanf(line, "  free_count:  %lu", &buddy_free);
        sscanf(line, "  local_alloc:  %lu", &numa_local);
        sscanf(line, "  remote_alloc: %lu", &numa_remote);
    }
    fclose(fp);

    /* 计算空闲率 */
    {
        FILE *mfp;
        unsigned long total = 0, free_pages = 0;

        mfp = fopen("/proc/meminfo", "r");
        if (mfp) {
            char ml[256];
            while (fgets(ml, sizeof(ml), mfp)) {
                sscanf(ml, "MemTotal: %lu kB", &total);
                sscanf(ml, "MemFree: %lu kB", &free_pages);
            }
            fclose(mfp);
        }

        if (total > 0) {
            int free_pct = (int)((free_pages * 100) / total);
            int numa_pct = (numa_local + numa_remote > 0) ?
                (int)((numa_local * 100) / (numa_local + numa_remote)) : 100;

            printf("[smartmem] buddy_alloc=%lu buddy_free=%lu | "
                   "free=%d%% | numa_locality=%d%%\n",
                   buddy_alloc, buddy_free, free_pct, numa_pct);
        } else {
            printf("[smartmem] buddy_alloc=%lu buddy_free=%lu\n",
                   buddy_alloc, buddy_free);
        }
    }

    return 0;
}

/* all 子命令：显示所有信息 */
static int cmd_all(void)
{
    const char *files[] = {
        "stats", "policies", "hotspots", "bottlenecks",
        "rootcauses", "autotune", "prediction"
    };
    int i;

    for (i = 0; i < 7; i++) {
        printf("\n");
        if (read_proc_named(files[i]) != 0)
            fprintf(stderr, "Warning: failed to read %s\n", files[i]);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    if (access(PROC_BASE, F_OK) != 0) {
        fprintf(stderr, "Error: smartmem module not loaded (%s not found)\n",
                PROC_BASE);
        return 1;
    }

    if (strcmp(argv[1], "stats") == 0) {
        return read_proc_named("stats");
    } else if (strcmp(argv[1], "policies") == 0) {
        return read_proc_named("policies");
    } else if (strcmp(argv[1], "hotspots") == 0) {
        return read_proc_named("hotspots");
    } else if (strcmp(argv[1], "bottlenecks") == 0) {
        return read_proc_named("bottlenecks");
    } else if (strcmp(argv[1], "rootcauses") == 0) {
        return read_proc_named("rootcauses");
    } else if (strcmp(argv[1], "autotune") == 0) {
        return read_proc_named("autotune");
    } else if (strcmp(argv[1], "prediction") == 0) {
        return read_proc_named("prediction");
    } else if (strcmp(argv[1], "all") == 0) {
        return cmd_all();
    } else if (strcmp(argv[1], "summary") == 0) {
        return cmd_summary();
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    } else {
        fprintf(stderr, "Error: unknown command: %s\n", argv[1]);
        usage();
        return 1;
    }
}
