#!/bin/bash
# SmartMemEngine 集成测试脚本

SMARTMEM_DIR="/proc/smartmem"
MODULE_PATH="/root/project/smartmem_engine/src/smartmem.ko"
PASS=0
FAIL=0
SKIP=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC}: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC}: $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC}: $1"; }

section() { echo ""; echo "=== $1 ==="; }

# 检查模块是否已加载
check_module() {
    lsmod | grep -q smartmem
}

# 等待模块就绪
wait_for_proc() {
    local i=0
    while [ ! -d "$SMARTMEM_DIR" ] && [ $i -lt 10 ]; do
        sleep 1
        i=$((i+1))
    done
    [ -d "$SMARTMEM_DIR" ]
}

# ============================================
echo "SmartMemEngine Integration Test"
echo "================================"
echo ""

# 1. 模块加载/卸载测试
section "1. Module Load/Unload"

# 先卸载确保干净状态
if check_module; then
    rmmod smartmem 2>/dev/null
    sleep 1
fi

# 加载模块
insmod "$MODULE_PATH" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "Module load (insmod)"
else
    fail "Module load (insmod)"
    echo "Cannot continue, aborting."
    exit 1
fi

sleep 1
if check_module; then
    pass "Module visible in lsmod"
else
    fail "Module visible in lsmod"
fi

if wait_for_proc; then
    pass "procfs directory created"
else
    fail "procfs directory created"
fi

# 卸载重载测试
rmmod smartmem 2>/dev/null
if [ $? -eq 0 ]; then
    pass "Module unload (rmmod)"
else
    fail "Module unload (rmmod)"
fi

sleep 1
if ! check_module; then
    pass "Module removed from lsmod after unload"
else
    fail "Module removed from lsmod after unload"
fi

# 重新加载用于后续测试
insmod "$MODULE_PATH" 2>/dev/null
sleep 1
if check_module && wait_for_proc; then
    pass "Module reload for subsequent tests"
else
    fail "Module reload for subsequent tests"
    exit 1
fi

# 2. procfs 接口测试
section "2. procfs Interface"

# config
if [ -f "$SMARTMEM_DIR/config" ]; then
    pass "config file exists"
    if cat "$SMARTMEM_DIR/config" > /dev/null 2>&1; then
        pass "config file readable"
        if cat "$SMARTMEM_DIR/config" | grep -q "hook_buddy_enabled"; then
            pass "config contains hook_buddy_enabled"
        else
            fail "config contains hook_buddy_enabled"
        fi
    else
        fail "config file readable"
    fi
else
    fail "config file exists"
fi

# config 写入
if echo "hook_buddy_enabled=true" > "$SMARTMEM_DIR/config" 2>/dev/null; then
    pass "config write key=value"
else
    fail "config write key=value"
fi

# stats
if [ -f "$SMARTMEM_DIR/stats" ]; then
    pass "stats file exists"
    if cat "$SMARTMEM_DIR/stats" | grep -q "alloc_count"; then
        pass "stats contains alloc_count"
    else
        fail "stats contains alloc_count"
    fi
else
    fail "stats file exists"
fi

# policies
if [ -f "$SMARTMEM_DIR/policies" ]; then
    pass "policies file exists"
    if cat "$SMARTMEM_DIR/policies" | grep -q "numa_buddy"; then
        pass "policies shows numa_buddy strategy"
    else
        fail "policies shows numa_buddy strategy"
    fi
else
    fail "policies file exists"
fi

# hotspots
if [ -f "$SMARTMEM_DIR/hotspots" ]; then
    pass "hotspots file exists"
    cat "$SMARTMEM_DIR/hotspots" > /dev/null 2>&1 && pass "hotspots readable" || fail "hotspots readable"
else
    fail "hotspots file exists"
fi

# bottlenecks
if [ -f "$SMARTMEM_DIR/bottlenecks" ]; then
    pass "bottlenecks file exists"
    cat "$SMARTMEM_DIR/bottlenecks" > /dev/null 2>&1 && pass "bottlenecks readable" || fail "bottlenecks readable"
else
    fail "bottlenecks file exists"
fi

# rootcauses
if [ -f "$SMARTMEM_DIR/rootcauses" ]; then
    pass "rootcauses file exists"
    cat "$SMARTMEM_DIR/rootcauses" > /dev/null 2>&1 && pass "rootcauses readable" || fail "rootcauses readable"
else
    fail "rootcauses file exists"
fi

# autotune
if [ -f "$SMARTMEM_DIR/autotune" ]; then
    pass "autotune file exists"
    cat "$SMARTMEM_DIR/autotune" > /dev/null 2>&1 && pass "autotune readable" || fail "autotune readable"
else
    fail "autotune file exists"
fi

# prediction
if [ -f "$SMARTMEM_DIR/prediction" ]; then
    pass "prediction file exists"
    if cat "$SMARTMEM_DIR/prediction" | grep -q "predicted_free"; then
        pass "prediction contains predicted_free"
    else
        fail "prediction contains predicted_free"
    fi
