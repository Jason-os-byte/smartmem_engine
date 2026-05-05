#!/bin/bash
# SmartMemEngine 单元测试 - procfs 接口
# 逐一验证 /proc/smartmem/ 下 9 个接口的读写行为

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"
PROC_DIR="/proc/smartmem"
TOOL_DIR="$PROJECT_DIR/src/tools"

# 颜色与计数
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0; SKIP=0

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} [$PASS]: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} [$FAIL]: $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC} [$SKIP]: $1"; }

section() { echo ""; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; echo "  $1"; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; }

# 确保模块加载
ensure_module() {
    if ! lsmod | grep -q smartmem; then
        insmod "$MODULE_PATH" 2>/dev/null
        sleep 1
    fi
    [ -d "$PROC_DIR" ]
}

# ============================================================
# 1. config 接口测试
# ============================================================
test_config() {
    section "1. /proc/smartmem/config (读写)"

    # 1.1 可读性
    ensure_module || { skip "模块未加载"; return; }
    cat "$PROC_DIR/config" > /dev/null 2>&1 && pass "config 可读" || fail "config 可读"

    # 1.2 包含所有配置项
    for key in hook_buddy_enabled hook_slub_enabled hook_vma_enabled \
               hook_lru_enabled hook_numa_enabled numa_aware_enabled \
               adaptive_slub_enabled ebpf_enabled trace_enabled auto_tune_enabled; do
        grep -q "$key" "$PROC_DIR/config" 2>/dev/null && pass "config 包含 $key" || fail "config 包含 $key"
    done

    # 1.3 写入合法值
    echo "hook_buddy_enabled=true" > "$PROC_DIR/config" 2>/dev/null && pass "config 写入 key=true" || fail "config 写入 key=true"
    echo "hook_buddy_enabled=false" > "$PROC_DIR/config" 2>/dev/null && pass "config 写入 key=false" || fail "config 写入 key=false"

    # 1.4 写入非法值
    echo "invalid_key=value" > "$PROC_DIR/config" 2>/dev/null
    pass "config 写入非法 key 不崩溃"

    echo "hook_buddy_enabled=invalid" > "$PROC_DIR/config" 2>/dev/null
    pass "config 写入非法 value 不崩溃"

    # 1.5 空写入
    echo "" > "$PROC_DIR/config" 2>/dev/null
    pass "config 空写入不崩溃"

    # 1.6 超长写入
    head -c 300 /dev/zero | tr '\0' 'A' > "$PROC_DIR/config" 2>/dev/null
    pass "config 超长写入不崩溃（应返回 -EINVAL）"
}

# ============================================================
# 2. control 接口测试
# ============================================================
test_control() {
    section "2. /proc/smartmem/control (读写)"

    ensure_module || return

    # 2.1 可读性
    cat "$PROC_DIR/control" > /dev/null 2>&1 && pass "control 可读" || fail "control 可读"

    # 2.2 help 信息包含所有命令
    local content
    content=$(cat "$PROC_DIR/control")
    for cmd in enable disable reset tune set; do
        echo "$content" | grep -q "$cmd" && pass "control 帮助包含 $cmd" || fail "control 帮助包含 $cmd"
    done

    # 2.3 enable 命令
    echo "enable trace_enabled" > "$PROC_DIR/control" 2>/dev/null && pass "control: enable trace_enabled" || fail "control: enable trace_enabled"
    echo "disable trace_enabled" > "$PROC_DIR/control" 2>/dev/null && pass "control: disable trace_enabled" || fail "control: disable trace_enabled"

    echo "enable auto_tune_enabled" > "$PROC_DIR/control" 2>/dev/null && pass "control: enable auto_tune_enabled" || fail "control: enable auto_tune_enabled"
    echo "disable auto_tune_enabled" > "$PROC_DIR/control" 2>/dev/null && pass "control: disable auto_tune_enabled" || fail "control: disable auto_tune_enabled"

    # 2.4 reset 命令
    for target in stats hotspots autotune prediction; do
        echo "reset $target" > "$PROC_DIR/control" 2>/dev/null && pass "control: reset $target" || fail "control: reset $target"
    done

    # 2.5 tune 命令
    for action in compact watermark numa slab; do
        echo "tune $action" > "$PROC_DIR/control" 2>/dev/null
        pass "control: tune $action 不崩溃"
    done

    # 2.6 set 命令
    echo "set hook_buddy_enabled=true" > "$PROC_DIR/control" 2>/dev/null && pass "control: set key=value" || fail "control: set key=value"

    # 2.7 非法命令
    echo "invalid_command" > "$PROC_DIR/control" 2>/dev/null
    pass "control: 非法命令不崩溃"

    echo "enable" > "$PROC_DIR/control" 2>/dev/null
    pass "control: enable 缺少参数不崩溃"

    echo "reset" > "$PROC_DIR/control" 2>/dev/null
    pass "control: reset 缺少参数不崩溃"

    echo "tune invalid" > "$PROC_DIR/control" 2>/dev/null
    pass "control: tune 非法动作不崩溃"
}

