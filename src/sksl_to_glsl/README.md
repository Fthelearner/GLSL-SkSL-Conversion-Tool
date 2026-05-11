# SkSL → GLSL 自定义导出器

这个目录包含 SkSL→GLSL 方向的自定义导出工具链。

## 文件

- `sksl_export_glsl_custom.cpp` — 自定义 GLSL 导出器源码。组合 `FramebufferFetchSupport` 和 `DualSourceBlending` 两类 capability，解决标准 `skslc` 单次只能保留一套 `ShaderCaps` 的限制。
- `build.sh` — 构建脚本。编译并链接自定义导出器到 `skia/out/SkSL/sksl_export_glsl_custom`。
- `export_artifacts.sh` — 导出入口脚本。先用 `skslc` 生成 AST，再调用自定义导出器生成 GLSL。当输入来自 `tests/shaders/*.sksl` 时自动复制为临时 `.rts` 以确保按 runtime shader 的 `ProgramKind` 编译。

## 当前边界

这个自定义导出器没有完整复刻 `tools/skslc/Main.cpp` 的 `#pragma settings` 解析流程。它是为需要同时开启 framebuffer fetch 和 dual-source blending 的测试着色器准备的。

## 用法

```bash
# 单个文件：导出 AST + GLSL 到指定目录
bash src/sksl_to_glsl/export_artifacts.sh tests/shaders/displacement_distort.sksl /tmp/out

# 指定输出基名
bash src/sksl_to_glsl/export_artifacts.sh tests/shaders/passthrough.sksl /tmp/out my_passthrough

# 批量导出
bash src/sksl_to_glsl/export_artifacts.sh tests/shaders/*.sksl results/sksl_toglsl

# 显式参数
bash src/sksl_to_glsl/export_artifacts.sh --output-dir results/sksl_toglsl tests/shaders/*.sksl
```

## 构建

```bash
bash src/sksl_to_glsl/build.sh
```

产物: `skia/out/SkSL/sksl_export_glsl_custom`

构建脚本从 `skia/out/SkSL/obj/skslc.ninja` 提取编译参数，复用 skslc 的所有目标文件（除 `skslc.Main.o`），链接自定义 `main()` 入口。
