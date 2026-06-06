/* memctl - SmartMemEngine 配置管理工具 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PROC_BASE   "/proc/smartmem"
#define CONFIG_FILE PROC_BASE "/config"
#define CONTROL_FILE PROC_BASE "/control"

static void usage(void)
{
    printf("Usage: memctl <command> [args]\n\n");
    printf("Configuration commands:\n");
    printf("  memctl config                    Show all configuration\n");
    printf("  memctl config get <key>          Get a config value\n");
    printf("  memctl config set <key>=<value>  Set a config value\n\n");
    printf("Control commands:\n");
    printf("  memctl enable <feature>          Enable a feature\n");
    printf("  memctl disable <feature>         Disable a feature\n");
    printf("  memctl reset <target>            Reset (stats/hotspots/autotune/prediction)\n");
    printf("  memctl tune <action>             Trigger tune (compact/watermark/numa/slab)\n\n");
    printf("Features: hook_buddy_enabled, hook_slub_enabled, hook_vma_enabled,\n");
    printf("          hook_lru_enabled, hook_numa_enabled, numa_aware_enabled,\n");
    printf("          adaptive_slub_enabled, trace_enabled,\n");
    printf("          auto_tune_enabled\n");
}

/* 读取 proc 文件内容并输出 */
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

/* 写入 proc 文件 */
static int write_proc_file(const char *path, const char *data)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fprintf(fp, "%s", data) < 0) {
        fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/* config 子命令 */
static int cmd_config(int argc, char *argv[])
{
    if (argc < 1) {
        /* memctl config - 显示所有配置 */
        return read_proc_file(CONFIG_FILE);
    }

    if (strcmp(argv[0], "get") == 0) {
        /* memctl config get <key> - 从配置文件中过滤显示 */
        FILE *fp;
        char line[256];
        char *key = argc > 1 ? argv[1] : NULL;

        fp = fopen(CONFIG_FILE, "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open %s: %s\n",
                    CONFIG_FILE, strerror(errno));
            return -1;
        }

        while (fgets(line, sizeof(line), fp)) {
            if (key) {
                /* 只显示匹配 key 的行 */
                if (strncmp(line, key, strlen(key)) == 0)
                    printf("%s", line);
            } else {
                printf("%s", line);
            }
        }

        fclose(fp);
        return 0;
    }

    if (strcmp(argv[0], "set") == 0 && argc > 1) {
        /* memctl config set key=value */
        return write_proc_file(CONFIG_FILE, argv[1]);
    }

    fprintf(stderr, "Error: unknown config subcommand: %s\n", argv[0]);
    return -1;
}

/* enable/disable 子命令 */
static int cmd_enable_disable(const char *action, const char *feature)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "%s %s", action, feature);
    return write_proc_file(CONTROL_FILE, cmd);
}

/* reset 子命令 */
static int cmd_reset(const char *target)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "reset %s", target);
    return write_proc_file(CONTROL_FILE, cmd);
}

/* tune 子命令 */
static int cmd_tune(const char *action)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "tune %s", action);
    return write_proc_file(CONTROL_FILE, cmd);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    /* 检查模块是否加载 */
    if (access(PROC_BASE, F_OK) != 0) {
        fprintf(stderr, "Error: smartmem module not loaded (%s not found)\n",
                PROC_BASE);
        return 1;
    }

    if (strcmp(argv[1], "config") == 0) {
        return cmd_config(argc - 2, argv + 2);
    } else if (strcmp(argv[1], "enable") == 0 && argc > 2) {
        return cmd_enable_disable("enable", argv[2]);
    } else if (strcmp(argv[1], "disable") == 0 && argc > 2) {
        return cmd_enable_disable("disable", argv[2]);
    } else if (strcmp(argv[1], "reset") == 0 && argc > 2) {
        return cmd_reset(argv[2]);
    } else if (strcmp(argv[1], "tune") == 0 && argc > 2) {
        return cmd_tune(argv[2]);
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    } else {
        fprintf(stderr, "Error: unknown command: %s\n", argv[1]);
        usage();
        return 1;
    }
}
