#!/bin/bash
# SmartMemEngine 性能影响测试
# 测量模块加载前后内存操作延迟变化

MODULE_PATH="/root/project/smartmem_engine/src/smartmem.ko"
ITERATIONS=100000

echo "SmartMemEngine Performance Impact Test"
echo "======================================="
echo ""

# 编译微基准测试程序
cat > /tmp/mem_bench.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int iterations = argc > 1 ? atoi(argv[1]) : 100000;
    int size = argc > 2 ? atoi(argv[2]) : 256;
    int i;
    struct timespec start, end;
    long long total_ns;
    void *ptr;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        ptr = malloc(size);
        if (ptr) {
            memset(ptr, 0, size);
            free(ptr);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    total_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
               (end.tv_nsec - start.tv_nsec);

    printf("iterations=%d size=%d total=%lldns avg=%lldns\n",
           iterations, size, total_ns, total_ns / iterations);
    return 0;
}
EOF

gcc -O2 -o /tmp/mem_bench /tmp/mem_bench.c

echo "1. Baseline (module not loaded)"
echo "-------------------------------"
if lsmod | grep -q smartmem; then
    rmmod smartmem 2>/dev/null
    sleep 1
fi

for size in 64 256 1024 4096; do
    echo -n "  malloc/free size=$size: "
    /tmp/mem_bench $ITERATIONS $size
done

echo ""
echo "2. With module loaded"
echo "---------------------"
insmod "$MODULE_PATH" 2>/dev/null
sleep 2

for size in 64 256 1024 4096; do
    echo -n "  malloc/free size=$size: "
    /tmp/mem_bench $ITERATIONS $size
done

echo ""
echo "3. Module overhead summary"
echo "--------------------------"
echo "Compare 'avg' values between baseline and with-module."
echo "Expected overhead: < 5% for most workloads."
echo ""

# 清理
rm -f /tmp/mem_bench /tmp/mem_bench.c
