# SmartMemEngine - 智能内存优化引擎

## 项目简介

SmartMemEngine 是一个工业级 Linux 内核内存优化引擎，通过 Hook 内存管理子系统，应用智能优化策略，提升数据库、大数据、容器等场景的内存访问问题的能力。

本项目旨在解决以下场景的内存问题：
- **数据库场景**：减少跨NUMA节点内存访问，提升查询性能
- **大数据场景**：优化内存分配模式，降低延迟和碎片
- **容器场景**：改善容器内存隔离和资源限制效果

## 功能特性

- **Buddy 分配优化**：NUMA 感知分配，减少跨节点访问
- **SLUB 分配优化**：自适应缓存大小，提升小对象分配效率
- **页面管理优化**：多代 LRU 策略，改善页面置换
- **实时监控**：eBPF/tracepoint 零开销监控
- **智能分析**：热点识别、瓶颈分析、根因定位
- **自动调优**：根据负载动态调整优化策略
- **灵活接口**：procfs/debugfs/netlink 多种接口

## 系统要求

- **内核版本**：Linux 6.x
- **编译工具**：gcc, make
- **内核头文件**：`/lib/modules/$(uname -r)/build`
- **可选工具**：clang/llvm（用于 eBPF 编译）

## 目录结构

```
smartmem_engine/
├── README.md              # 本文件
├── Makefile               # 编译文件
├── smartmem.h             # 核心头文件
├── smartmem.c             # 模块主入口
├── core/                  # 核心模块（引擎、配置、统计）
├── hooks/                 # Hook层（buddy、slub、vma、lru、numa）
├── strategy/              # 策略引擎和内置策略
├── monitor/               # 监控系统（eBPF、tracepoint）
├── analysis/              # 分析引擎（热点、瓶颈、根因）
├── optimization/          # 优化引擎（自动调优、预测）
├── interface/             # 接口层（procfs、debugfs）
└── tools/                 # 用户空间工具（memctl、memstat、memview）
```

## 构建编译

### 编译模块

```bash
cd smartmem_engine
make
```

编译成功后会生成 `smartmem.ko` 内核模块文件。

### 清理编译文件

```bash
make clean
```

### 安装模块到系统

```bash
sudo make install
```

这会将模块安装到 `/lib/modules/$(uname -r)/extra/` 目录。

## 安装与使用

### 加载模块

```bash
sudo insmod smartmem.ko
```

查看模块加载状态：

```bash
dmesg | tail
lsmod | grep smartmem
```

### 验证模块运行

查看 procfs 接口：

```bash
# 查看模块配置
cat /proc/smartmem/config

# 查看内存分配统计
cat /proc/smartmem/stats

# 查看当前策略
cat /proc/smartmem/policies
```

### 配置模块

启用 NUMA 感知策略：

```bash
echo "numa_aware_enabled=1" | sudo tee /proc/smartmem/config
```

启用自动调优：

```bash
echo "auto_tune_enabled=1" | sudo tee /proc/smartmem/config
```

### 卸载模块

```bash
sudo rmmod smartmem
```

## 接口说明

### procfs 接口

| 接口 | 功能 | 操作 |
|------|------|------|
| `/proc/smartmem/config` | 配置信息 | 读取/写入 |
| `/proc/smartmem/stats` | 统计信息 | 只读 |
| `/proc/smartmem/hotspots` | 热点调用栈 | 只读 |
| `/proc/smartmem/policies` | 策略信息 | 只读 |
| `/proc/smartmem/control` | 控制接口 | 读取/写入 |

### debugfs 接口

| 接口 | 功能 |
|------|------|
| `/sys/kernel/debug/smartmem/dump` | 状态 dump |
| `/sys/kernel/debug/smartmem/internal` | 内部数据导出 |

### 用户空间工具

- **memctl**：配置管理工具
  ```bash
  sudo ./tools/memctl set numa_aware_enabled 1
  sudo ./tools/memctl get numa_aware_enabled
  ```

- **memstat**：统计查看工具
  ```bash
  sudo ./tools/memstat --buddy
  sudo ./tools/memstat --slub
  ```

- **memview**：可视化展示工具
  ```bash
  sudo ./tools/memview --hotspots
  ```

## 许可证

GPL v2

## 作者

Jason

## 版本

1.0.0
