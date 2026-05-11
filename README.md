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
│   ├── shaders/                  # SkSL 着色器测试文件
│   ├── frag/                     # GLSL 着色器测试文件
│   ├── assets/                   # 测试纹理图像
│   ├── fixtures/                 # 预期输出 (回归测试用)
│   ├── runner.sh                 # 双向往返测试编排器
│   ├── params.sh                 # 着色器参数配置
│   ├── render_sksl.py            # SkSL→PNG 渲染
│   ├── render_animated.py        # 多帧动画渲染
│   ├── render_ascii.py           # ASCII 终端帧查看器
│   └── compare_images.py         # 像素级图像对比
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

# glslang-to-sksl (GLSL→SkSL)
cd ../glslang/build
cmake .. && make glslang-to-sksl
```

> 如果构建失败，检查 `skia/BUILD.gn` 和 `glslang/CMakeLists.txt` 中的 `binding_registry` 路径是否正确指向 `src/binding_registry/`。

### 3. 运行测试

```bash
# 完整双向 round-trip 测试 (所有着色器)
bash tests/runner.sh --all

# 单个着色器测试
bash tests/runner.sh --shader passthrough

# 只测试方向1
bash tests/runner.sh --dir1 --shader water_ripple
```

### 4. 常用命令

```bash
# 渲染 SkSL → PNG
uv run python src/renderer/main.py --effect passthrough --output out.png

# 编译 SkSL → GLSL
bash tools/skslc_tool.sh compile tests/shaders/passthrough.sksl out.glsl

# 批量编译测试目录
bash tools/skslc_tool.sh batch

# 导出 SkSL 的 AST + GLSL
bash src/sksl_to_glsl/export_artifacts.sh tests/shaders/passthrough.sksl /tmp/out

# 图像像素对比
python3 tests/compare_images.py before.png after.png --json

# 实时着色器预览
python3 tools/shader_preview.py --sksl tests/shaders/water_ripple.sksl | ffplay ...
```

## 双向测试流程

`tests/runner.sh` 自动化完整的 round-trip 验证：

### 方向1: SkSL → GLSL → SkSL
| 步骤 | 操作 |
|------|------|
| D1.S1 | 渲染原始 SkSL (skia-python RuntimeEffect) |
| D1.S2 | SkSL → GLSL (sksl_export_glsl_custom) |
| D1.S3 | 渲染中间 GLSL (render_glsl) |
| D1.S4 | GLSL → SkSL (glslang-to-sksl) |
| D1.S5 | 渲染还原 SkSL |
| 对比 | 原始 SKSL 渲染 vs 还原 SKSL 渲染 |

### 方向2: GLSL → SkSL → GLSL
| 步骤 | 操作 |
|------|------|
| D2.S1 | 渲染原始 GLSL |
| D2.S2 | GLSL → SkSL |
| D2.S3 | 渲染中间 SkSL |
| D2.S4 | SkSL → GLSL |
| D2.S5 | 渲染还原 GLSL |
| 对比 | 原始 GLSL 渲染 vs 还原 GLSL 渲染 |

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

### 添加新测试着色器

1. 创建 `.sksl` 文件放入 `tests/shaders/`
2. 在 `tests/params.sh` 中添加 `<name>_textures`、`<name>_uniforms` 等函数
3. 将着色器名称加入 `ALL_SHADERS` 数组
4. 运行 `bash tests/runner.sh --shader <name>` 验证

## 测试结果

当前 5 个着色器 10 条转换链路全部通过 (PSNR=∞, diff=0%):

| 着色器 | Dir1 | Dir2 |
|--------|------|------|
| passthrough | PASS | PASS |
| water_ripple | PASS | PASS |
| displacement_distort | PASS | PASS |
| linear_gradient_blend | PASS | PASS |
| variable_radius_blur_approx | PASS | PASS |
