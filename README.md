# SmartMemEngine - 智能内存优化引擎

## 项目简介

SmartMemEngine 是一个工业级 Linux 内核内存优化引擎，基于 Linux 6.12 内核模块实现，通过 kprobe/kretprobe Hook 内存管理子系统，应用智能优化策略，实现内存热点的实时识别、瓶颈分析和自动调优。

**核心能力**：
- **实时监控**：通过 kprobe/tracepoint 零侵入采集 Buddy/SLUB/NUMA 内存事件
- **智能分析**：热点识别（调用栈聚合+评分衰减）、瓶颈检测（5类）、根因分析（5类）
- **自动调优**：内存整理(compaction)、水位线调整、页缓存/SLAB回收
- **趋势预测**：基于线性回归的 OOM 风险预测和内存耗尽时间估算
- **策略引擎**：NUMA感知Buddy分配、自适应SLUB缓存、多代LRU、NUMA负载均衡

## 系统要求

- **内核版本**：Linux 6.12+
- **架构**：x86_64
- **编译工具**：gcc, make
- **内核源码**：`/lib/modules/$(uname -r)/build`（需包含有效 Module.symvers）

## 目录结构

```
smartmem_engine/
├── src/
│   ├── main.c                 # 模块入口
│   ├── Makefile               # 内核模块编译
│   ├── core/                  # 引擎核心（引擎、配置、统计）
│   ├── hooks/                 # Hook层（buddy、slub、vma、lru、numa）
│   ├── strategy/              # 策略引擎和4个内置策略
│   ├── monitor/               # 监控系统（eBPF框架、tracepoint）
│   ├── analysis/              # 分析引擎（热点、瓶颈、根因）
│   ├── optimization/          # 优化引擎（自动调优、预测模型）
│   ├── interface/             # 接口层（procfs、debugfs）
│   └── tools/                 # 用户空间工具
│       ├── memctl.c           # 配置管理
│       ├── memstat.c          # 统计查看
│       ├── memview.c          # 可视化展示
│       ├── test_smartmem.sh   # 集成测试
│       ├── test_perf.sh       # 性能测试
│       └── Makefile
└── docs/                      # 设计文档
```

## 构建与安装

### 编译内核模块

```bash
cd src
make
```

### 编译用户空间工具

```bash
cd src/tools
make
```

### 安装工具到系统

```bash
cd src/tools
make install
```

## 使用指南

### 加载/卸载模块

```bash
# 加载
insmod smartmem.ko

# 卸载
rmmod smartmem

# 查看加载日志
dmesg | grep smartmem
```

### procfs 接口

| 接口 | 功能 | 权限 |
|------|------|------|
| `/proc/smartmem/config` | 配置信息 | 读写 |
| `/proc/smartmem/stats` | 统计信息 | 只读 |
| `/proc/smartmem/policies` | 策略状态 | 只读 |
| `/proc/smartmem/hotspots` | Top-N 分配热点 | 只读 |
| `/proc/smartmem/bottlenecks` | 瓶颈分析 | 只读 |
| `/proc/smartmem/rootcauses` | 根因分析 | 只读 |
| `/proc/smartmem/autotune` | 自动调优状态 | 读写 |
| `/proc/smartmem/prediction` | 内存预测 | 只读 |
| `/proc/smartmem/control` | 控制命令 | 读写 |

### control 命令

```bash
# 启用/禁用功能
echo 'enable auto_tune_enabled' > /proc/smartmem/control
echo 'disable trace_enabled' > /proc/smartmem/control

# 重置
echo 'reset stats' > /proc/smartmem/control
echo 'reset hotspots' > /proc/smartmem/control
echo 'reset prediction' > /proc/smartmem/control

# 手动调优
echo 'tune compact' > /proc/smartmem/control
echo 'tune slab' > /proc/smartmem/control
```

### 自动调优写入

```bash
echo 'compact' > /proc/smartmem/autotune
echo 'watermark' > /proc/smartmem/autotune
echo 'numa' > /proc/smartmem/autotune
echo 'slab' > /proc/smartmem/autotune
```

### debugfs 接口

| 接口 | 功能 |
|------|------|
| `/sys/kernel/debug/smartmem/status` | 完整状态 dump |
| `/sys/kernel/debug/smartmem/hotspots` | 原始热点数据（含未解析栈） |
| `/sys/kernel/debug/smartmem/tune_history` | 调优历史详情 |

### 用户空间工具

**memctl** - 配置管理：
```bash
memctl config                        # 查看所有配置
memctl config get hook_buddy_enabled # 获取单个配置
memctl config set hook_buddy_enabled=true  # 设置配置
memctl enable auto_tune_enabled      # 启用功能
memctl disable auto_tune_enabled     # 禁用功能
memctl reset stats                   # 重置统计
memctl tune compact                  # 手动触发内存整理
```

**memstat** - 统计查看：
```bash
memstat stats        # 分配统计
memstat policies     # 策略状态
memstat hotspots     # 热点调用栈
memstat bottlenecks  # 瓶颈分析
memstat rootcauses   # 根因分析
memstat autotune     # 调优历史
memstat prediction   # 内存预测
memstat summary      # 一行摘要
memstat all          # 全部信息
```

**memview** - 可视化：
```bash
memview overview    # 彩色概览（内存条、NUMA局部性）
memview bar         # 内存分段条形图
memview top         # 实时监控（2秒刷新）
memview watch 5     # watch 模式（带增量，5秒刷新）
```

## 自动化测试

```bash
# 集成测试（约30秒）
cd src/tools
bash test_smartmem.sh

# 性能影响测试
bash test_perf.sh
```

## 常见问题

### _printk symbol version mismatch

如果 `insmod` 报错 `disagrees about version of symbol _printk`，说明 `Module.symvers` 中 CRC 值为零。运行修复脚本：

```bash
cd src/tools
bash fix_symvers.sh
cd ..
make clean && make
```

## 许可证

GPL v2

## 作者

Jason

## 版本

1.0.0
