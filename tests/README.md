# 测试目录

SKSL ↔ GLSL 双向转换测试基础设施，提供三种模式：
- **test**：自动化双向 round-trip 验证（Direction 1 + Direction 2）
- **pipeline**：灵活的单方向转换 + 渲染 + 动画
- **live**：实时 GUI 预览窗口（SDL2，壁钟时间驱动）

## 目录结构

```
tests/
├── shaders/              # SkSL 着色器源文件 (.sksl + .params.json)
├── frag/                 # GLSL 着色器源文件 (.frag)
├── assets/               # 测试纹理 (PNG)
│
├── runner.sh             # 主测试编排器 (test / pipeline / live)
├── params.sh             # Round-Trip 着色器参数配置 (被 runner.sh source)
│
├── render_sksl.py        # SkSL→PNG 渲染工具 (单帧)
├── render_animated.py    # SkSL 多帧动画渲染工具
├── render_ascii.py       # ASCII 终端帧查看器
└── compare_images.py     # 像素级图像对比工具 (PSNR/MSE)
```

## 依赖

| 工具 | 路径 | 用途 |
|------|------|------|
| `sksl_export_glsl_custom` | `skia/out/SkSL/` | SkSL→GLSL 转换 + provenance |
| `skslc` | `skia/out/stog/` | SkSL 编译器 (标准) |
| `glslang` | `glslang/build/StandAlone/` | GLSL→SPIR-V 编译器 |
| `glslang-to-sksl` | `glslang/build/glslang_demo/` | GLSL→SkSL 转换 + provenance |
| `render_glsl` | `glslang/glslang_demo/` | GLX+OpenGL GLSL 离线渲染器 |
| `render_glsl_live` | `glslang/glslang_demo/` | SDL2+OpenGL GLSL 实时 GUI 渲染 |
| `shader_live_preview.py` | `src/renderer/` | SDL2+skia-python SkSL 实时 GUI 渲染 |
| `uv` | 系统 PATH | Python 包管理器 |
| `convert` | ImageMagick | PPM↔PNG 转换 |
| `ffmpeg` | (可选) 系统 PATH | 帧序列→MP4 编码 |

### 一次性构建依赖

```bash
# 编译 GLSL 离线渲染器
gcc -std=c11 -O2 -Wall -o glslang/glslang_demo/render_glsl \
    glslang/glslang_demo/render_glsl.c -lepoxy -lX11 -lGL -lm

# 编译 GLSL 实时渲染器
gcc -O2 -o glslang/glslang_demo/render_glsl_live \
    glslang/glslang_demo/render_glsl_live.c \
    $(pkg-config --cflags --libs sdl2 epoxy) -lGL -lm

# 编译 sksl_export_glsl_custom (带 provenance 生成)
bash src/sksl_to_glsl/build.sh
```

---

## 快速开始

```bash
# 查看帮助
bash tests/runner.sh help

# 运行 Round-Trip 测试
bash tests/runner.sh test --shader passthrough

# 单方向 Pipeline 转换
bash tests/runner.sh pipeline tests/shaders/passthrough.sksl

# 实时 GUI 预览
bash tests/runner.sh live tests/frag/spread.frag --width 640 --height 360
```

---

# test 模式：双向 round-trip 测试

自动化 SkSL ↔ GLSL 双向转换 round-trip 验证，验证转译在像素级语义上是无损的。

## 用法

```bash
# 所有着色器，两个方向
bash tests/runner.sh test --all

# 单着色器
bash tests/runner.sh test --shader passthrough
bash tests/runner.sh test --shader water_ripple --shader displacement_distort

# 单个方向
bash tests/runner.sh test --dir1 --shader passthrough
bash tests/runner.sh test --dir2 --shader passthrough
```

## 测试流程