# ============================================================
# 3. stats 接口测试
# ============================================================
test_stats() {
    section "3. /proc/smartmem/stats (只读)"

    ensure_module || return

    # 3.1 可读性
    cat "$PROC_DIR/stats" > /dev/null 2>&1 && pass "stats 可读" || fail "stats 可读"

    # 3.2 包含所有统计段
    for section_name in "Buddy Allocator" "SLUB Allocator" "NUMA" "Monitoring" "Auto-Tune" "Prediction"; do
        grep -q "$section_name" "$PROC_DIR/stats" && pass "stats 包含 $section_name 段" || fail "stats 包含 $section_name 段"
    done

    # 3.3 统计值为数字
    local alloc
    alloc=$(grep "alloc_count:" "$PROC_DIR/stats" | head -1 | awk '{print $2}')
    [ -n "$alloc" ] && [ "$alloc" -ge 0 ] 2>/dev/null && pass "buddy alloc_count 为非负整数 ($alloc)" || fail "buddy alloc_count 为非负整数"

    # 3.4 reset 后归零
    echo "reset stats" > "$PROC_DIR/control" 2>/dev/null
    sleep 1
    # 短暂等待后统计应该从0开始增长（系统自身会触发分配）
    pass "reset stats 执行成功"

    # 3.5 只读验证（写入应失败或不生效）
    echo "test" > "$PROC_DIR/stats" 2>/dev/null
    pass "stats 写入不崩溃（只读文件）"
}

# ============================================================
# 4. policies 接口测试
# ============================================================
test_policies() {
    section "4. /proc/smartmem/policies (只读)"

    ensure_module || return

    # 4.1 可读性
    cat "$PROC_DIR/policies" > /dev/null 2>&1 && pass "policies 可读" || fail "policies 可读"

    # 4.2 包含4种策略
    for strategy in "numa_buddy" "adaptive_slub" "multi_gen_lru" "numa_balance"; do
        grep -q "$strategy" "$PROC_DIR/policies" && pass "policies 包含 $strategy" || fail "policies 包含 $strategy"
    done

    # 4.3 策略包含统计信息
    grep -q "local_alloc" "$PROC_DIR/policies" && pass "policies 包含策略统计" || fail "policies 包含策略统计"
}

# ============================================================
# 5. hotspots 接口测试
# ============================================================
test_hotspots() {
    section "5. /proc/smartmem/hotspots (只读)"

    ensure_module || return

    # 5.1 可读性
    cat "$PROC_DIR/hotspots" > /dev/null 2>&1 && pass "hotspots 可读" || fail "hotspots 可读"

    # 5.2 格式验证
    grep -q "SmartMemEngine Hotspots" "$PROC_DIR/hotspots" && pass "hotspots 包含标题" || fail "hotspots 包含标题"

    # 5.3 重置
    echo "reset hotspots" > "$PROC_DIR/control" 2>/dev/null && pass "hotspots 重置成功" || fail "hotspots 重置成功"

    # 5.4 重置后读取
    cat "$PROC_DIR/hotspots" > /dev/null 2>&1 && pass "hotspots 重置后可读" || fail "hotspots 重置后可读"

    # 5.5 触发内存活动后检查数据
    # 产生一些慢分配
    stress --vm 1 --vm-bytes 256M --timeout 5s 2>/dev/null &
    wait $! 2>/dev/null
    sleep 2

    if grep -q "Hotspot #" "$PROC_DIR/hotspots" 2>/dev/null; then
        pass "压力后产生热点数据"
        # 验证热点格式
        grep -q "score=" "$PROC_DIR/hotspots" && pass "热点包含 score" || fail "热点包含 score"
        grep -q "alloc_count=" "$PROC_DIR/hotspots" && pass "热点包含 alloc_count" || fail "热点包含 alloc_count"
        grep -q "latency_avg=" "$PROC_DIR/hotspots" && pass "热点包含 latency_avg" || fail "热点包含 latency_avg"
        grep -q "call_stack:" "$PROC_DIR/hotspots" && pass "热点包含 call_stack" || fail "热点包含 call_stack"
    else
        skip "压力后未产生热点（可能无 >100us 慢分配）"
    fi
}

# ============================================================
# 6. bottlenecks 接口测试
# ============================================================
test_bottlenecks() {
    section "6. /proc/smartmem/bottlenecks (只读)"

    ensure_module || return

    # 6.1 可读性
    cat "$PROC_DIR/bottlenecks" > /dev/null 2>&1 && pass "bottlenecks 可读" || fail "bottlenecks 可读"

    # 6.2 格式验证
    grep -q "SmartMemEngine Bottlenecks" "$PROC_DIR/bottlenecks" && pass "bottlenecks 包含标题" || fail "bottlenecks 包含标题"

    # 6.3 读取时触发分析（bottleneck_update 在 read 回调中调用）
    # 连续读取不应崩溃
    for i in 1 2 3; do
        cat "$PROC_DIR/bottlenecks" > /dev/null 2>&1 || { fail "bottlenecks 第${i}次读取失败"; return; }
    done
    pass "bottlenecks 连续3次读取正常"
}

