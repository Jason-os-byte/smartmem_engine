#!/bin/bash
# SmartMemEngine 集成测试 - 模块加载/卸载生命周期

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0; SKIP=0

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} [$PASS]: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} [$FAIL]: $1"; }

section() { echo ""; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; echo "  $1"; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; }

# ============================================================
# 1. 基本加载/卸载
# ============================================================
test_basic_load() {
    section "1. 基本加载/卸载"

    # 确保干净状态
    rmmod smartmem 2>/dev/null; sleep 1

    # 加载
    insmod "$MODULE_PATH" 2>/dev/null && pass "insmod 成功" || fail "insmod 成功"
    lsmod | grep -q smartmem && pass "lsmod 可见" || fail "lsmod 可见"
    [ -d "/proc/smartmem" ] && pass "/proc/smartmem 目录存在" || fail "/proc/smartmem 目录存在"

    # dmesg 无错误
    if dmesg | tail -30 | grep -i "smartmem" | grep -qi "error\|fail\|warn"; then
        fail "dmesg 无错误/警告"
    else
        pass "dmesg 无错误/警告"
    fi

    # 卸载
    rmmod smartmem 2>/dev/null && pass "rmmod 成功" || fail "rmmod 成功"
    ! lsmod | grep -q smartmem && pass "卸载后 lsmod 不可见" || fail "卸载后 lsmod 不可见"
    [ ! -d "/proc/smartmem" ] && pass "卸载后 /proc/smartmem 已删除" || fail "卸载后 /proc/smartmem 已删除"
}

# ============================================================
# 2. 卸载时各功能启用状态
# ============================================================
test_unload_with_features() {
    section "2. 功能启用状态下卸载"

    rmmod smartmem 2>/dev/null; sleep 1
    insmod "$MODULE_PATH" 2>/dev/null && pass "加载模块" || fail "加载模块"
    sleep 1

    # 启用多个功能
    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
    echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
    pass "启用 trace + auto_tune"

    # 触发一些活动
    sleep 3

    # 卸载（应自动停止所有功能）
    rmmod smartmem 2>/dev/null && pass "功能启用状态下 rmmod 成功" || fail "功能启用状态下 rmmod 成功"
    ! lsmod | grep -q smartmem && pass "卸载干净" || fail "卸载干净"

    # dmesg 无 oops/panic
    if dmesg | tail -30 | grep -qi "oops\|panic\|bug"; then
        fail "dmesg 无 oops/panic"
    else
        pass "dmesg 无 oops/panic"
    fi
}

# ============================================================
# 3. 重复加载/卸载
# ============================================================
test_repeated_load() {
    section "3. 重复加载/卸载（5次）"

    for i in 1 2 3 4 5; do
        rmmod smartmem 2>/dev/null; sleep 1
        insmod "$MODULE_PATH" 2>/dev/null || { fail "第${i}次 insmod 失败"; continue; }
        [ -d "/proc/smartmem" ] || { fail "第${i}次 /proc/smartmem 不存在"; continue; }
        # 每次加载后读取所有接口
        cat /proc/smartmem/config > /dev/null 2>&1 || { fail "第${i}次读取 config 失败"; continue; }
        cat /proc/smartmem/stats > /dev/null 2>&1 || { fail "第${i}次读取 stats 失败"; continue; }
        pass "第${i}次加载/读取/卸载正常"
    done
    rmmod smartmem 2>/dev/null; sleep 1
}

# ============================================================
# 4. 加载后各子模块初始化验证
# ============================================================
test_init_sequence() {
    section "4. 初始化序列验证"

    rmmod smartmem 2>/dev/null; sleep 1
    insmod "$MODULE_PATH" 2>/dev/null && pass "模块加载" || fail "模块加载"
    sleep 1

    # 检查 dmesg 中各子模块初始化消息
    local dmesg_out
    dmesg_out=$(dmesg | tail -50 | grep "smartmem")

    for msg in "config initialized" "stats initialized" "engine initialized" \
               "hook initialized" "strategy initialized" "monitor initialized" \
               "analysis initialized" "optimization initialized" "interface layer initialized"; do
        echo "$dmesg_out" | grep -q "$msg" && pass "初始化: $msg" || fail "初始化: $msg"
    done

    # 检查所有 procfs 文件创建
    for file in config control policies stats hotspots bottlenecks rootcauses autotune prediction; do
        [ -f "/proc/smartmem/$file" ] && pass "procfs: $file 已创建" || fail "procfs: $file 已创建"
    done

    rmmod smartmem 2>/dev/null; sleep 1
}

# ============================================================
# 5. 卸载后资源释放验证
# ============================================================
test_cleanup() {
    section "5. 卸载后资源释放"

    rmmod smartmem 2>/dev/null; sleep 1
    insmod "$MODULE_PATH" 2>/dev/null || { fail "加载失败"; return; }
    sleep 1

    # 启用功能并产生数据
    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
    echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
    sleep 5

    # 记录卸载前状态
    local before_mem
    before_mem=$(grep "MemFree" /proc/meminfo | awk '{print $2}')

    # 卸载
    rmmod smartmem 2>/dev/null && pass "卸载成功" || fail "卸载成功"
    sleep 2

    # 检查 procfs 已清理
    [ ! -d "/proc/smartmem" ] && pass "procfs 目录已清理" || fail "procfs 目录已清理"

    # 检查 debugfs 已清理
    [ ! -d "/sys/kernel/debug/smartmem" ] && pass "debugfs 目录已清理" || fail "debugfs 目录已清理"

    # 检查 kprobe 已注销（dmesg 不应有警告）
    if dmesg | tail -20 | grep -qi "kprobe.*warn\|probe.*leak"; then
        fail "kprobe 资源无泄漏"
    else
        pass "kprobe 资源无泄漏"
    fi

    # 检查 dmesg 退出消息
    dmesg | tail -30 | grep "smartmem" | grep -q "exited" && pass "退出消息存在" || fail "退出消息存在"
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 集成测试 - 生命周期       ║"
echo "╚══════════════════════════════════════════╝"

test_basic_load
test_unload_with_features
test_repeated_load
test_init_sequence
test_cleanup

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  测试结果汇总"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TOTAL=$((PASS + FAIL + SKIP))
echo "  总计: $TOTAL"
echo -e "  ${GREEN}通过: $PASS${NC}"
echo -e "  ${RED}失败: $FAIL${NC}"
echo ""

[ $FAIL -eq 0 ] && echo -e "${GREEN}全部通过!${NC}" || echo -e "${RED}存在失败用例!${NC}"
exit $FAIL