```
D1: SkSL ──S1──▶ PNG ──S2──▶ GLSL ──S3──▶ PNG ──S4──▶ SkSL ──S5──▶ PNG
                    ▲                                              │
                    └──────────── compare_images ──────────────────┘

D2: GLSL ──S1──▶ PNG ──S2──▶ SkSL ──S3──▶ PNG ──S4──▶ GLSL ──S5──▶ PNG
                    ▲                                              │
                    └──────────── compare_images ──────────────────┘
```

## 输出目录结构

```
results/t4/
├── v1/                          # Direction 1 产物
│   ├── sksl/                    # 原始 SkSL 渲染 PNG
│   ├── sksl_to_glsl/<shader>/  # SkSL→GLSL 产物 (.glsl + .provenance)
│   ├── to_glsl/                 # 中间 GLSL 渲染 PNG
│   ├── glsl_to_sksl/<shader>/  # GLSL→SkSL 产物 (.sksl + .provenance)
│   └── to_sksl/                 # 还原 SkSL 渲染 PNG
├── v12/                         # Direction 2 产物 (结构对称)
├── reports/                     # 对比 JSON 报告
└── tmp/                         # 临时文件 (PPM、日志等)
```

已有的产物会自动跳过 (SKIP)，便于增量重跑。

---

# pipeline 模式：单方向转换 + 渲染 + 动画

Pipeline 提供灵活的单方向转换流程：**before → convert → after**，支持单帧渲染、多帧动画、终端 ASCII 预览和 MP4 编码。

## 流水线结构

```
输入着色器
│
├── before/    原始着色器 + 渲染结果 (PNG / 帧序列)
├── code/      转换后的代码 (.glsl 或 .sksl + .provenance)
└── after/     转换后代码的渲染结果 (PNG / 帧序列)
```

对于 `sksl_to_glsl` 方向：before=SkSL渲染, code=GLSL代码, after=GLSL渲染
对于 `glsl_to_sksl` 方向：before=GLSL渲染, code=SkSL代码, after=SkSL渲染

## 用法

```bash
# 基本：转换并渲染单个 .sksl 文件
bash tests/runner.sh pipeline my_shader.sksl

# 转换 .frag / .glsl 文件
bash tests/runner.sh pipeline tests/frag/spread.frag --direction glsl_to_sksl

# 双向流水线 (SKSL→GLSL 再 GLSL→SKSL)
bash tests/runner.sh pipeline my_shader.sksl --direction bidirectional

# 动画模式
bash tests/runner.sh pipeline my_animated.sksl --fps 60 --duration 5

# 动画模式 + 禁用 ASCII 终端预览
bash tests/runner.sh pipeline my_animated.sksl --fps 30 --no-ascii

# 强制重新渲染 (覆盖已有产物)
bash tests/runner.sh pipeline my_shader.sksl --force

# 自定义输出目录
bash tests/runner.sh pipeline my_shader.sksl --output results/my_test

# 使用显式配置文件
bash tests/runner.sh pipeline my_shader.sksl --config my.params.json
```

## CLI 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--config FILE` | 显式配置文件路径 | 自动检测 `<name>.params.json` |
| `--width W` | 覆盖输出宽度 | 1280 (或配置文件中的值) |
| `--height H` | 覆盖输出高度 | 720 |
| `--fps N` | 动画帧率 | 30 |
| `--duration SEC` | 动画时长 (秒) | 配置文件中的值 |
| `--no-animate` | 强制单帧模式 | (自动检测) |
| `--no-ascii` | 禁用终端 ASCII 预览 | (默认启用，需 TTY) |
| `--direction DIR` | `sksl_to_glsl` / `glsl_to_sksl` / `bidirectional` | 自动检测 (按文件后缀) |
| `--output DIR` | 自定义输出根目录 | `results/<name>/` |
| `--force` | 强制重新渲染 | (默认跳过已有产物) |

## 配置文件格式 (.params.json)

配置文件自动从输入文件同目录的 `<name>.params.json` 加载。格式：