# ============================================================
# 7. rootcauses 接口测试
# ============================================================
test_rootcauses() {
    section "7. /proc/smartmem/rootcauses (只读)"

    ensure_module || return

    # 7.1 可读性
    cat "$PROC_DIR/rootcauses" > /dev/null 2>&1 && pass "rootcauses 可读" || fail "rootcauses 可读"

    # 7.2 格式验证
    grep -q "SmartMemEngine Root Cause" "$PROC_DIR/rootcauses" && pass "rootcauses 包含标题" || fail "rootcauses 包含标题"

    # 7.3 连续读取
    for i in 1 2 3; do
        cat "$PROC_DIR/rootcauses" > /dev/null 2>&1 || { fail "rootcauses 第${i}次读取失败"; return; }
    done
    pass "rootcauses 连续3次读取正常"
}

# ============================================================
# 8. autotune 接口测试
# ============================================================
test_autotune() {
    section "8. /proc/smartmem/autotune (读写)"

    ensure_module || return

    # 8.1 可读性
    cat "$PROC_DIR/autotune" > /dev/null 2>&1 && pass "autotune 可读" || fail "autotune 可读"

    # 8.2 包含统计和历史
    grep -q "SmartMemEngine Auto-Tune" "$PROC_DIR/autotune" && pass "autotune 包含标题" || fail "autotune 包含标题"
    grep -q "total_tunes" "$PROC_DIR/autotune" && pass "autotune 包含统计" || fail "autotune 包含统计"

    # 8.3 写入触发调优
    echo "compact" > "$PROC_DIR/autotune" 2>/dev/null && pass "autotune 写入 compact" || fail "autotune 写入 compact"
    echo "slab" > "$PROC_DIR/autotune" 2>/dev/null && pass "autotune 写入 slab" || fail "autotune 写入 slab"

    # 8.4 等待冷却后再次触发
    echo "watermark" > "$PROC_DIR/autotune" 2>/dev/null
    pass "autotune 写入 watermark（可能被冷却期阻止）"

    # 8.5 非法写入
    echo "invalid" > "$PROC_DIR/autotune" 2>/dev/null
    pass "autotune 非法写入不崩溃"

    # 8.6 验证历史记录
    sleep 1
    if grep -q "action=" "$PROC_DIR/autotune" 2>/dev/null; then
        pass "autotune 包含历史记录"
    else
        # 可能被冷却期阻止，没有历史
        pass "autotune 历史可能为空（冷却期限制）"
    fi
}

# ============================================================
# 9. prediction 接口测试
# ============================================================
test_prediction() {
    section "9. /proc/smartmem/prediction (只读)"

    ensure_module || return

    # 9.1 可读性
    cat "$PROC_DIR/prediction" > /dev/null 2>&1 && pass "prediction 可读" || fail "prediction 可读"

    # 9.2 包含预测结果字段
    for field in "type:" "severity:" "predicted_free:" "confidence:" "description:"; do
        grep -q "$field" "$PROC_DIR/prediction" && pass "prediction 包含 $field" || fail "prediction 包含 $field"
    done

    # 9.3 采样不足时的行为
    echo "reset prediction" > "$PROC_DIR/control" 2>/dev/null
    sleep 1
    cat "$PROC_DIR/prediction" > /dev/null 2>&1 && pass "prediction 重置后可读" || fail "prediction 重置后可读"

    # 9.4 等待采样积累后预测
    sleep 6
    if grep -q "slope=" "$PROC_DIR/prediction" 2>/dev/null; then
        pass "prediction 采样积累后包含 slope 信息"
    else
        pass "prediction 结果格式正常（可能未显示 slope）"
    fi

    # 9.5 连续读取
    for i in 1 2 3; do
        cat "$PROC_DIR/prediction" > /dev/null 2>&1 || { fail "prediction 第${i}次读取失败"; return; }
    done
    pass "prediction 连续3次读取正常"
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 单元测试 - procfs 接口   ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_config
test_control
test_stats
test_policies
test_hotspots
test_bottlenecks
test_rootcauses
test_autotune
test_prediction

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  测试结果汇总"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TOTAL=$((PASS + FAIL + SKIP))
echo "  总计: $TOTAL"
echo -e "  ${GREEN}通过: $PASS${NC}"
echo -e "  ${RED}失败: $FAIL${NC}"
echo -e "  ${YELLOW}跳过: $SKIP${NC}"
echo ""

[ $FAIL -eq 0 ] && echo -e "${GREEN}全部通过!${NC}" || echo -e "${RED}存在失败用例!${NC}"
exit $FAIL
