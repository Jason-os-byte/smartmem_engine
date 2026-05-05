# SmartMemEngine 测试方案

## 1. 测试目录结构

```
tests/
├── README.md              # 本文件：测试方案总览
├── unit/                  # 单元测试（procfs 接口逐一验证）
│   ├── test_procfs.sh     # procfs 9 个接口的完整测试
│   ├── test_debugfs.sh    # debugfs 3 个接口的完整测试
│   └── test_tools.sh      # 用户空间工具的完整测试
├── integration/           # 集成测试（模块协同、端到端流程）
│   ├── test_load_unload.sh    # 模块加载/卸载生命周期
│   ├── test_monitor_flow.sh   # 监控→分析→优化 全链路
│   └── test_autotune_flow.sh  # 自动调优闭环验证
├── stress/                # 压力测试与稳定性
│   ├── test_memory_pressure.sh   # 内存压力场景
│   ├── test_stability.sh         # 长时间运行稳定性
│   └── test_reload_stress.sh     # 反复加载/卸载压力
├── scripts/               # 测试辅助脚本
│   ├── run_all.sh             # 一键运行全部测试
│   ├── setup_env.sh           # 测试环境初始化
│   └── generate_pressure.sh   # 内存压力生成器
└── reports/               # 测试报告输出目录
```

## 2. 测试矩阵

| 测试类别 | 用例数 | 覆盖模块 | 执行时间 |
|---------|--------|---------|---------|
| 单元测试 | 68 | procfs/debugfs/tools | ~2min |
| 集成测试 | 18 | 全链路/生命周期/调优 | ~5min |
| 压力测试 | 12 | 稳定性/极端场景 | ~15min |

## 3. 执行方式

```bash
# 运行全部测试
cd tests && bash scripts/run_all.sh

# 运行单类测试
bash tests/unit/test_procfs.sh
bash tests/integration/test_load_unload.sh
bash tests/stress/test_memory_pressure.sh
```

## 4. 前置条件

- 内核模块源码已编译：`src/smartmem.ko` 存在
- 用户空间工具已编译：`src/tools/memctl memstat memview` 存在
- root 权限（insmod/rmmod/debugfs 需要）
- 可选：stress-ng（压力测试场景）
