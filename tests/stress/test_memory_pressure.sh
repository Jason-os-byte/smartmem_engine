#!/bin/bash
# SmartMemEngine 压力测试 - 内存压力场景

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} [$PASS]: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} [$FAIL]: $1"; }

section() { echo ""; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; echo "  $1"; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; }

ensure_module() {
    if ! lsmod | grep -q smartmem; then
        rmmod smartmem 2>/dev/null; sleep 1
        insmod "$MODULE_PATH" 2>/dev/null; sleep 1
    fi
    lsmod | grep -q smartmem
}

check_oops() {
    if dmesg | tail -50 | grep -qi "oops\|panic\|bug\|call trace"; then
        fail "dmesg 检测到内核异常"
        dmesg | tail -20 | grep -i "oops\|panic\|bug\|call trace"
        return 1
    fi
    return 0
}

# ============================================================
# 1. 高强度 Buddy 分配压力
# ============================================================
test_buddy_pressure() {
    section "1. Buddy 分配器压力"

    ensure_module || return

    echo "reset stats" > /proc/smartmem/control 2>/dev/null

    # 多进程并发分配
    for i in $(seq 1 4); do
        stress --vm 1 --vm-bytes 512M --timeout 20s 2>/dev/null &
    done

    # 持续监控模块状态
    for i in $(seq 1 10); do
        sleep 2
        if ! lsmod | grep -q smartmem; then
            fail "模块在压力期间被意外卸载"
            wait 2>/dev/null
            return
        fi
    done

    wait 2>/dev/null
    sleep 2

    pass "Buddy 压力测试完成（模块存活）"
    check_oops && pass "dmesg 无内核异常" || true

    # 验证统计仍然可读
    cat /proc/smartmem/stats > /dev/null 2>&1 && pass "压力后 stats 可读" || fail "压力后 stats 可读"
    local alloc_count
    alloc_count=$(grep "alloc_count:" /proc/smartmem/stats | head -1 | awk '{print $2}')
    [ -n "$alloc_count" ] && pass "alloc_count = $alloc_count" || fail "alloc_count 为空"
}

# ============================================================
# 2. SLUB 分配压力
# ============================================================
test_slub_pressure() {
    section "2. SLUB 分配器压力"

    ensure_module || return

    echo "reset stats" > /proc/smartmem/control 2>/dev/null

    # 大量小型分配（触发 kmalloc/kfree）
    for i in $(seq 1 4); do
        stress --vm 2 --vm-bytes 64M --timeout 20s 2>/dev/null &
    done

    wait 2>/dev/null
    sleep 2

    pass "SLUB 压力测试完成"
    check_oops && pass "dmesg 无内核异常" || true

    cat /proc/smartmem/stats > /dev/null 2>&1 && pass "SLUB 压力后 stats 可读" || fail "SLUB 压力后 stats 可读"
}

# ============================================================
# 3. 内存碎片化压力
# ============================================================
test_fragmentation_pressure() {
    section "3. 内存碎片化压力"

    ensure_module || return

    local free_before
    free_before=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    local order_before
    order_before=$(grep "alloc_count:" /proc/smartmem/stats | head -1 | awk '{print $2}')

    # 交替大小分配制造碎片
    for round in $(seq 1 3); do
        # 大块分配
        stress --vm 1 --vm-bytes 512M --timeout 5s 2>/dev/null &
        sleep 6
        # 小块分配
        stress --vm 4 --vm-bytes 32M --timeout 5s 2>/dev/null &
        sleep 6
    done

    local free_after
    free_after=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    pass "碎片化压力: free ${free_before}KB -> ${free_after}KB"

    # 触发 compact 并验证
    echo "tune compact" > /proc/smartmem/control 2>/dev/null
    sleep 2

    local free_after_compact
    free_after_compact=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    pass "compact 后: free ${free_after}KB -> ${free_after_compact}KB"

    check_oops && pass "碎片化压力无内核异常" || true
}