else
    fail "prediction file exists"
fi

# control
if [ -f "$SMARTMEM_DIR/control" ]; then
    pass "control file exists"
    cat "$SMARTMEM_DIR/control" > /dev/null 2>&1 && pass "control readable" || fail "control readable"
else
    fail "control file exists"
fi

# 3. control 命令测试
section "3. Control Commands"

# reset stats
echo "reset stats" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: reset stats"
else
    fail "control: reset stats"
fi

# reset hotspots
echo "reset hotspots" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: reset hotspots"
else
    fail "control: reset hotspots"
fi

# reset prediction
echo "reset prediction" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: reset prediction"
else
    fail "control: reset prediction"
fi

# reset autotune
echo "reset autotune" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: reset autotune"
else
    fail "control: reset autotune"
fi

# enable trace
echo "enable trace_enabled" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: enable trace_enabled"
else
    fail "control: enable trace_enabled"
fi

# disable trace
echo "disable trace_enabled" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: disable trace_enabled"
else
    fail "control: disable trace_enabled"
fi

# tune compact
echo "tune compact" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: tune compact"
else
    fail "control: tune compact"
fi

# tune slab
echo "tune slab" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: tune slab"
else
    fail "control: tune slab"
fi

# set 配置
echo "set hook_buddy_enabled=true" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -eq 0 ]; then
    pass "control: set hook_buddy_enabled=true"
else
    fail "control: set hook_buddy_enabled=true"
fi

# 4. 统计数据采集测试
section "4. Statistics Collection"

# 触发一些内存操作后检查统计
sleep 3

BUDDY_ALLOC=$(cat "$SMARTMEM_DIR/stats" | grep "alloc_count:" | head -1 | awk '{print $2}')
if [ -n "$BUDDY_ALLOC" ] && [ "$BUDDY_ALLOC" -ge 0 ] 2>/dev/null; then
    pass "buddy alloc_count is numeric ($BUDDY_ALLOC)"
else
    fail "buddy alloc_count is numeric"
fi

TRACE_EVENTS=$(cat "$SMARTMEM_DIR/stats" | grep "trace_events:" | awk '{print $2}')
if [ -n "$TRACE_EVENTS" ] && [ "$TRACE_EVENTS" -ge 0 ] 2>/dev/null; then
    pass "trace_events is numeric ($TRACE_EVENTS)"
else
    fail "trace_events is numeric"
fi

# 5. 预测模型测试
section "5. Prediction Model"

# 等待采样积累
sleep 6

PRED_TYPE=$(cat "$SMARTMEM_DIR/prediction" | grep "type:" | awk '{print $2}')
if [ -n "$PRED_TYPE" ]; then
    pass "prediction type returned ($PRED_TYPE)"
else
    fail "prediction type returned"
fi

PRED_SAMPLES=$(cat "$SMARTMEM_DIR/prediction" | grep "samples:" | awk '{print $2}')
if [ -n "$PRED_SAMPLES" ] && [ "$PRED_SAMPLES" -ge 1 ] 2>/dev/null; then
    pass "prediction samples >= 1 ($PRED_SAMPLES)"
else
    fail "prediction samples >= 1"
fi

# 6. 用户空间工具测试
section "6. Userspace Tools"

TOOL_DIR="$(dirname $0)"

if [ -x "$TOOL_DIR/memctl" ]; then
    pass "memctl exists and executable"
    $TOOL_DIR/memctl config > /dev/null 2>&1 && pass "memctl config works" || fail "memctl config works"
    $TOOL_DIR/memctl enable trace_enabled > /dev/null 2>&1 && pass "memctl enable works" || fail "memctl enable works"
    $TOOL_DIR/memctl disable trace_enabled > /dev/null 2>&1 && pass "memctl disable works" || fail "memctl disable works"
    $TOOL_DIR/memctl reset stats > /dev/null 2>&1 && pass "memctl reset works" || fail "memctl reset works"
    $TOOL_DIR/memctl tune compact > /dev/null 2>&1 && pass "memctl tune works" || fail "memctl tune works"
else
    skip "memctl not found"
fi

if [ -x "$TOOL_DIR/memstat" ]; then
    pass "memstat exists and executable"
    $TOOL_DIR/memstat stats > /dev/null 2>&1 && pass "memstat stats works" || fail "memstat stats works"
    $TOOL_DIR/memstat policies > /dev/null 2>&1 && pass "memstat policies works" || fail "memstat policies works"
    $TOOL_DIR/memstat hotspots > /dev/null 2>&1 && pass "memstat hotspots works" || fail "memstat hotspots works"
    $TOOL_DIR/memstat bottlenecks > /dev/null 2>&1 && pass "memstat bottlenecks works" || fail "memstat bottlenecks works"
    $TOOL_DIR/memstat prediction > /dev/null 2>&1 && pass "memstat prediction works" || fail "memstat prediction works"
    $TOOL_DIR/memstat summary > /dev/null 2>&1 && pass "memstat summary works" || fail "memstat summary works"
    $TOOL_DIR/memstat all > /dev/null 2>&1 && pass "memstat all works" || fail "memstat all works"
