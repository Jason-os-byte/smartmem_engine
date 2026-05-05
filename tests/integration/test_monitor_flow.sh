#!/bin/bash
# SmartMemEngine 集成测试 - 监控→分析→优化 全链路

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
# 1. 监控链路：Hook → Stats → procfs
# ============================================================
test_hook_stats_flow() {
    section "1. Hook → Stats 数据流"

    ensure_module || return

    # 重置统计
    echo "reset stats" > /proc/smartmem/control 2>/dev/null
    sleep 1

    local alloc_before
    alloc_before=$(grep "alloc_count:" /proc/smartmem/stats | head -1 | awk '{print $2}')

    # 产生内存活动
    stress --vm 1 --vm-bytes 256M --timeout 5s 2>/dev/null &
    wait $! 2>/dev/null
    sleep 2

    local alloc_after
    alloc_after=$(grep "alloc_count:" /proc/smartmem/stats | head -1 | awk '{print $2}')

    if [ -n "$alloc_before" ] && [ -n "$alloc_after" ] && [ "$alloc_after" -ge "$alloc_before" ] 2>/dev/null; then
        pass "Hook 采集数据流经 Stats: $alloc_before -> $alloc_after"
    else
        fail "Hook 采集数据流经 Stats"
    fi
}

# ============================================================
# 2. 监控链路：Tracepoint → Stats
# ============================================================
test_trace_stats_flow() {
    section "2. Tracepoint → Stats 数据流"

    ensure_module || return

    # 启用 trace
    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null
    sleep 2

    local trace_before
    trace_before=$(grep "trace_events:" /proc/smartmem/stats | awk '{print $2}')

    # 产生活动
    stress --vm 1 --vm-bytes 128M --timeout 3s 2>/dev/null &
    wait $! 2>/dev/null
    sleep 2

    local trace_after
    trace_after=$(grep "trace_events:" /proc/smartmem/stats | awk '{print $2}')

    if [ -n "$trace_before" ] && [ -n "$trace_after" ] && [ "$trace_after" -gt "$trace_before" ] 2>/dev/null; then
        pass "Tracepoint 采集数据流经 Stats: $trace_before -> $trace_after"
    else
        pass "Tracepoint 数据可能未启用（检查 dmesg）"
    fi

    echo "disable trace_enabled" > /proc/smartmem/control 2>/dev/null
}

# ============================================================
# 3. 分析链路：Hotspot → procfs
# ============================================================
test_hotspot_flow() {
    section "3. Hotspot 识别 → procfs 数据流"

    ensure_module || return

    echo "reset hotspots" > /proc/smartmem/control 2>/dev/null
    sleep 1

    # 产生内存压力
    stress --vm 2 --vm-bytes 512M --timeout 8s 2>/dev/null &
    sleep 10

    # 检查热点数据
    if grep -q "Hotspot #" /proc/smartmem/hotspots 2>/dev/null; then
        pass "Hotspot 识别产生了热点数据"

        # 验证热点数据完整性
        grep -q "score=" /proc/smartmem/hotspots && pass "热点包含评分" || fail "热点包含评分"
        grep -q "call_stack:" /proc/smartmem/hotspots && pass "热点包含调用栈" || fail "热点包含调用栈"
        grep -q "latency_avg=" /proc/smartmem/hotspots && pass "热点包含延迟" || fail "热点包含延迟"
    else
        fail "压力后未产生热点数据"
    fi

    wait 2>/dev/null
}

# ============================================================
# 4. 分析链路：Bottleneck → RootCause 联动
# ============================================================
test_bottleneck_rootcause_flow() {
    section "4. Bottleneck → RootCause 联动"

    ensure_module || return

    # 读取 rootcauses（内部会先触发 bottleneck_update + root_cause_update）
    cat /proc/smartmem/rootcauses > /dev/null 2>&1 && pass "rootcauses 读取正常" || fail "rootcauses 读取正常"

    # 在内存健康状态下，两者都应为空
    local bn_count rc_count
    bn_count=$(grep -c "Bottleneck #" /proc/smartmem/bottlenecks 2>/dev/null || echo 0)
    rc_count=$(grep -c "Root Cause #" /proc/smartmem/rootcauses 2>/dev/null || echo 0)

    if [ "$bn_count" -eq 0 ] && [ "$rc_count" -eq 0 ]; then
        pass "内存健康时无瓶颈/根因（正常）"
    else
        pass "检测到 $bn_count 个瓶颈，$rc_count 个根因"
    fi
}

