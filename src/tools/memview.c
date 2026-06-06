/* memview - SmartMemEngine 可视化展示工具 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>

#define PROC_BASE "/proc/smartmem"

/* ANSI 颜色码 */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

static void usage(void)
{
    printf("Usage: memview <command> [options]\n\n");
    printf("Commands:\n");
    printf("  memview overview    Color-coded system overview\n");
    printf("  memview top         Real-time monitoring (refresh every 2s)\n");
    printf("  memview watch [n]   Watch mode, refresh every n seconds (default 5)\n");
    printf("  memview bar         Memory usage bar chart\n");
}

/* 读取 /proc/meminfo 中的关键字段 */
static int read_meminfo(unsigned long *total, unsigned long *free,
                        unsigned long *available, unsigned long *cached,
                        unsigned long *slab)
{
    FILE *fp;
    char line[256];

    fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;

    *total = *free = *available = *cached = *slab = 0;
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "MemTotal: %lu kB", total);
        sscanf(line, "MemFree: %lu kB", free);
        sscanf(line, "MemAvailable: %lu kB", available);
        sscanf(line, "Cached: %lu kB", cached);
        sscanf(line, "Slab: %lu kB", slab);
    }

    fclose(fp);
    return 0;
}

/* 从 smartmem stats 中解析数据 */
static int read_smartmem_stats(unsigned long *buddy_alloc,
                               unsigned long *buddy_free,
                               unsigned long *slub_alloc,
                               unsigned long *slub_free,
                               unsigned long *numa_local,
                               unsigned long *numa_remote)
{
    FILE *fp;
    char path[256], line[256];

    *buddy_alloc = *buddy_free = *slub_alloc = *slub_free = 0;
    *numa_local = *numa_remote = 0;

    snprintf(path, sizeof(path), "%s/stats", PROC_BASE);
    fp = fopen(path, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "  alloc_count: %lu", buddy_alloc);
        sscanf(line, "  free_count:  %lu", buddy_free);
        sscanf(line, "  local_alloc:  %lu", numa_local);
        sscanf(line, "  remote_alloc: %lu", numa_remote);
    }

    fclose(fp);

    /* SLUB 数据在第二段，需要跳过 Buddy 段后再解析 */
    snprintf(path, sizeof(path), "%s/stats", PROC_BASE);
    fp = fopen(path, "r");
    if (!fp)
        return 0;

    int in_slub = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "SLUB"))
            in_slub = 1;
        if (in_slub) {
            sscanf(line, "  alloc_count: %lu", slub_alloc);
            sscanf(line, "  free_count:  %lu", slub_free);
        }
    }

    fclose(fp);
    return 0;
}

/* 绘制比例条 */
static void draw_bar(int width, int pct, const char *color)
{
    int filled = (pct * width) / 100;
    int i;

    printf("%s[", color);
    for (i = 0; i < width; i++) {
        if (i < filled)
            printf("#");
        else
            printf("-");
    }
    printf("]%s", COLOR_RESET);
}

/* 根据百分比选择颜色 */
static const char *pct_color(int pct)
{
    if (pct > 80) return COLOR_RED;
    if (pct > 60) return COLOR_YELLOW;
    return COLOR_GREEN;
}

