#!/bin/bash
# SmartMemEngine 压力测试 - 反复加载/卸载压力

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0

# 默认循环次数，可通过参数调整
ROUNDS=${1:-20}

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} [$PASS]: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} [$FAIL]: $1"; }

section() { echo ""; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; echo "  $1"; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; }

check_oops() {
    # 使用 -E 扩展正则 + 单词边界，避免 "debugfs" 中的 "bug" 子串误判
    # 内核异常关键字: Oops, kernel panic, kernel BUG, "Call Trace:", WARNING:
    if dmesg | tail -30 | grep -qE "Oops|[Kk]ernel panic|kernel BUG|Call Trace:|WARNING:"; then
        fail "dmesg 检测到内核异常"
        return 1
    fi
    return 0
}

# ============================================================
# 1. 基本循环加载/卸载
# ============================================================
test_basic_reload() {
    section "1. 基本循环加载/卸载（${ROUNDS}次）"

    local success=0
    for i in $(seq 1 $ROUNDS); do
        rmmod smartmem 2>/dev/null
        sleep 0.5
        insmod "$MODULE_PATH" 2>/dev/null || { fail "第${i}次 insmod 失败"; continue; }
        lsmod | grep -q smartmem || { fail "第${i}次 lsmod 不可见"; continue; }
        [ -d "/proc/smartmem" ] || { fail "第${i}次 /proc/smartmem 不存在"; continue; }
        success=$((success + 1))
    done

    pass "基本循环: ${success}/${ROUNDS} 次成功"

    # 最终卸载
    rmmod smartmem 2>/dev/null; sleep 1
    check_oops && pass "基本循环无内核异常" || true
}

# ============================================================
# 2. 功能启用状态循环加载/卸载
# ============================================================
test_reload_with_features() {
    section "2. 功能启用状态循环加载/卸载（${ROUNDS}次）"

    local success=0
    for i in $(seq 1 $ROUNDS); do
        rmmod smartmem 2>/dev/null
        sleep 0.5
        insmod "$MODULE_PATH" 2>/dev/null || { fail "第${i}次 insmod 失败"; continue; }
        sleep 0.5

        # 启用功能
        echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
        echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null

        # 读取数据
        cat /proc/smartmem/stats > /dev/null 2>&1
        cat /proc/smartmem/config > /dev/null 2>&1
        cat /proc/smartmem/hotspots > /dev/null 2>&1

        success=$((success + 1))
    done

    pass "功能启用循环: ${success}/${ROUNDS} 次成功"

    rmmod smartmem 2>/dev/null; sleep 1
    check_oops && pass "功能启用循环无内核异常" || true
}

# ============================================================
# 3. 快速连续加载/卸载
# ============================================================
test_rapid_reload() {
    section "3. 快速连续加载/卸载（10次，无间隔）"

    local success=0
    for i in $(seq 1 10); do
        rmmod smartmem 2>/dev/null
        insmod "$MODULE_PATH" 2>/dev/null || { fail "快速第${i}次 insmod 失败"; continue; }
        lsmod | grep -q smartmem && success=$((success + 1)) || true
    done

    pass "快速循环: ${success}/10 次成功"

    rmmod smartmem 2>/dev/null; sleep 1
    check_oops && pass "快速循环无内核异常" || true
}

# ============================================================
# 4. 压力期间加载/卸载
# ============================================================
test_reload_under_pressure() {
    section "4. 压力期间加载/卸载"

    # 启动持续内存压力
    stress --vm 2 --vm-bytes 256M --timeout 60s 2>/dev/null &
    local stress_pid=$!

    local success=0
    for i in $(seq 1 10); do
        rmmod smartmem 2>/dev/null
        sleep 1
        insmod "$MODULE_PATH" 2>/dev/null || { fail "压力下第${i}次 insmod 失败"; continue; }
        sleep 1

        # 验证功能正常
        cat /proc/smartmem/stats > /dev/null 2>&1 || { fail "压力下第${i}次 stats 不可读"; continue; }
        echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
        cat /proc/smartmem/stats > /dev/null 2>&1

        success=$((success + 1))
    done

    kill $stress_pid 2>/dev/null
    wait $stress_pid 2>/dev/null

    pass "压力下循环: ${success}/10 次成功"

    rmmod smartmem 2>/dev/null; sleep 1
    check_oops && pass "压力下循环无内核异常" || true
}

# ============================================================
# 5. 加载/卸载后资源泄漏检查
# ============================================================
test_reload_leak_check() {
    section "5. 加载/卸载后资源泄漏检查"

    # 记录初始内存
    local initial_free
    initial_free=$(grep "MemFree" /proc/meminfo | awk '{print $2}')

    # 20 次加载/卸载循环
    for i in $(seq 1 20); do
        insmod "$MODULE_PATH" 2>/dev/null
        sleep 0.5

        # 启用功能产生数据
        echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
        echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
        sleep 1

        # 读取所有接口
        for file in config stats hotspots bottlenecks rootcauses autotune prediction policies; do
            cat "/proc/smartmem/$file" > /dev/null 2>&1
        done

        rmmod smartmem 2>/dev/null
        sleep 0.5
    done

    # 记录最终内存
    local final_free
    final_free=$(grep "MemFree" /proc/meminfo | awk '{print $2}')

    pass "内存变化: Free ${initial_free}KB -> ${final_free}KB"

    local diff
    diff=$((final_free - initial_free))
    if [ "$diff" -lt -524288 ]; then
        fail "内存减少超过 512MB（可能存在泄漏: ${diff}KB）"
    else
        pass "内存变化在正常范围（${diff}KB）"
    fi

    # 检查 procfs/debugfs 已完全清理
    [ ! -d "/proc/smartmem" ] && pass "procfs 已清理" || fail "procfs 残留"
    [ ! -d "/sys/kernel/debug/smartmem" ] && pass "debugfs 已清理" || fail "debugfs 残留"

    check_oops && pass "资源泄漏检查无内核异常" || true
}

# ============================================================
# 6. procfs 并发访问 + 加载/卸载
# ============================================================
test_concurrent_reload() {
    section "6. 并发 procfs 读取 + 加载/卸载"

    # 启动后台读取
    (
        for j in $(seq 1 50); do
            cat /proc/smartmem/stats > /dev/null 2>&1
            cat /proc/smartmem/config > /dev/null 2>&1
            sleep 0.2
        done
    ) &
    local reader_pid=$!

    # 同时进行加载/卸载
    for i in $(seq 1 5); do
        rmmod smartmem 2>/dev/null
        sleep 1
        insmod "$MODULE_PATH" 2>/dev/null
        sleep 1
    done

    wait $reader_pid 2>/dev/null

    pass "并发读写 + 加载/卸载完成"
    check_oops && pass "并发测试无内核异常" || true

    # 确保模块最终处于干净状态
    rmmod smartmem 2>/dev/null; sleep 1
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 压力测试 - 反复加载/卸载   ║"
echo "║  循环次数: ${ROUNDS}次                          ║"
echo "╚══════════════════════════════════════════╝"

test_basic_reload
test_reload_with_features
test_rapid_reload
test_reload_under_pressure
test_reload_leak_check
test_concurrent_reload

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  测试结果汇总"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TOTAL=$((PASS + FAIL))
echo "  总计: $TOTAL"
echo -e "  ${GREEN}通过: $PASS${NC}"
echo -e "  ${RED}失败: $FAIL${NC}"
echo ""

[ $FAIL -eq 0 ] && echo -e "${GREEN}全部通过!${NC}" || echo -e "${RED}存在失败用例!${NC}"
exit $FAIL
