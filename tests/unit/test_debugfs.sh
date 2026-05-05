#!/bin/bash
# SmartMemEngine 单元测试 - debugfs 接口

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MODULE_PATH="$PROJECT_DIR/src/smartmem.ko"
DEBUGFS_DIR="/sys/kernel/debug/smartmem"

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
# 0. debugfs 可用性检查
# ============================================================
test_debugfs_available() {
    section "0. debugfs 可用性"

    # 检查 debugfs 是否挂载
    if mount | grep -q "debugfs"; then
        pass "debugfs 已挂载"
    else
        mount -t debugfs none /sys/kernel/debug/ 2>/dev/null
        if mount | grep -q "debugfs"; then
            pass "debugfs 手动挂载成功"
        else
            fail "debugfs 挂载失败"
            return 1
        fi
    fi

    # 检查 smartmem 目录
    if [ -d "$DEBUGFS_DIR" ]; then
        pass "debugfs smartmem 目录存在"
    else
        # 模块可能没加载
        ensure_module
        sleep 1
        if [ -d "$DEBUGFS_DIR" ]; then
            pass "debugfs smartmem 目录存在（加载模块后）"
        else
            fail "debugfs smartmem 目录不存在"
            return 1
        fi
    fi

    return 0
}

# ============================================================
# 1. status 接口测试
# ============================================================
test_status() {
    section "1. /sys/kernel/debug/smartmem/status"

    [ -f "$DEBUGFS_DIR/status" ] && pass "status 文件存在" || { fail "status 文件不存在"; return; }

    # 可读性
    cat "$DEBUGFS_DIR/status" > /dev/null 2>&1 && pass "status 可读" || fail "status 可读"

    # 包含完整状态 dump 的各段
    local content
    content=$(cat "$DEBUGFS_DIR/status" 2>/dev/null)

    for section_name in "Configuration" "Hook" "Strategy" "Bottleneck" "Root Cause" "Auto-Tune" "Prediction" "Monitor"; do
        echo "$content" | grep -q "$section_name" && pass "status 包含 $section_name 段" || fail "status 包含 $section_name 段"
    done

    # 多次读取不崩溃
    for i in 1 2 3 4 5; do
        cat "$DEBUGFS_DIR/status" > /dev/null 2>&1 || { fail "status 第${i}次读取失败"; return; }
    done
    pass "status 连续5次读取正常"
}

# ============================================================
# 2. hotspots 接口测试
# ============================================================
test_hotspots() {
    section "2. /sys/kernel/debug/smartmem/hotspots"

    [ -f "$DEBUGFS_DIR/hotspots" ] && pass "hotspots 文件存在" || { fail "hotspots 文件不存在"; return; }

    # 可读性
    cat "$DEBUGFS_DIR/hotspots" > /dev/null 2>&1 && pass "hotspots 可读" || fail "hotspots 可读"

    # debugfs 热点应包含原始栈和解析栈
    local content
    content=$(cat "$DEBUGFS_DIR/hotspots" 2>/dev/null)

    echo "$content" | grep -q "raw_stack\|resolved_stack" && pass "hotspots 包含栈信息（raw/resolved）" || fail "hotspots 包含栈信息"

    # 空数据时也不崩溃
    echo "reset hotspots" > /proc/smartmem/control 2>/dev/null
    cat "$DEBUGFS_DIR/hotspots" > /dev/null 2>&1 && pass "hotspots 重置后可读" || fail "hotspots 重置后可读"
}

# ============================================================
# 3. tune_history 接口测试
# ============================================================
test_tune_history() {
    section "3. /sys/kernel/debug/smartmem/tune_history"

    [ -f "$DEBUGFS_DIR/tune_history" ] && pass "tune_history 文件存在" || { fail "tune_history 文件不存在"; return; }

    # 可读性
    cat "$DEBUGFS_DIR/tune_history" > /dev/null 2>&1 && pass "tune_history 可读" || fail "tune_history 可读"

    # 触发一次调优使历史不为空
    echo "compact" > /proc/smartmem/autotune 2>/dev/null
    sleep 1

    cat "$DEBUGFS_DIR/tune_history" > /dev/null 2>&1 && pass "tune_history 调优后可读" || fail "tune_history 调优后可读"

    # 历史应包含 action 和时间戳
    local content
    content=$(cat "$DEBUGFS_DIR/tune_history" 2>/dev/null)
    echo "$content" | grep -q "action=" && pass "tune_history 包含 action" || fail "tune_history 包含 action"
}

# ============================================================
# 执行
# ============================================================
echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 单元测试 - debugfs 接口  ║"
echo "╚══════════════════════════════════════════╝"

ensure_module || { echo "FATAL: 无法加载模块"; exit 1; }

if test_debugfs_available; then
    test_status
    test_hotspots
    test_tune_history
else
    skip "所有 debugfs 测试（debugfs 不可用）"
fi

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
