#!/bin/bash
# SmartMemEngine 一键运行全部测试

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TESTS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$TESTS_DIR/.." && pwd)"
REPORTS_DIR="$TESTS_DIR/reports"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

# 创建报告目录
mkdir -p "$REPORTS_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SUMMARY_FILE="$REPORTS_DIR/summary_${TIMESTAMP}.txt"

TOTAL_PASS=0; TOTAL_FAIL=0; TOTAL_SKIP=0
TESTS_RUN=0; TESTS_PASS=0; TESTS_FAIL=0

run_test() {
    local test_name="$1"
    local test_script="$2"
    local report_file="$REPORTS_DIR/${test_name}_${TIMESTAMP}.txt"

    echo ""
    echo -e "${YELLOW}▶ 运行: $test_name${NC}"
    echo "  脚本: $test_script"
    echo "  报告: $report_file"
    echo ""

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -x "$test_script" ]; then
        echo -e "  ${RED}SKIP: $test_script 不可执行${NC}"
        TOTAL_SKIP=$((TOTAL_SKIP + 1))
        return
    fi

    # 运行测试并记录输出
    bash "$test_script" 2>&1 | tee "$report_file"
    local exit_code=${PIPESTATUS[0]}

    # 从输出中提取结果
    local pass_count fail_count skip_count
    pass_count=$(grep -oP '通过: \K\d+' "$report_file" 2>/dev/null | tail -1)
    fail_count=$(grep -oP '失败: \K\d+' "$report_file" 2>/dev/null | tail -1)
    skip_count=$(grep -oP '跳过: \K\d+' "$report_file" 2>/dev/null | tail -1)

    [ -z "$pass_count" ] && pass_count=0
    [ -z "$fail_count" ] && fail_count=0
    [ -z "$skip_count" ] && skip_count=0

    TOTAL_PASS=$((TOTAL_PASS + pass_count))
    TOTAL_FAIL=$((TOTAL_FAIL + fail_count))
    TOTAL_SKIP=$((TOTAL_SKIP + skip_count))

    if [ "$exit_code" -eq 0 ]; then
        echo -e "  ${GREEN}✓ $test_name 通过 (PASS: $pass_count, FAIL: $fail_count)${NC}"
        TESTS_PASS=$((TESTS_PASS + 1))
    else
        echo -e "  ${RED}✗ $test_name 失败 (PASS: $pass_count, FAIL: $fail_count)${NC}"
        TESTS_FAIL=$((TESTS_FAIL + 1))
    fi
}

# ============================================================
# 环境检查
# ============================================================
echo "╔══════════════════════════════════════════════╗"
echo "║        SmartMemEngine 全量测试               ║"
echo "║        $(date '+%Y-%m-%d %H:%M:%S')                ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# 检查 root 权限
if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}ERROR: 需要 root 权限运行测试${NC}"
    exit 1
fi

# 检查模块存在
if [ ! -f "$PROJECT_DIR/src/smartmem.ko" ]; then
    echo -e "${RED}ERROR: smartmem.ko 不存在，请先编译模块${NC}"
    exit 1
fi

# 环境初始化
echo "初始化测试环境..."
bash "$SCRIPT_DIR/setup_env.sh" || {
    echo -e "${YELLOW}WARNING: 环境初始化失败，部分测试可能无法运行${NC}"
}

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  阶段 1: 单元测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

run_test "unit_procfs" "$TESTS_DIR/unit/test_procfs.sh"
run_test "unit_debugfs" "$TESTS_DIR/unit/test_debugfs.sh"
run_test "unit_tools" "$TESTS_DIR/unit/test_tools.sh"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  阶段 2: 集成测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

run_test "integration_load_unload" "$TESTS_DIR/integration/test_load_unload.sh"
run_test "integration_monitor_flow" "$TESTS_DIR/integration/test_monitor_flow.sh"
run_test "integration_autotune_flow" "$TESTS_DIR/integration/test_autotune_flow.sh"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  阶段 3: 压力测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 压力测试默认运行，可通过 STRESS_SKIP=1 跳过
if [ "${STRESS_SKIP:-0}" -eq 1 ]; then
    echo -e "${YELLOW}跳过压力测试（STRESS_SKIP=1）${NC}"
else
    run_test "stress_memory_pressure" "$TESTS_DIR/stress/test_memory_pressure.sh"
    run_test "stress_stability" "$TESTS_DIR/stress/test_stability.sh 300"
    run_test "stress_reload" "$TESTS_DIR/stress/test_reload_stress.sh 20"
fi

# ============================================================
# 汇总报告
# ============================================================
echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║              测试结果汇总                     ║"
echo "╚══════════════════════════════════════════════╝"
echo ""
echo "  测试套件: $TESTS_RUN 运行, $TESTS_PASS 通过, $TESTS_FAIL 失败"
echo "  用例总计: $((TOTAL_PASS + TOTAL_FAIL + TOTAL_SKIP))"
echo -e "  ${GREEN}通过: $TOTAL_PASS${NC}"
echo -e "  ${RED}失败: $TOTAL_FAIL${NC}"
echo -e "  ${YELLOW}跳过: $TOTAL_SKIP${NC}"
echo ""
echo "  报告目录: $REPORTS_DIR"
echo ""

# 写入汇总文件
{
    echo "SmartMemEngine 测试汇总"
    echo "时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "测试套件: 运行=$TESTS_RUN 通过=$TESTS_PASS 失败=$TESTS_FAIL"
    echo "用例: 通过=$TOTAL_PASS 失败=$TOTAL_FAIL 跳过=$TOTAL_SKIP"
} > "$SUMMARY_FILE"

echo "汇总文件: $SUMMARY_FILE"

if [ $TOTAL_FAIL -gt 0 ] || [ $TESTS_FAIL -gt 0 ]; then
    echo -e "${RED}存在失败用例!${NC}"
    exit 1
else
    echo -e "${GREEN}全部通过!${NC}"
    exit 0
fi
