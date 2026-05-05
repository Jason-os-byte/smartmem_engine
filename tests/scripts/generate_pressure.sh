#!/bin/bash
# SmartMemEngine 内存压力生成器
# 用法: generate_pressure.sh [模式] [持续时间秒] [强度]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TESTS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$TESTS_DIR/.." && pwd)"

MODE=${1:-mixed}
DURATION=${2:-60}
INTENSITY=${3:-2}

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

usage() {
    echo "SmartMemEngine 内存压力生成器"
    echo ""
    echo "用法: $0 [模式] [持续时间] [强度]"
    echo ""
    echo "模式:"
    echo "  buddy     - 大块内存分配（触发 buddy allocator）"
    echo "  slub      - 小块内存分配（触发 kmalloc/kfree）"
    echo "  fragment  - 碎片化压力（交替大小分配）"
    echo "  numa      - NUMA 跨节点分配"
    echo "  oom       - OOM 临近压力"
    echo "  mixed     - 混合压力（默认）"
    echo "  wave      - 波浪式压力（逐渐增加/减少）"
    echo ""
    echo "持续时间: 秒数（默认 60）"
    echo "强度: 并发进程数 1-8（默认 2）"
    echo ""
    echo "示例:"
    echo "  $0 buddy 30 4     # 30秒 buddy 压力，4进程"
    echo "  $0 mixed 120 2    # 2分钟混合压力"
    echo "  $0 wave 300 1     # 5分钟波浪压力"
}

check_stress() {
    if command -v stress > /dev/null 2>&1; then
        return 0
    elif command -v stress-ng > /dev/null 2>&1; then
        return 0
    else
        echo -e "${RED}ERROR: 需要安装 stress 或 stress-ng${NC}"
        exit 1
    fi
}

# Buddy 分配压力
pressure_buddy() {
    echo -e "${GREEN}▶ Buddy 分配压力（${DURATION}秒，${INTENSITY}进程）${NC}"
    local bytes=$((512 / INTENSITY))
    for i in $(seq 1 $INTENSITY); do
        stress --vm 1 --vm-bytes "${bytes}M" --timeout "${DURATION}s" 2>/dev/null &
    done
    wait 2>/dev/null
}

# SLUB 分配压力
pressure_slub() {
    echo -e "${GREEN}▶ SLUB 分配压力（${DURATION}秒，${INTENSITY}进程）${NC}"
    for i in $(seq 1 $INTENSITY); do
        stress --vm 2 --vm-bytes 64M --timeout "${DURATION}s" 2>/dev/null &
    done
    wait 2>/dev/null
}

# 碎片化压力
pressure_fragment() {
    echo -e "${GREEN}▶ 碎片化压力（${DURATION}秒）${NC}"
    local end_time=$(( $(date +%s) + DURATION ))
    while [ "$(date +%s)" -lt "$end_time" ]; do
        # 大块
        stress --vm 1 --vm-bytes 512M --timeout 5s 2>/dev/null &
        wait 2>/dev/null
        # 小块
        stress --vm 4 --vm-bytes 16M --timeout 5s 2>/dev/null &
        wait 2>/dev/null
    done
}

# NUMA 压力
pressure_numa() {
    echo -e "${GREEN}▶ NUMA 跨节点压力（${DURATION}秒）${NC}"
    if ! command -v numactl > /dev/null 2>&1; then
        echo -e "${YELLOW}numactl 不可用，回退到普通分配${NC}"
        stress --vm $INTENSITY --vm-bytes 128M --timeout "${DURATION}s" 2>/dev/null
        return
    fi

    local num_nodes
    num_nodes=$(numactl --hardware 2>/dev/null | grep "available:" | awk '{print $2}')

    if [ -z "$num_nodes" ] || [ "$num_nodes" -le 1 ]; then
        echo -e "${YELLOW}单 NUMA 节点，回退到普通分配${NC}"
        stress --vm $INTENSITY --vm-bytes 128M --timeout "${DURATION}s" 2>/dev/null
        return
    fi

    for node in $(seq 0 $((num_nodes - 1))); do
        numactl --membind="$node" stress --vm 1 --vm-bytes 128M --timeout "${DURATION}s" 2>/dev/null &
    done
    wait 2>/dev/null
}

# OOM 临近压力
pressure_oom() {
    echo -e "${GREEN}▶ OOM 临近压力（${DURATION}秒）${NC}"
    local total_mem
    total_mem=$(grep "MemTotal" /proc/meminfo | awk '{print $2}')
    local target_mb=$((total_mem / 1024 - 128))
    stress --vm 2 --vm-bytes "${target_mb}M" --timeout "${DURATION}s" 2>/dev/null
}

# 混合压力
pressure_mixed() {
    echo -e "${GREEN}▶ 混合压力（${DURATION}秒，${INTENSITY}进程）${NC}"
    local half=$((DURATION / 2))

    # 前半段 buddy 压力
    stress --vm $INTENSITY --vm-bytes 256M --timeout "${half}s" 2>/dev/null &
    local pid1=$!

    # 前半段 slub 压力
    stress --vm $INTENSITY --vm-bytes 64M --timeout "${half}s" 2>/dev/null &
    local pid2=$!

    wait $pid1 $pid2 2>/dev/null

    # 后半段碎片化
    pressure_fragment &
    local frag_pid=$!

    # 后半段同时 OOM
    local remaining=$((DURATION - half))
    stress --vm 1 --vm-bytes 512M --timeout "${remaining}s" 2>/dev/null &

    wait 2>/dev/null
}

# 波浪式压力
pressure_wave() {
    echo -e "${GREEN}▶ 波浪式压力（${DURATION}秒）${NC}"
    local end_time=$(( $(date +%s) + DURATION ))
    local wave=0
    local direction=1

    while [ "$(date +%s)" -lt "$end_time" ]; do
        # 波浪大小: 64M -> 512M -> 64M ...
        local size=$((64 + wave * 64))
        [ "$size" -gt 512 ] && size=512
        [ "$size" -lt 64 ] && size=64

        echo "  波浪: ${size}M (wave=$wave)"

        stress --vm 1 --vm-bytes "${size}M" --timeout 5s 2>/dev/null &
        wait 2>/dev/null

        wave=$((wave + direction))
        [ "$wave" -ge 7 ] && direction=-1
        [ "$wave" -le 0 ] && direction=1
    done
}

# ============================================================
# 主入口
# ============================================================
case "$1" in
    -h|--help|help)
        usage
        exit 0
        ;;
esac

check_stress

echo "╔══════════════════════════════════════════╗"
echo "║  SmartMemEngine 内存压力生成器             ║"
echo "║  模式: $MODE    持续: ${DURATION}秒    强度: $INTENSITY"
echo "╚══════════════════════════════════════════╝"
echo ""

# 记录初始状态
local free_before
free_before=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
echo "初始状态: MemFree=${free_before}KB"

case "$MODE" in
    buddy)     pressure_buddy ;;
    slub)      pressure_slub ;;
    fragment)  pressure_fragment ;;
    numa)      pressure_numa ;;
    oom)       pressure_oom ;;
    mixed)     pressure_mixed ;;
    wave)      pressure_wave ;;
    *)
        echo -e "${RED}未知模式: $MODE${NC}"
        usage
        exit 1
        ;;
esac

# 最终状态
local free_after
free_after=$(grep "MemFree" /proc/meminfo | awk '{print $2}')
echo ""
echo "最终状态: MemFree=${free_after}KB"
echo -e "${GREEN}压力生成完成${NC}"