/* overview 命令 */
static int cmd_overview(void)
{
    unsigned long total, free_mem, avail, cached, slab;
    unsigned long buddy_alloc, buddy_free, slub_alloc, slub_free;
    unsigned long numa_local, numa_remote;
    int used_pct, free_pct;
    time_t now = time(NULL);

    if (read_meminfo(&total, &free_mem, &avail, &cached, &slab) != 0) {
        fprintf(stderr, "Error: cannot read /proc/meminfo\n");
        return -1;
    }

    used_pct = total > 0 ? (int)(((total - free_mem) * 100) / total) : 0;
    free_pct = total > 0 ? (int)((free_mem * 100) / total) : 0;

    printf("\n");
    printf(COLOR_BOLD "  SmartMemEngine Overview" COLOR_RESET "  %s", ctime(&now));
    printf("\n");

    /* 内存使用条形图 */
    printf("  Memory Usage:  ");
    draw_bar(40, used_pct, pct_color(used_pct));
    printf(" %d%% used\n", used_pct);

    printf("  Memory Free:   ");
    draw_bar(40, free_pct, pct_color(100 - free_pct));
    printf(" %d%% free\n", free_pct);

    printf("\n");
    printf("  %-16s %10lu kB\n", "Total:", total);
    printf("  %-16s %10lu kB\n", "Free:", free_mem);
    printf("  %-16s %10lu kB\n", "Available:", avail);
    printf("  %-16s %10lu kB\n", "Cached:", cached);
    printf("  %-16s %10lu kB\n", "Slab:", slab);

    /* smartmem 统计 */
    if (read_smartmem_stats(&buddy_alloc, &buddy_free,
                            &slub_alloc, &slub_free,
                            &numa_local, &numa_remote) == 0) {
        int numa_pct = (numa_local + numa_remote > 0) ?
            (int)((numa_local * 100) / (numa_local + numa_remote)) : 100;

        printf("\n");
        printf(COLOR_BOLD "  SmartMem Statistics" COLOR_RESET "\n");
        printf("  %-16s %10lu\n", "Buddy Alloc:", buddy_alloc);
        printf("  %-16s %10lu\n", "Buddy Free:", buddy_free);
        printf("  %-16s %10lu\n", "SLUB Alloc:", slub_alloc);
        printf("  %-16s %10lu\n", "SLUB Free:", slub_free);

        printf("\n");
        printf("  NUMA Locality: ");
        draw_bar(30, numa_pct, numa_pct > 80 ? COLOR_GREEN :
                                  numa_pct > 50 ? COLOR_YELLOW : COLOR_RED);
        printf(" %d%% local\n", numa_pct);
    }

    printf("\n");
    return 0;
}

/* top 命令：实时监控 */
static int cmd_top(int interval)
{
    while (1) {
        /* 清屏 */
        printf("\033[2J\033[H");

        cmd_overview();

        printf("  Refresh: %ds | Press Ctrl+C to exit\n", interval);
        fflush(stdout);

        sleep(interval);
    }

    return 0;
}

/* watch 命令：带时间戳的监控 */
static int cmd_watch(int interval)
{
    unsigned long buddy_alloc_prev = 0, buddy_free_prev = 0;
    int first = 1;

    while (1) {
        unsigned long buddy_alloc, buddy_free, slub_alloc, slub_free;
        unsigned long numa_local, numa_remote;
        unsigned long total, free_mem, avail, cached, slab;
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strlen(ts) - 1] = '\0';  /* 去掉换行 */

        if (read_meminfo(&total, &free_mem, &avail, &cached, &slab) != 0) {
            fprintf(stderr, "[%s] Error: cannot read meminfo\n", ts);
            sleep(interval);
            continue;
        }

        if (read_smartmem_stats(&buddy_alloc, &buddy_free,
                                &slub_alloc, &slub_free,
                                &numa_local, &numa_remote) != 0) {
            fprintf(stderr, "[%s] Error: cannot read smartmem stats\n", ts);
            sleep(interval);
            continue;
        }

        {
            int free_pct = total > 0 ? (int)((free_mem * 100) / total) : 0;
            int numa_pct = (numa_local + numa_remote > 0) ?
                (int)((numa_local * 100) / (numa_local + numa_remote)) : 100;
            long da = first ? 0 : (long)(buddy_alloc - buddy_alloc_prev);
            long df = first ? 0 : (long)(buddy_free - buddy_free_prev);

            printf("[%s] free=%d%% numa=%d%% buddy(+%ld/-%ld)\n",
                   ts, free_pct, numa_pct, da, df);

            buddy_alloc_prev = buddy_alloc;
            buddy_free_prev = buddy_free;
            first = 0;
        }

        fflush(stdout);
        sleep(interval);
    }

    return 0;
}