```json
{
  "dimensions": {"width": 1280, "height": 720},
  "textures": {
    "image": "tests/assets/input.png",
    "displacementMap": {"path": "tests/assets/displacement.png", "raw": true}
  },
  "uniforms": {
    "iResolution": [1280, 720],
    "strength": 1.2,
    "progress": 0.35
  },
  "animation": {
    "enabled": true,
    "uniform": "iTime",
    "start": 0.0,
    "end": 5.0,
    "fps": 30,
    "ascii": {"enabled": true, "fps": 10}
  },
  "direction": "sksl_to_glsl"
}
```

未指定的字段使用默认值，相对纹理路径相对于 `tests/` 目录解析。

---

# live 模式：实时 GUI 预览

打开 SDL2 窗口，使用壁钟时间驱动着色器实时渲染。

- **GLSL 着色器**：调用 `render_glsl_live`（C + SDL2 + OpenGL），GPU 原生渲染，高帧率
- **SkSL 着色器**：调用 `shader_live_preview.py`（Python + SDL2 + skia-python），CPU RuntimeEffect 渲染

按 Esc 或关闭窗口退出。

## 用法

```bash
# GLSL 实时预览
bash tests/runner.sh live tests/frag/spread.frag --width 640 --height 360

# SkSL 实时预览
bash tests/runner.sh live glslang/glslang_demo/result/v7/spread.sksl --width 640 --height 360

# 带纹理的 SkSL 预览
bash tests/runner.sh live tests/shaders/water_ripple.sksl \
    --texture image=tests/assets/input.png \
    --uniform progress=0.5

# 限制帧率
bash tests/runner.sh live tests/frag/spread.frag --fps 30
```

## 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--width W` | 窗口宽度 | 1280 |
| `--height H` | 窗口高度 | 720 |
| `--fps N` | 目标帧率上限 (0=不限) | 0 |
| `--texture N=P` | 子纹理：名称=路径 (可重复) | |
| `--uniform N=V` | 静态 uniform：名称=值 (可重复，逗号分隔矢量) | |
| `--raw NAME` | 使用最近邻采样的子着色器名称 (可重复) | |
| `--title TITLE` | 窗口标题 | |

## 注意事项

- SkSL 渲染使用 CPU（skia-python RuntimeEffect），复杂度高的着色器建议降低分辨率
- GLSL 渲染使用 GPU（OpenGL），大部分着色器可满帧运行
- 需图形环境（X11/Wayland），不支持纯终端/SSH 环境

---

# 辅助工具

## render_sksl.py — 独立 SkSL 渲染 (单帧)

```bash
uv run python tests/render_sksl.py \
    --sksl tests/shaders/passthrough.sksl \
    --output output.png \
    --texture image=tests/assets/input.png \
    --uniform iResolution=1280,720 \
    --raw displacementMap
```

## render_animated.py — SkSL 多帧动画

```bash
uv run python tests/render_animated.py \
    --sksl tests/shaders/water_ripple.sksl \
    --output-dir /tmp/frames \
    --time-start 0 --time-end 3 --fps 30 \
    --time-uniform iTime \
    --uniform progress=0.5 \
    --texture image=tests/assets/input.png
```

参数：`--single-frame N` 可只渲染第 N 帧（调试用）。

## render_ascii.py — ASCII 终端查看器

监视目录中生成的 `frame_*.png`，在终端以 ASCII 艺术逐帧显示。检测 `render_done.marker` 文件确认渲染完成。

```bash
python3 tests/render_ascii.py --frame-dir /tmp/frames --fps 10 --wait
```

## compare_images.py — 像素对比

```bash
python3 tests/compare_images.py before.png after.png --json --output-json report.json
```

指标：MSE、PSNR、最大像素差、差异像素百分比。默认阈值：PSNR ≥ 30dB 且 diff% ≤ 5% 判定为 PASS。
