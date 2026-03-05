# xv6 数据分析系统使用说明

## 概述

本系统用于自动化收集和分析 xv6 操作系统优化项目的性能数据，生成论文级别的图表和报告。

## 快速开始

### 1. 安装依赖

```bash
pip install -r tools/requirements.txt
```

### 2. 一键运行

```bash
make analyze
```

这将自动执行：
1. 数据收集（baseline 和 testing 分支）
2. 数据解析
3. 图表生成
4. 报告生成

### 3. 查看结果

- **数据**: `data/results.json`
- **图表**: `figures/*.png` 和 `figures/*.pdf`
- **报告**: `reports/experiment_report.md`

---

## 分步执行

### 步骤 1: 数据收集

```bash
# 收集 baseline 分支数据
make collect
# 或
./tools/collect_data.sh baseline

# 收集 testing 分支数据
./tools/collect_data.sh testing

# 收集所有分支数据
./tools/collect_data.sh all
```

**注意**: 数据收集需要 `expect` 工具来自动化 QEMU 交互。

安装 expect:
```bash
# Ubuntu/Debian
sudo apt-get install expect

# macOS
brew install expect
```

### 步骤 2: 数据解析

```bash
make parse
# 或
python3 tools/parse_results.py
```

输出: `data/results.json`

### 步骤 3: 图表生成

```bash
make plot
# 或
python3 tools/plot_figures.py
```

输出:
- `figures/fig1.png` + `fig1.pdf`: COW 性能对比
- `figures/fig2.png` + `fig2.pdf`: Lazy Allocation 性能对比
- `figures/fig3.png` + `fig3.pdf`: Buddy System 性能对比

### 步骤 4: 报告生成

```bash
make report
# 或
python3 tools/generate_report.py
```

输出: `reports/experiment_report.md`

---

## 配置文件

### config.yaml

配置文件位于 `tools/config.yaml`，包含以下部分:

#### 1. 测试配置

```yaml
tests:
  - id: COW_1_no_access
    name: "COW: No Access"
    category: COW
    description: "fork后子进程立即退出（零复制）"
```

#### 2. 图表配置

```yaml
figures:
  - id: fig1
    type: line
    title: "Copy-on-Write Performance Comparison"
    data:
      - COW_1_no_access
      - COW_2_readonly
      - COW_3_partial
      - COW_4_fullwrite
```

#### 3. 质量配置

```yaml
quality:
  dpi: 300
  format:
    - png
    - pdf
  style: "seaborn-v0_8-darkgrid"
```

---

## 输出格式

### 数据格式 (results.json)

```json
{
  "metadata": {
    "baseline_branch": "baseline",
    "testing_branch": "testing",
    "timestamp": "2026-03-05T10:30:00"
  },
  "results": {
    "COW_1_no_access": {
      "baseline": {"avg": 18, "std": 2, "unit": "ticks"},
      "testing": {"avg": 5, "std": 1, "unit": "ticks"},
      "improvement": 72.2,
      "assessment": "显著提升"
    }
  }
}
```

### 图表质量

- **分辨率**: 300 DPI（论文级别）
- **格式**: PNG（插入 Word）+ PDF（LaTeX）
- **样式**: seaborn-v0_8-darkgrid（专业风格）

---

## 常见问题

### Q1: expect not found

**错误**: `[WARNING] expect not found, please run manually`

**解决**: 安装 expect 工具
```bash
# Ubuntu/Debian
sudo apt-get install expect

# macOS
brew install expect
```

### Q2: Python 依赖缺失

**错误**: `ModuleNotFoundError: No module named 'yaml'`

**解决**: 安装依赖
```bash
pip install -r tools/requirements.txt
```

### Q3: 数据文件不存在

**错误**: `[ERROR] data/baseline.log not found`

**解决**: 先运行数据收集
```bash
./tools/collect_data.sh baseline
```

### Q4: 图表样式错误

**错误**: `ValueError: style seaborn-v0_8-darkgrid not found`

**解决**: 代码会自动降级到默认样式，不影响使用

---

## 清理数据

```bash
make clean-analysis
```

这将删除:
- `data/` 目录
- `figures/` 目录
- `reports/` 目录

---

## 目录结构

```
xv6/
├── tools/
│   ├── config.yaml            # 配置文件
│   ├── collect_data.sh        # 数据收集脚本
│   ├── parse_results.py       # 数据解析
│   ├── plot_figures.py        # 图表生成
│   ├── generate_report.py     # 报告生成
│   ├── requirements.txt       # Python 依赖
│   └── README.md              # 本文档
│
├── data/
│   ├── baseline.log           # baseline 原始日志
│   ├── testing.log            # testing 原始日志
│   └── results.json           # 结构化数据
│
├── figures/
│   ├── fig1.png               # COW 性能对比（300 DPI）
│   ├── fig1.pdf               # COW 性能对比（PDF）
│   ├── fig2.png               # Lazy 性能对比
│   ├── fig2.pdf
│   ├── fig3.png               # Buddy 性能对比
│   └── fig3.pdf
│
└── reports/
    └── experiment_report.md   # 实验报告
```

---

## 技术细节

### 数据收集流程

1. 切换到目标分支（baseline/testing）
2. 编译 xv6
3. 启动 QEMU
4. 自动运行 `perftest` 命令
5. 保存输出到日志文件
6. 切换回原分支

### 数据解析流程

1. 使用正则表达式提取 `RESULT:` 行
2. 验证数据完整性
3. 计算性能提升百分比
4. 评估改进效果（显著提升/明显提升/小幅提升/基本持平/性能下降）

### 图表生成流程

1. 加载配置和结果数据
2. 设置 matplotlib 样式
3. 根据配置生成折线图或柱状图
4. 添加数值标注和图例
5. 保存为 PNG（300 DPI）和 PDF 格式

---

## 扩展开发

### 添加新的测试项

1. 在 `tools/config.yaml` 的 `tests` 部分添加配置
2. 在 `user/perftest.c` 中实现测试
3. 重新运行 `make analyze`

### 添加新的图表

1. 在 `tools/config.yaml` 的 `figures` 部分添加配置
2. 指定图表类型（line/bar）和数据源
3. 重新运行 `make plot`

### 自定义图表样式

修改 `tools/config.yaml` 的 `quality` 部分:

```yaml
quality:
  dpi: 300
  format:
    - png
    - pdf
  style: "seaborn-v0_8-darkgrid"
  font_size: 12
  figure_size: [10, 6]
```

---

## 许可证

本项目遵循 xv6 的原始许可证。

---

## 联系方式

如有问题，请查看项目文档或提交 Issue。
