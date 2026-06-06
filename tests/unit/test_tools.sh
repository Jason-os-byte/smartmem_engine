#!/bin/bash
# SmartMemEngine 单元测试 - 用户空间工具

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"
TOOL_DIR="$PROJECT_DIR/src/tools"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0; SKIP=0

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}PASS${NC} [$PASS]: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC} [$FAIL]: $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC} [$SKIP]: $1"; }

section() { echo ""; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; echo "  $1"; echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; }

ensure_module() {
    if ! lsmod | grep -q smartmem; then
        insmod "$MODULE_PATH" 2>/dev/null
        sleep 1
    fi
    lsmod | grep -q smartmem
}

# ============================================================
# 1. memctl 测试
# ============================================================
test_memctl() {
    section "1. memctl 配置管理工具"

    local memctl="$TOOL_DIR/memctl"
    [ -x "$memctl" ] || { skip "memctl 不存在或不可执行"; return; }

    # 无参数显示帮助
    $memctl 2>&1 | grep -q "Usage" && pass "memctl 无参数显示帮助" || fail "memctl 无参数显示帮助"
    $memctl help 2>&1 | grep -q "Usage" && pass "memctl help 显示帮助" || fail "memctl help 显示帮助"

    # config 子命令
    $memctl config > /dev/null 2>&1 && pass "memctl config 显示配置" || fail "memctl config 显示配置"
    $memctl config get hook_buddy_enabled > /dev/null 2>&1 && pass "memctl config get 获取单项" || fail "memctl config get 获取单项"
    $memctl config set hook_buddy_enabled=true > /dev/null 2>&1 && pass "memctl config set 设置配置" || fail "memctl config set 设置配置"

    # enable/disable
    $memctl enable trace_enabled > /dev/null 2>&1 && pass "memctl enable trace_enabled" || fail "memctl enable trace_enabled"
    $memctl disable trace_enabled > /dev/null 2>&1 && pass "memctl disable trace_enabled" || fail "memctl disable trace_enabled"
    $memctl enable auto_tune_enabled > /dev/null 2>&1 && pass "memctl enable auto_tune_enabled" || fail "memctl enable auto_tune_enabled"
    $memctl disable auto_tune_enabled > /dev/null 2>&1 && pass "memctl disable auto_tune_enabled" || fail "memctl disable auto_tune_enabled"

    # reset
    $memctl reset stats > /dev/null 2>&1 && pass "memctl reset stats" || fail "memctl reset stats"
    $memctl reset hotspots > /dev/null 2>&1 && pass "memctl reset hotspots" || fail "memctl reset hotspots"
    $memctl reset prediction > /dev/null 2>&1 && pass "memctl reset prediction" || fail "memctl reset prediction"
    $memctl reset autotune > /dev/null 2>&1 && pass "memctl reset autotune" || fail "memctl reset autotune"

    # tune
    $memctl tune compact > /dev/null 2>&1 && pass "memctl tune compact" || fail "memctl tune compact"
    $memctl tune slab > /dev/null 2>&1 && pass "memctl tune slab" || fail "memctl tune slab"

    # 非法命令
    $memctl invalid_cmd 2>&1 | grep -q "Error\|Usage" && pass "memctl 非法命令报错" || fail "memctl 非法命令报错"

    # 模块未加载时
    rmmod smartmem 2>/dev/null
    $memctl config 2>&1 | grep -q "not loaded\|Error" && pass "memctl 模块未加载时报错" || fail "memctl 模块未加载时报错"
    insmod "$MODULE_PATH" 2>/dev/null; sleep 1
}

# ============================================================
# 2. memstat 测试
# ============================================================
test_memstat() {
    section "2. memstat 统计查看工具"

    local memstat="$TOOL_DIR/memstat"
    [ -x "$memstat" ] || { skip "memstat 不存在或不可执行"; return; }

    # 无参数显示帮助
    $memstat 2>&1 | grep -q "Usage" && pass "memstat 无参数显示帮助" || fail "memstat 无参数显示帮助"

    # 各子命令
    for cmd in stats policies hotspots bottlenecks rootcauses autotune prediction summary all; do
        $memstat $cmd > /dev/null 2>&1 && pass "memstat $cmd" || fail "memstat $cmd"
    done

    # summary 输出格式
    local output
    output=$($memstat summary 2>&1)
    echo "$output" | grep -q "buddy_alloc=" && pass "memstat summary 包含 buddy_alloc" || fail "memstat summary 包含 buddy_alloc"
    echo "$output" | grep -q "free=" && pass "memstat summary 包含 free" || fail "memstat summary 包含 free"
    echo "$output" | grep -q "numa_locality=" && pass "memstat summary 包含 numa_locality" || fail "memstat summary 包含 numa_locality"

    # all 输出包含多段
    output=$($memstat all 2>&1)
    echo "$output" | grep -q "Buddy Allocator" && pass "memstat all 包含 Buddy 段" || fail "memstat all 包含 Buddy 段"
    echo "$output" | grep -q "Prediction" && pass "memstat all 包含 Prediction 段" || fail "memstat all 包含 Prediction 段"

    # 非法命令
    $memstat invalid 2>&1 | grep -q "Error\|Usage" && pass "memstat 非法命令报错" || fail "memstat 非法命令报错"
}

# ============================================================
# 3. memview 测试
# ============================================================
test_memview() {
    section "3. memview 可视化工具"

    local memview="$TOOL_DIR/memview"
    [ -x "$memview" ] || { skip "memview 不存在或不可执行"; return; }

    # 无参数显示帮助
    $memview 2>&1 | grep -q "Usage" && pass "memview 无参数显示帮助" || fail "memview 无参数显示帮助"

    # overview
    local output
    output=$(timeout 2 $memview overview 2>&1)
    [ $? -eq 0 ] && pass "memview overview" || fail "memview overview"
    echo "$output" | grep -q "Memory Usage" && pass "overview 包含 Memory Usage" || fail "overview 包含 Memory Usage"
    echo "$output" | grep -q "NUMA Locality" && pass "overview 包含 NUMA Locality" || fail "overview 包含 NUMA Locality"

    # bar
    output=$($memview bar 2>&1)
    [ $? -eq 0 ] && pass "memview bar" || fail "memview bar"
    echo "$output" | grep -q "Memory Layout" && pass "bar 包含 Memory Layout" || fail "bar 包含 Memory Layout"
    # 检查图例（剥离 ANSI 转义码后匹配，因为彩色输出中 F 和 = 之间有 \033[0m）
    echo "$output" | sed 's/\x1b\[[0-9;]*m//g' | grep -q "F=Free" && pass "bar 包含图例" || fail "bar 包含图例"

    # top（限时2秒，timeout 超时退出码 124 是预期行为）
    timeout 3 $memview top > /dev/null 2>&1
    rc=$?
    [ $rc -eq 124 ] || [ $rc -le 1 ] && pass "memview top 运行正常（Ctrl+C/超时退出）" || fail "memview top 运行正常 (rc=$rc)"

    # watch 限时3秒（超时退出码 124 是预期行为）
    timeout 4 $memview watch 1 > /dev/null 2>&1
    rc=$?
    [ $rc -eq 124 ] || [ $rc -le 1 ] && pass "memview watch 1 运行正常" || fail "memview watch 1 运行正常 (rc=$rc)"

    # 非法命令
    $memview invalid 2>&1 | grep -q "Error\|Usage" && pass "memview 非法命令报错" || fail "memview 非法命令报错"
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 单元测试 - 用户空间工具   ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

test_memctl
test_memstat
test_memview

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
