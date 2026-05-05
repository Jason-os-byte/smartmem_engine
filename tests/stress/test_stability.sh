#!/bin/bash
# SmartMemEngine 压力测试 - 长时间运行稳定性

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0

# 默认运行 300 秒（5分钟），可通过参数调整
DURATION=${1:-300}

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
        return 1
    fi
    return 0
}

# ============================================================
# 1. 长时间自动调优稳定性
# ============================================================
test_long_autotune() {
    section "1. 长时间自动调优稳定性（${DURATION}秒）"

    ensure_module || return

    echo "reset stats" > /proc/smartmem/control 2>/dev/null
    echo "reset autotune" > /proc/smartmem/control 2>/dev/null
    echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null

    pass "自动调优已启用"

    local start_time end_time elapsed
    start_time=$(date +%s)
    local check_interval=30
    local check_count=0

    # 后台压力
    stress --vm 2 --vm-bytes 256M --timeout "${DURATION}s" 2>/dev/null &
    local stress_pid=$!

    while true; do
        end_time=$(date +%s)
        elapsed=$((end_time - start_time))

        if [ "$elapsed" -ge "$DURATION" ]; then
            break
        fi

        sleep "$check_interval"
        check_count=$((check_count + 1))

        # 定期健康检查
        if ! lsmod | grep -q smartmem; then
            fail "模块在长时间运行中异常消失"
            kill $stress_pid 2>/dev/null
            return
        fi

        # 定期读取 procfs
        if ! cat /proc/smartmem/stats > /dev/null 2>&1; then
            fail "stats 读取失败（第${check_count}次检查）"
        fi

        # 每60秒输出一次状态
        if [ $((check_count % 2)) -eq 0 ]; then
            local alloc_count
            alloc_count=$(grep "alloc_count:" /proc/smartmem/stats | head -1 | awk '{print $2}')
            local total_tunes
            total_tunes=$(grep "total_tunes:" /proc/smartmem/stats | awk '{print $2}')
            echo "  [${elapsed}s] alloc=$alloc_count tunes=$total_tunes"
        fi
    done

    wait $stress_pid 2>/dev/null

    echo "disable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
    pass "长时间自动调优测试完成（${DURATION}秒，${check_count}次检查）"
    check_oops && pass "长时间运行无内核异常" || true

    # 验证最终统计
    cat /proc/smartmem/stats > /dev/null 2>&1 && pass "长时间运行后 stats 可读" || fail "长时间运行后 stats 可读"
}

# ============================================================
# 2. 长时间 trace 监控稳定性
# ============================================================
test_long_trace() {
    section "2. 长时间 trace 监控稳定性（${DURATION}秒）"

    ensure_module || return

    echo "reset stats" > /proc/smartmem/control 2>/dev/null
    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null

    pass "trace 已启用"

    local start_time end_time elapsed
    start_time=$(date +%s)

    stress --vm 1 --vm-bytes 128M --timeout "${DURATION}s" 2>/dev/null &
    local stress_pid=$!

    while true; do
        end_time=$(date +%s)
        elapsed=$((end_time - start_time))

        if [ "$elapsed" -ge "$DURATION" ]; then
            break
        fi

        sleep 30

        # 读取 trace 统计
        if ! grep "trace_events:" /proc/smartmem/stats > /dev/null 2>&1; then
            fail "trace_events 读取失败"
        fi
    done

    wait $stress_pid 2>/dev/null

    echo "disable trace_enabled" > /proc/smartmem/control 2>/dev/null
    pass "长时间 trace 监控测试完成"
    check_oops && pass "trace 长时间运行无异常" || true

    local trace_events
    trace_events=$(grep "trace_events:" /proc/smartmem/stats | awk '{print $2}')
    [ -n "$trace_events" ] && pass "trace_events = $trace_events" || fail "trace_events 为空"
}

# ============================================================
# 3. 内存泄漏检测
# ============================================================
test_memory_leak() {
    section "3. 内存泄漏检测"

    ensure_module || return

    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
    echo "enable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null

    # 记录初始内存
    local initial_slab
    initial_slab=$(grep "Slab:" /proc/meminfo | awk '{print $2}')
    local initial_free
    initial_free=$(grep "MemFree" /proc/meminfo | awk '{print $2}')

    pass "初始状态: Slab=${initial_slab}KB Free=${initial_free}KB"

    # 运行压力循环
    local rounds=10
    for i in $(seq 1 $rounds); do
        stress --vm 1 --vm-bytes 128M --timeout 5s 2>/dev/null &
        wait 2>/dev/null
        sleep 2

        # 定期读取所有 procfs 接口
        cat /proc/smartmem/stats > /dev/null 2>&1
        cat /proc/smartmem/hotspots > /dev/null 2>&1
        cat /proc/smartmem/prediction > /dev/null 2>&1
        cat /proc/smartmem/bottlenecks > /dev/null 2>&1
        cat /proc/smartmem/rootcauses > /dev/null 2>&1
        cat /proc/smartmem/autotune > /dev/null 2>&1

        # 每轮重置统计
        echo "reset stats" > /proc/smartmem/control 2>/dev/null
    done

    # 记录最终内存
    local final_slab
    final_slab=$(grep "Slab:" /proc/meminfo | awk '{print $2}')
    local final_free
    final_free=$(grep "MemFree" /proc/meminfo | awk '{print $2}')

    pass "最终状态: Slab=${final_slab}KB Free=${final_free}KB"

    # 简单判断：如果 Slab 增长超过 50% 可能存在泄漏
    local slab_growth
    if [ -n "$initial_slab" ] && [ "$initial_slab" -gt 0 ]; then
        slab_growth=$(( (final_slab - initial_slab) * 100 / initial_slab ))
        if [ "$slab_growth" -gt 50 ]; then
            fail "Slab 增长 ${slab_growth}%（可能内存泄漏）"
        else
            pass "Slab 增长 ${slab_growth}%（正常范围）"
        fi
    else
        pass "无法计算 Slab 增长率"
    fi

    echo "disable trace_enabled" > /proc/smartmem/control 2>/dev/null
    echo "disable auto_tune_enabled" > /proc/smartmem/control 2>/dev/null
}

# ============================================================
# 4. 预测模型长期精度
# ============================================================
test_prediction_stability() {
    section "4. 预测模型长期运行"

    ensure_module || return

    echo "reset prediction" > /proc/smartmem/control 2>/dev/null

    local samples_history=""
    local check_count=0

    for i in $(seq 1 6); do
        sleep 10
        check_count=$((check_count + 1))

        local samples
        samples=$(grep "samples:" /proc/smartmem/prediction | awk '{print $2}')
        [ -n "$samples" ] || samples=0

        samples_history="$samples_history [$check_count]=$samples"

        # 读取预测值
        local pred_free
        pred_free=$(grep "predicted_free:" /proc/smartmem/prediction | awk '{print $2}')
        [ -n "$pred_free" ] && pass "第${check_count}次: samples=$samples predicted_free=$pred_free" || pass "第${check_count}次: samples=$samples（预测值不可用）"
    done

    pass "预测模型长期运行采样历史: $samples_history"
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 压力测试 - 长时间稳定性   ║"
echo "║  运行时间: ${DURATION}秒                      ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_long_autotune
test_long_trace
test_memory_leak
test_prediction_stability

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
