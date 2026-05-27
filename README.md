# SkSL ↔ GLSL Bidirectional Transpiler

基于 Skia 和 glslang 编译器的 SkSL ↔ GLSL 双向语义转换工具链，支持带 provenience 配置文件的 round-trip 语义还原。

## 工程结构

```
project/
├── src/
│   ├── binding_registry/          # 双向转换共享语义规则表 (C++ 库)
│   ├── sksl_to_glsl/             # SkSL→GLSL 自定义导出器
│   └── renderer/                 # Python 着色器渲染框架 (skia-python)
├── tests/
│   ├── shaders/                  # SkSL 着色器测试文件 (5 个)
│   ├── frag/                     # GLSL 着色器测试文件 (9 个)
│   ├── assets/                   # 测试纹理图像
│   ├── fixtures/                 # 预期输出 (回归测试用)
│   ├── runner.sh                 # 双向测试编排器 (fulltest / pipeline / test)
│   ├── params.sh                 # 着色器参数配置
│   ├── render_sksl.py            # SkSL→PNG 渲染 (skia-python RuntimeEffect)
│   ├── render_animated.py        # 多帧动画渲染
│   ├── render_ascii.py           # ASCII 终端帧查看器
│   ├── compare_images.py         # 像素级图像对比 + 差异灰度图
│   └── preprocess_glsl.py        # GLSL 预处理 (默认值注入)
├── tools/
│   ├── skslc_tool.sh             # skslc 编译器封装
│   └── shader_preview.py         # 实时着色器预览 (ffplay)
├── docs/                         # 分析与映射规则文档
├── results/                      # 测试产物统一输出目录
├── skia/                         # Skia 源码 (含 GLSLCodeGenerator 修改)
├── glslang/                      # glslang 源码 (含 GLSLSkSLCodeGenerator 修改)
└── zjuthesis/                    # 毕业论文
```

## 两条转换链路

### 方向 1: SkSL → GLSL (前向)
```
SkSL 源码 → Skia Compiler → SkSL AST → GLSLCodeGenerator → GLSL 输出 + .provenance
```
- 代码生成器: `skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp`
- 自定义导出器: `src/sksl_to_glsl/sksl_export_glsl_custom.cpp`
- 编译产物: `skia/out/SkSL/sksl_export_glsl_custom`

### 方向 2: GLSL → SkSL (反向)
```
GLSL 源码 → glslang Compiler → glslang AST → GLSLSkSLCodeGenerator → SkSL 输出 + .provenance
```
- 代码生成器: `glslang/glslang/GenericCodeGen/GLSLSkSLCodeGenerator.cpp`
- 转换器入口: `glslang/glslang_demo/glslang_to_sksl.cpp`
- 编译产物: `glslang/build/glslang_demo/glslang-to-sksl`

`binding_registry/` 为两条链路提供统一的语义规则表（builtin / intrinsic / feature），正反向转换共享同一套规则。

## 渲染规则

| 规则 | 说明 |
|------|------|
| SkSL 渲染 | **CPU RuntimeEffect 优先**；遇到 `fwidth`/`dFdx`/`dFdy` 等不支持的函数时自动 fallback 到 GPU |
| GLSL 渲染 | **固定 GPU** — 始终通过 `render_glsl` (OpenGL 3.3) 渲染 |
| 时间统一 | 所有含 `iTime` 的着色器统一锁定 `iTime=1.5`，单帧对比 |
| Alpha 语义 | SkSL `.eval()` 返回预乘 alpha，GLSL `texture()` 返回直通 alpha；`fix_premultiplied_alpha` 在 GLSL 渲染前自动 inline helper 函数并注入 `.rgb *= .a` 修正 |

## 环境要求

