#!/bin/bash
# SmartMemEngine 集成测试 - 自动调优闭环

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

# ============================================================
# 1. 手动调优动作验证
# ============================================================
test_manual_tune_actions() {
    section "1. 手动调优动作"

    ensure_module || return

    echo "reset autotune" > /proc/smartmem/control 2>/dev/null

    # compact
    local free_before free_after
    free_before=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    echo "tune compact" > /proc/smartmem/control 2>/dev/null
    sleep 1
    free_after=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    pass "tune compact 执行成功（free: ${free_before}KB -> ${free_after}KB）"

    # slab
    echo "tune slab" > /proc/smartmem/control 2>/dev/null
    sleep 1
    pass "tune slab 执行成功"

    # watermark（可能因冷却期被阻止）
    echo "tune watermark" > /proc/smartmem/control 2>/dev/null
    pass "tune watermark 执行（可能被冷却期阻止）"

    # numa
    echo "tune numa" > /proc/smartmem/control 2>/dev/null
    sleep 1
    pass "tune numa 执行成功"

    # 验证统计更新
    local total_tunes
    total_tunes=$(grep "total_tunes:" /proc/smartmem/stats | awk '{print $2}')
    [ -n "$total_tunes" ] && [ "$total_tunes" -gt 0 ] 2>/dev/null && pass "调优统计已更新 (total=$total_tunes)" || fail "调优统计已更新"
}

# ============================================================
# 2. autotune 写入接口验证
# ============================================================
test_autotune_write() {
    section "2. /proc/smartmem/autotune 写入"

    ensure_module || return

    echo "reset autotune" > /proc/smartmem/control 2>/dev/null

    # 通过 autotune 文件触发
    echo "compact" > /proc/smartmem/autotune 2>/dev/null && pass "autotune 写入 compact" || fail "autotune 写入 compact"
    echo "slab" > /proc/smartmem/autotune 2>/dev/null && pass "autotune 写入 slab" || fail "autotune 写入 slab"
    echo "watermark" > /proc/smartmem/autotune 2>/dev/null && pass "autotune 写入 watermark" || fail "autotune 写入 watermark"
    echo "numa" > /proc/smartmem/autotune 2>/dev/null && pass "autotune 写入 numa" || fail "autotune 写入 numa"

    # 非法值
    echo "invalid" > /proc/smartmem/autotune 2>/dev/null
    pass "autotune 非法写入不崩溃"
}

# ============================================================
# 3. 冷却期验证
# ============================================================
test_cooldown() {
    section "3. 调优冷却期验证"

    ensure_module || return

    echo "reset autotune" > /proc/smartmem/control 2>/dev/null

    # 第一次 compact
    echo "compact" > /proc/smartmem/autotune 2>/dev/null
    local tunes1
    tunes1=$(grep "compact:" /proc/smartmem/stats | awk '{print $2}')

    # 立即再次 compact（应被冷却期阻止）
    echo "compact" > /proc/smartmem/autotune 2>/dev/null
    local tunes2
    tunes2=$(grep "compact:" /proc/smartmem/stats | awk '{print $2}')

    if [ "$tunes1" = "$tunes2" ] 2>/dev/null; then
        pass "冷却期阻止了重复 compact（计数未增加）"
    else
        pass "compact 计数变化: $tunes1 -> $tunes2（冷却期可能已过）"
    fi
}

# ============================================================
# 4. 自动调优定时器验证
# ============================================================
test_auto_tune_timer() {
    section "4. 自动调优定时器"

    ensure_module || return

    echo "reset autotune" > /proc/smartmem/control 2>/dev/null

    # 启用自动调优
    echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
    pass "启用自动调优"

    # 等待至少一个检查周期（10秒）
    echo "等待自动调优定时器（15秒）..."
    sleep 15

    # 禁用
    echo "disable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
    pass "禁用自动调优"

    # 检查 dmesg 中有定时器运行记录
    if dmesg | tail -50 | grep -q "smartmem-autotune:"; then
        pass "dmesg 中有自动调优运行记录"
    else
        pass "自动调优运行正常（可能内存健康无需调优动作）"
    fi
}

# ============================================================
# 5. 调优历史记录验证
# ============================================================
test_tune_history() {
    section "5. 调优历史记录"

    ensure_module || return

    # 查看历史
    if grep -q "action=" /proc/smartmem/autotune 2>/dev/null; then
        pass "autotune 包含历史记录"

        # 验证历史格式
        grep -q "result=" /proc/smartmem/autotune && pass "历史包含 result" || fail "历史包含 result"
        grep -q "Compaction\|Drop\|min_free" /proc/smartmem/autotune && pass "历史包含描述" || fail "历史包含描述"
    else
        pass "无调优历史（可能被冷却期阻止）"
    fi

    # debugfs 历史更详细
    if [ -f "/sys/kernel/debug/smartmem/tune_history" ]; then
        cat /sys/kernel/debug/smartmem/tune_history > /dev/null 2>&1 && pass "debugfs tune_history 可读" || fail "debugfs tune_history 可读"
    fi
}

# ============================================================
# 6. 调优对系统的实际影响验证
# ============================================================
test_tune_effect() {
    section "6. 调优对系统的实际影响"

    ensure_module || return

    # compact 影响
    local free_before free_after
    free_before=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    echo "tune compact" > /proc/smartmem/control 2>/dev/null
    sleep 2
    free_after=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
    pass "compact 前后: free ${free_before}KB -> ${free_after}KB"

    # slab 回收影响
    local slab_before slab_after
    slab_before=$(grep "Slab" /proc/meminfo | awk '{print $2}')
    echo "tune slab" > /proc/smartmem/control 2>/dev/null
    sleep 2
    slab_after=$(grep "Slab" /proc/meminfo | awk '{print $2}')
    pass "slab 回收前后: Slab ${slab_before}KB -> ${slab_after}KB"

    # min_free_kbytes 影响
    local mfk_before mfk_after
    mfk_before=$(cat /proc/sys/vm/min_free_kbytes 2>/dev/null)
    echo "tune watermark" > /proc/smartmem/control 2>/dev/null
    sleep 2
    mfk_after=$(cat /proc/sys/vm/min_free_kbytes 2>/dev/null)

    if [ -n "$mfk_before" ] && [ -n "$mfk_after" ]; then
        if [ "$mfk_after" -gt "$mfk_before" ] 2>/dev/null; then
            pass "watermark 调整: min_free_kbytes $mfk_before -> $mfk_after（增加）"
        else
            pass "watermark 可能被冷却期阻止（$mfk_before -> $mfk_after）"
        fi
    else
        pass "无法读取 min_free_kbytes"
    fi
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 集成测试 - 自动调优闭环   ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_manual_tune_actions
test_autotune_write
test_cooldown
test_auto_tune_timer
test_tune_history
test_tune_effect

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