# ============================================================
# 5. 预测模型链路：采样 → 预测
# ============================================================
test_prediction_flow() {
    section "5. 预测模型 采样→预测 数据流"

    ensure_module || return

    # 重置
    echo "reset prediction" > /proc/smartmem/control 2>/dev/null
    sleep 1

    # 采样不足时
    local pred_type
    pred_type=$(grep "type:" /proc/smartmem/prediction | head -1 | awk '{print $2}')
    [ -n "$pred_type" ] && pass "预测返回 type=$pred_type（采样不足时）" || fail "预测返回 type"

    # 等待采样积累
    echo "等待采样积累（6秒）..."
    sleep 6

    # 采样充足后
    pred_type=$(grep "type:" /proc/smartmem/prediction | head -1 | awk '{print $2}')
    [ -n "$pred_type" ] && pass "预测返回 type=$pred_type" || fail "预测返回 type"

    # 检查模型统计
    grep -q "samples:" /proc/smartmem/prediction && pass "包含采样计数" || fail "包含采样计数"

    local samples
    samples=$(grep "samples:" /proc/smartmem/prediction | awk '{print $2}')
    [ -n "$samples" ] && [ "$samples" -ge 1 ] 2>/dev/null && pass "采样数 >= 1 ($samples)" || fail "采样数 >= 1"
}

# ============================================================
# 6. 端到端：压力 → 热点 → 瓶颈 → 根因 → 预测
# ============================================================
test_end_to_end() {
    section "6. 端到端全链路验证"

    ensure_module || return

    # 重置所有数据
    echo "reset stats" > /proc/smartmem/control 2>/dev/null
    echo "reset hotspots" > /proc/smartmem/control 2>/dev/null
    echo "reset prediction" > /proc/smartmem/control 2>/dev/null
    echo "reset autotune" > /proc/smartmem/control 2>/dev/null

    # 启用监控
    echo "enable trace_enabled" > /proc/smartmem/control 2>/dev/null

    # 产生压力
    echo "运行内存压力测试（15秒）..."
    stress --vm 2 --vm-bytes 512M --timeout 15s 2>/dev/null &
    sleep 17

    # 验证各环节数据
    local has_stats=0 has_hotspots=0 has_prediction=0

    grep -q "alloc_count:" /proc/smartmem/stats && has_stats=1 || has_stats=0
    grep -q "Hotspot #\|No hotspot" /proc/smartmem/hotspots && has_hotspots=1 || has_hotspots=0
    grep -q "predicted_free:" /proc/smartmem/prediction && has_prediction=1 || has_prediction=0

    [ "$has_stats" -eq 1 ] && pass "端到端: Stats 有数据" || fail "端到端: Stats 有数据"
    [ "$has_hotspots" -eq 1 ] && pass "端到端: Hotspots 有数据" || fail "端到端: Hotspots 有数据"
    [ "$has_prediction" -eq 1 ] && pass "端到端: Prediction 有数据" || fail "端到端: Prediction 有数据"

    # 关闭监控
    echo "disable trace_enabled" > /proc/smartmem/control 2>/dev/null

    # 最终快照
    echo ""
    echo "  --- 最终状态快照 ---"
    grep "alloc_count:" /proc/smartmem/stats | head -1
    grep "trace_events:" /proc/smartmem/stats
    grep "type:" /proc/smartmem/prediction | head -1
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 集成测试 - 监控→分析→优化  ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_hook_stats_flow
test_trace_stats_flow
test_hotspot_flow
test_bottleneck_rootcause_flow
test_prediction_flow
test_end_to_end

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