- **OS**: Linux (X11 + epoxy + GLX)
- **编译器**: GCC 或 Clang (C++20)
- **构建工具**: GN + Ninja (skia), CMake + Make (glslang)
- **Python**: ≥3.11, 包管理用 [uv](https://docs.astral.sh/uv/)
- **运行时依赖**: ImageMagick (`convert`), ffmpeg (动画导出), ffplay (实时预览)

## 快速开始

### 1. 安装 Python 依赖

```bash
uv sync
```

### 2. 编译所有 C++ 工具

```bash
# skslc + sksl_modules (SkSL→GLSL)
cd skia
mkdir -p out/stog && echo "skia_compile_sksl_tests = true" > out/stog/args.gn
bin/gn gen out/stog && ninja -C out/stog skslc sksl_modules

# 自定义 GLSL 导出器
bash src/sksl_to_glsl/build.sh

# render_glsl (OpenGL 离屏渲染器)
cd ../glslang/glslang_demo
gcc -O2 -o render_glsl render_glsl.c $(pkg-config --cflags --libs epoxy gl) -lX11 -lm

# glslang + glslang-to-sksl (GLSL→SkSL)
cd ../build
cmake .. && make glslang glslang-to-sksl
```

> 如果构建失败，检查 `skia/BUILD.gn` 和 `glslang/CMakeLists.txt` 中的 `binding_registry` 路径是否正确指向 `src/binding_registry/`。

## 测试命令

### fulltest — 双向闭环验证（推荐）

对一个着色器文件执行完整的 v1 → v2 → 闭环 三阶段验证，输出到指定目录：

```bash
# SkSL→GLSL 方向完整测试
bash tests/runner.sh fulltest tests/shaders/water_ripple.sksl --output results/sksltoglsl/test2

# GLSL→SkSL 方向完整测试
bash tests/runner.sh fulltest tests/frag/purple_cloud.frag --output results/glsltosksl/test2

# 只跑 v1（原始→转换→对比）
bash tests/runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2 --stage v1

# 只跑 v2（v1/code→逆向转换→对比）
bash tests/runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2 --stage v2

# 只跑闭环对比（v1/before vs v2/after）
bash tests/runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2 --stage report

# CPU 模式：强制 SkSL 走 CPU RuntimeEffect 渲染（默认已走 GPU）
bash tests/runner.sh fulltest tests/frag/purple_cloud.frag --output results/glsltosksl/test2 --cpu
```

**输出目录结构**（`<output>/<shader_name>/`）：

```
<shader>/
├── v1/
│   ├── before/       # 原始着色器 + 渲染 PNG
│   ├── code/         # 转换结果 + .provenance
│   ├── after/        # 转换后渲染 PNG
│   └── report/       # 对比 JSON + 差异灰度图
├── v2/
│   ├── before/       # v1/code 着色器 + 渲染 PNG
│   ├── code/         # 逆向转换结果 + .provenance
│   ├── after/        # 逆向渲染 PNG
│   └── report/       # 对比 JSON + 差异灰度图
└── report/           # 闭环: v1/before vs v2/after JSON + 差异灰度图
```

### test — 批量双向回归测试

对 `ALL_SHADERS` 中的所有着色器执行双向 round-trip：

```bash
# 所有着色器
bash tests/runner.sh test --all

# 单个着色器
bash tests/runner.sh test --shader water_ripple

# 只跑方向 1 (SkSL→GLSL→SkSL)
bash tests/runner.sh test --dir1 --shader displacement_distort

# 只跑方向 2 (GLSL→SkSL→GLSL)
bash tests/runner.sh test --dir2 --shader passthrough
```

### pipeline — 单方向灵活转换

```bash
# SkSL → GLSL
bash tests/runner.sh pipeline tests/shaders/water_ripple.sksl --direction sksl_to_glsl

# GLSL → SkSL
bash tests/runner.sh pipeline tests/frag/spread.frag --direction glsl_to_sksl

# 指定输出目录
bash tests/runner.sh pipeline my_shader.sksl --output results/custom_dir

# 强制单帧（禁用动画检测）
bash tests/runner.sh pipeline tests/frag/curve.frag --direction glsl_to_sksl --no-animate
```

### compare — 图像对比

```bash
# 基本对比
python3 tests/compare_images.py before.png after.png

# 生成 JSON 报告
python3 tests/compare_images.py before.png after.png --output-json report.json

# 生成差异灰度图（自动拉伸对比度）
python3 tests/compare_images.py before.png after.png --diffmap diff.png

# 保留原始差异值（不做对比度拉伸）
python3 tests/compare_images.py before.png after.png --diffmap diff.png --diffmap-raw

# 通过 runner.sh
bash tests/runner.sh compare before.png after.png --diffmap diff.png
```

### 其他常用命令

```bash
# 渲染 SkSL → PNG (CPU RuntimeEffect)
uv run python tests/render_sksl.py --sksl tests/shaders/passthrough.sksl --output out.png

# 渲染 SkSL 并指定纹理/参数
uv run python tests/render_sksl.py --sksl tests/shaders/water_ripple.sksl \
    --output out.png --texture image=tests/assets/input.png \
    --uniform progress=0.35 --uniform waveCount=2.0

# 编译 SkSL → GLSL
bash tools/skslc_tool.sh compile tests/shaders/passthrough.sksl out.glsl

# 实时着色器预览
python3 tools/shader_preview.py --sksl tests/shaders/water_ripple.sksl | ffplay ...
```

## 开发指南

### 修改 binding_registry (语义规则)

1. 编辑 `src/binding_registry/` 下对应头文件/源文件
2. 重新编译 skslc: `cd skia && ninja -C out/stog skslc`
3. 重新编译 glslang-to-sksl: `cd glslang/build && make glslang-to-sksl`

### 修改 GLSL 代码生成器 (SkSL→GLSL)

1. 编辑 `skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp`
2. 重新编译: `cd skia && ninja -C out/stog skslc`
3. 重新编译自定义导出器: `bash src/sksl_to_glsl/build.sh`

### 修改 GLSL→SkSL 代码生成器

1. 编辑 `glslang/glslang/GenericCodeGen/GLSLSkSLCodeGenerator.cpp`
2. 重新编译: `cd glslang/build && make glslang-to-sksl`

### 修改 GLSL 渲染器

1. 编辑 `glslang/glslang_demo/render_glsl.c`
2. 重新编译: `cd glslang/glslang_demo && gcc -O2 -o render_glsl render_glsl.c $(pkg-config --cflags --libs epoxy gl) -lX11 -lm`

### 添加新测试着色器

**SkSL 着色器：**
1. 创建 `.sksl` 文件放入 `tests/shaders/`
2. 创建 `<name>.params.json` 配置文件（纹理、uniforms、尺寸）
3. 在 `tests/params.sh` 中添加 `<name>_textures`、`<name>_uniforms` 等函数
4. 将着色器名称加入 `ALL_SHADERS` 数组

**GLSL 着色器：**
1. 创建 `.frag` 文件放入 `tests/frag/`
2. 如需纹理/uniform，创建 `<name>.params.json` 配置文件
3. 若无需配置（Shadertoy 风格），runner 会自动生成默认配置

## 测试结果

### 闭环验证（v1/before vs v2/after）— 全部通过

| 着色器 | sksltoglsl 闭环 | glsltosksl 闭环 |
|--------|:---:|:---:|
| passthrough | PSNR=∞, 0% | PSNR=∞, 0% |
| water_ripple | PSNR=∞, 0% | PSNR=∞, 0% |
| displacement_distort | PSNR=∞, 0% | PSNR=∞, 0% |
| linear_gradient_blend | PSNR=∞, 0% | PSNR=∞, 0% |
| variable_radius_blur_approx | PSNR=∞, 0% | PSNR=∞, 0% |
| curve | — | PSNR=∞, 0% |
| picture_blur | — | PSNR=∞, 0% |
| purple_cloud | — | PSNR=∞, 0% |
| spread | — | PSNR=∞, 0% |

### v1 跨语言对比（单次转换，CPU vs GPU 后端）

| 着色器 | sksltoglsl | glsltosksl | 差异类型 |
|--------|:---:|:---:|------|
| passthrough | 0% | 0% | — |
| water_ripple | 1.07% | 1.07% | 数值梯度放大 |
| displacement_distort | 0.3% | 0.3% | 精度容差 |
| linear_gradient_blend | 3.33% | 3.33% | 精度容差 |
| variable_radius_blur_approx | 6.54% | 6.54% | 精度容差 |
| picture_blur | — | 14.0% (maxΔ=9) | 精度容差 |
| purple_cloud | — | 0% | GPU 默认模式 |
| curve | — | 0% | 自动 GPU fallback |
| spread | — | 9.52% | 程序化噪声 |

> 所有跨语言差异均源于 CPU RuntimeEffect 与 GPU OpenGL 的浮点精度 / 数学库实现差异，转换链路本身语义正确（闭环为证）。SkSL 渲染默认使用 GPU 路径消除后端差异，如需 CPU 渲染可使用 `--cpu` 标志。