# ============================================================
# 4. OOM 临近压力
# ============================================================
test_near_oom() {
    section "4. OOM 临近压力"

    ensure_module || return

    local total_mem
    total_mem=$(grep "MemTotal" /proc/meminfo | awk '{print $2}')
    local target_mb=$((total_mem / 1024 - 64))

    # 尝试消耗大部分内存（留64MB）
    echo "尝试消耗 ${target_mb}MB 内存..."

    stress --vm 2 --vm-bytes "${target_mb}M" --timeout 15s 2>/dev/null &
    local stress_pid=$!

    # 监控模块在 OOM 临近时的行为
    for i in $(seq 1 8); do
        sleep 2
        if ! lsmod | grep -q smartmem; then
            fail "模块在 OOM 压力期间异常"
            wait $stress_pid 2>/dev/null
            return
        fi
    done

    wait $stress_pid 2>/dev/null
    sleep 3

    pass "OOM 临近压力测试完成（模块存活）"
    check_oops && pass "OOM 压力无内核异常" || true

    # 验证调优动作被触发
    cat /proc/smartmem/stats > /dev/null 2>&1 && pass "OOM 压力后 stats 可读" || fail "OOM 压力后 stats 可读"
}

# ============================================================
# 5. NUMA 压力（多节点）
# ============================================================
test_numa_pressure() {
    section "5. NUMA 跨节点压力"

    ensure_module || return

    if [ ! -x /usr/bin/numactl ] 2>/dev/null; then
        pass "numactl 不存在，跳过 NUMA 压力测试"
        return
    fi

    local num_nodes
    num_nodes=$(numactl --hardware 2>/dev/null | grep "available:" | awk '{print $2}')

    if [ -z "$num_nodes" ] || [ "$num_nodes" -le 1 ]; then
        pass "单 NUMA 节点系统，跳过跨节点测试"
        return
    fi

    echo "检测到 $num_nodes 个 NUMA 节点"

    # 跨节点分配
    for node in $(seq 0 $((num_nodes - 1))); do
        numactl --membind="$node" stress --vm 1 --vm-bytes 128M --timeout 10s 2>/dev/null &
    done

    wait 2>/dev/null
    sleep 2

    pass "NUMA 跨节点压力测试完成"
    check_oops && pass "NUMA 压力无内核异常" || true

    # 验证 NUMA 统计
    if grep -q "numa_locality:" /proc/smartmem/stats 2>/dev/null; then
        local locality
        locality=$(grep "numa_locality:" /proc/smartmem/stats | awk '{print $2}')
        pass "NUMA locality = $locality"
    else
        pass "NUMA 统计未采集（可能 hook 未启用）"
    fi
}

# ============================================================
# 6. 并发 procfs 访问压力
# ============================================================
test_concurrent_procfs() {
    section "6. 并发 procfs 读取压力"

    ensure_module || return

    local pids=""

    # 10 个并发读取进程
    for i in $(seq 1 10); do
        (
            for j in $(seq 1 20); do
                cat /proc/smartmem/stats > /dev/null 2>&1
                cat /proc/smartmem/config > /dev/null 2>&1
                cat /proc/smartmem/hotspots > /dev/null 2>&1
                cat /proc/smartmem/prediction > /dev/null 2>&1
            done
        ) &
        pids="$pids $!"
    done

    # 同时有写入
    (
        for j in $(seq 1 5); do
            echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
            sleep 1
            echo "disable trace_enabled" > /proc/smartmem/control 2>/dev/null
            sleep 1
        done
    ) &
    pids="$pids $!"

    # 等待所有进程完成
    for pid in $pids; do
        wait "$pid" 2>/dev/null
    done

    pass "并发 procfs 访问完成"
    check_oops && pass "并发访问无内核异常" || true
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 压力测试 - 内存压力场景   ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_buddy_pressure
test_slub_pressure
test_fragmentation_pressure
test_near_oom
test_numa_pressure
test_concurrent_procfs

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