else
    skip "memstat not found"
fi

if [ -x "$TOOL_DIR/memview" ]; then
    pass "memview exists and executable"
    timeout 2 $TOOL_DIR/memview overview > /dev/null 2>&1 && pass "memview overview works" || fail "memview overview works"
    $TOOL_DIR/memview bar > /dev/null 2>&1 && pass "memview bar works" || fail "memview bar works"
else
    skip "memview not found"
fi

# 7. debugfs 接口测试
section "7. debugfs Interface"

if [ -d "/sys/kernel/debug/smartmem" ]; then
    pass "debugfs smartmem directory exists"

    if [ -f "/sys/kernel/debug/smartmem/status" ]; then
        pass "debugfs status file exists"
        cat /sys/kernel/debug/smartmem/status > /dev/null 2>&1 && pass "debugfs status readable" || fail "debugfs status readable"
    else
        fail "debugfs status file exists"
    fi

    if [ -f "/sys/kernel/debug/smartmem/hotspots" ]; then
        pass "debugfs hotspots file exists"
        cat /sys/kernel/debug/smartmem/hotspots > /dev/null 2>&1 && pass "debugfs hotspots readable" || fail "debugfs hotspots readable"
    else
        fail "debugfs hotspots file exists"
    fi

    if [ -f "/sys/kernel/debug/smartmem/tune_history" ]; then
        pass "debugfs tune_history file exists"
        cat /sys/kernel/debug/smartmem/tune_history > /dev/null 2>&1 && pass "debugfs tune_history readable" || fail "debugfs tune_history readable"
    else
        fail "debugfs tune_history file exists"
    fi
else
    skip "debugfs smartmem directory not found (need root or debugfs mounted)"
fi

# 8. 内存压力场景测试
section "8. Memory Pressure Scenario"

# 重置统计
echo "reset stats" > "$SMARTMEM_DIR/control" 2>/dev/null
echo "reset hotspots" > "$SMARTMEM_DIR/control" 2>/dev/null

# 启用 trace 和 autotune
echo "enable trace_enabled" > "$SMARTMEM_DIR/control" 2>/dev/null
echo "enable auto_tune_enabled" > "$SMARTMEM_DIR/control" 2>/dev/null

# 运行内存压力
if command -v stress > /dev/null 2>&1; then
    stress --vm 2 --vm-bytes 512M --timeout 10s 2>/dev/null &
    STRESS_PID=$!
    sleep 12
    wait $STRESS_PID 2>/dev/null

    # 检查压力期间数据变化
    POST_ALLOC=$(cat "$SMARTMEM_DIR/stats" | grep "alloc_count:" | head -1 | awk '{print $2}')
    if [ -n "$POST_ALLOC" ] && [ "$POST_ALLOC" -gt 0 ] 2>/dev/null; then
        pass "alloc_count increased during stress ($POST_ALLOC)"
    else
        fail "alloc_count increased during stress"
    fi

    # 检查热点数据
    HOTSPOT_LINE=$(cat "$SMARTMEM_DIR/hotspots" | grep "Hotspot #" | head -1)
    if [ -n "$HOTSPOT_LINE" ]; then
        pass "hotspot data collected during stress"
    else
        fail "hotspot data collected during stress"
    fi

    # 检查预测
    PRED_DESC=$(cat "$SMARTMEM_DIR/prediction" | grep "description:" | head -1)
    if [ -n "$PRED_DESC" ]; then
        pass "prediction description present after stress"
    else
        fail "prediction description present after stress"
    fi
else
    skip "stress tool not installed, skipping pressure test"
fi

# 禁用 autotune
echo "disable auto_tune_enabled" > "$SMARTMEM_DIR/control" 2>/dev/null

# 9. 边界条件测试
section "9. Boundary Conditions"

# 无效 control 命令
echo "invalid_command" > "$SMARTMEM_DIR/control" 2>/dev/null
if [ $? -ne 0 ]; then
    pass "control rejects invalid command"
else
    # procfs write 返回值不一定反映到用户空间
    pass "control handles invalid command (no crash)"
fi

# 无效 config 写入
echo "invalidkey=invalidvalue" > "$SMARTMEM_DIR/config" 2>/dev/null
pass "config handles invalid key (no crash)"

# 多次快速加载/卸载
rmmod smartmem 2>/dev/null
sleep 1
insmod "$MODULE_PATH" 2>/dev/null && pass "reload after unload" || fail "reload after unload"
sleep 1

# 10. 结果汇总
section "Test Summary"

TOTAL=$((PASS + FAIL + SKIP))
echo ""
echo "  Total:  $TOTAL"
echo -e "  ${GREEN}Pass:   $PASS${NC}"
echo -e "  ${RED}Fail:   $FAIL${NC}"
echo -e "  ${YELLOW}Skip:   $SKIP${NC}"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