/* bar 命令：内存分段条形图 */
static int cmd_bar(void)
{
    unsigned long total, free_mem, avail, cached, slab;
    unsigned long anon, buffers, shmem;
    FILE *fp;
    char line[256];

    if (read_meminfo(&total, &free_mem, &avail, &cached, &slab) != 0) {
        fprintf(stderr, "Error: cannot read /proc/meminfo\n");
        return -1;
    }

    /* 读取更多分段数据 */
    anon = buffers = shmem = 0;
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            sscanf(line, "AnonPages: %lu kB", &anon);
            sscanf(line, "Buffers: %lu kB", &buffers);
            sscanf(line, "Shmem: %lu kB", &shmem);
        }
        fclose(fp);
    }

    printf("\n  Memory Layout Bar (total=%lu kB)\n\n", total);

    if (total > 0) {
        int bar_width = 60;
        int free_w = (int)((free_mem * bar_width) / total);
        int anon_w = (int)((anon * bar_width) / total);
        int cache_w = (int)((cached * bar_width) / total);
        int slab_w = (int)((slab * bar_width) / total);
        int buf_w = (int)((buffers * bar_width) / total);
        int used_w, i;

        /* 把整数截断造成的剩余宽度补到 free 段（最大的非"其他"段），
         * 避免出现 '?' 占位字符。*/
        used_w = free_w + anon_w + cache_w + slab_w + buf_w;
        if (used_w < bar_width)
            free_w += (bar_width - used_w);
        else if (used_w > bar_width)
            free_w -= (used_w - bar_width);
        if (free_w < 0)
            free_w = 0;

        printf("  ");
        for (i = 0; i < bar_width; i++) {
            if (i < free_w)
                printf(COLOR_GREEN "F" COLOR_RESET);
            else if (i < free_w + anon_w)
                printf(COLOR_RED "A" COLOR_RESET);
            else if (i < free_w + anon_w + cache_w)
                printf(COLOR_CYAN "C" COLOR_RESET);
            else if (i < free_w + anon_w + cache_w + slab_w)
                printf(COLOR_YELLOW "S" COLOR_RESET);
            else if (i < free_w + anon_w + cache_w + slab_w + buf_w)
                printf(COLOR_BLUE "B" COLOR_RESET);
            else
                printf(COLOR_GREEN "F" COLOR_RESET);
        }

        printf("\n\n");
        printf("  " COLOR_GREEN "F" COLOR_RESET "=Free(%lu kB  %d%%)  ",
               free_mem, (int)((free_mem * 100) / total));
        printf(COLOR_RED "A" COLOR_RESET "=Anon(%lu kB  %d%%)  ",
               anon, (int)((anon * 100) / total));
        printf(COLOR_CYAN "C" COLOR_RESET "=Cache(%lu kB  %d%%)\n",
               cached, (int)((cached * 100) / total));
        printf("  " COLOR_YELLOW "S" COLOR_RESET "=Slab(%lu kB  %d%%)  ",
               slab, (int)((slab * 100) / total));
        printf(COLOR_BLUE "B" COLOR_RESET "=Buffers(%lu kB  %d%%)\n",
               buffers, (int)((buffers * 100) / total));
    }

    printf("\n");
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

    if (strcmp(argv[1], "overview") == 0) {
        return cmd_overview();
    } else if (strcmp(argv[1], "top") == 0) {
        return cmd_top(2);
    } else if (strcmp(argv[1], "watch") == 0) {
        int interval = 5;
        if (argc > 2)
            interval = atoi(argv[2]);
        if (interval < 1)
            interval = 1;
        return cmd_watch(interval);
    } else if (strcmp(argv[1], "bar") == 0) {
        return cmd_bar();
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    } else {
        fprintf(stderr, "Error: unknown command: %s\n", argv[1]);
        usage();
        return 1;
    }
}
