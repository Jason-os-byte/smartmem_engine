#!/bin/bash
# SmartMemEngine 测试环境初始化

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TESTS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$TESTS_DIR/.." && pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  SmartMemEngine 测试环境初始化"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

ERRORS=0

# 1. 检查 root 权限
echo ""
echo "[1/7] 检查 root 权限..."
if [ "$(id -u)" -eq 0 ]; then
    echo -e "  ${GREEN}✓ root 权限正常${NC}"
else
    echo -e "  ${RED}✗ 需要 root 权限${NC}"
    ERRORS=$((ERRORS + 1))
fi

# 2. 检查内核版本
echo ""
echo "[2/7] 检查内核版本..."
KVER=$(uname -r)
echo "  当前内核: $KVER"
if [[ "$KVER" == 6.12.* ]]; then
    echo -e "  ${GREEN}✓ 内核版本匹配 (6.12.x)${NC}"
else
    echo -e "  ${YELLOW}⚠ 内核版本非 6.12.x，可能存在兼容性问题${NC}"
fi

# 3. 检查模块编译产物
echo ""
echo "[3/7] 检查模块编译产物..."
if [ -f "$PROJECT_DIR/src/smartmem.ko" ]; then
    echo -e "  ${GREEN}✓ smartmem.ko 存在${NC}"
else
    echo -e "  ${RED}✗ smartmem.ko 不存在，请先编译${NC}"
    ERRORS=$((ERRORS + 1))
fi

# 4. 检查用户空间工具
echo ""
echo "[4/7] 检查用户空间工具..."
for tool in memctl memstat memview; do
    if [ -x "$PROJECT_DIR/src/tools/$tool" ]; then
        echo -e "  ${GREEN}✓ $tool 已编译${NC}"
    else
        echo -e "  ${YELLOW}⚠ $tool 不存在或不可执行（相关测试将跳过）${NC}"
    fi
done

# 5. 检查依赖工具
echo ""
echo "[5/7] 检查依赖工具..."
for cmd in insmod rmmod lsmod dmesg grep awk cat; do
    if command -v "$cmd" > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ $cmd 可用${NC}"
    else
        echo -e "  ${RED}✗ $cmd 不可用${NC}"
        ERRORS=$((ERRORS + 1))
    fi
done

# 检查可选工具
for cmd in stress stress-ng numactl; do
    if command -v "$cmd" > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ $cmd 可用（可选）${NC}"
    else
        echo -e "  ${YELLOW}⚠ $cmd 不可用（部分压力测试将跳过）${NC}"
    fi
done

# 6. 检查 procfs/debugfs 可用性
echo ""
echo "[6/7] 检查 procfs/debugfs..."
if [ -d "/proc" ]; then
    echo -e "  ${GREEN}✓ /proc 可访问${NC}"
else
    echo -e "  ${RED}✗ /proc 不可访问${NC}"
    ERRORS=$((ERRORS + 1))
fi

if [ -d "/sys/kernel/debug" ]; then
    echo -e "  ${GREEN}✓ debugfs 已挂载${NC}"
else
    echo -e "  ${YELLOW}⚠ debugfs 未挂载，尝试挂载...${NC}"
    mount -t debugfs none /sys/kernel/debug 2>/dev/null && echo -e "  ${GREEN}✓ debugfs 挂载成功${NC}" || echo -e "  ${RED}✗ debugfs 挂载失败${NC}"
fi

# 7. 清理旧模块
echo ""
echo "[7/7] 清理旧模块..."
if lsmod | grep -q smartmem; then
    echo "  卸载现有 smartmem 模块..."
    rmmod smartmem 2>/dev/null && echo -e "  ${GREEN}✓ 旧模块已卸载${NC}" || echo -e "  ${YELLOW}⚠ 旧模块卸载失败${NC}"
else
    echo -e "  ${GREEN}✓ 无旧模块需要卸载${NC}"
fi

# 清理 dmesg
dmesg -c > /dev/null 2>&1

# 创建报告目录
mkdir -p "$TESTS_DIR/reports"

# 汇总
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ $ERRORS -eq 0 ]; then
    echo -e "  ${GREEN}环境初始化完成，无错误${NC}"
    exit 0
else
    echo -e "  ${RED}环境初始化完成，$ERRORS 个错误${NC}"
    exit 1
fi
