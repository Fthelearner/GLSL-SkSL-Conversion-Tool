# SkSL 编译流程文档

## 概述

本文档描述了如何编译 skslc 工具以及如何使用它将 SkSL 文件转换为各种目标格式。

## 目录结构

```
project/
├── src/
│   └── binding_registry/      # 双向转换的语义 binding registry
│       ├── include/           # 头文件
│       └── src/               # 实现文件
├── skia/                      # Skia 源码
│   ├── tools/skslc/           # skslc 源码
│   ├── src/sksl/              # SkSL 编译器实现
│   └── out/stog/              # 构建输出目录
│       ├── skslc              # 编译后的 skslc
│       └── *.sksl             # 预编译的 SkSL 模块
├── tests/shaders/             # SkSL 测试文件
├── tests/frag/                # GLSL 测试文件
└── tools/
    └── skslc_tool.sh          # 编译工具脚本
```

## 一、编译 skslc

### 1.1 使用脚本编译（推荐）

```bash
# 使用默认构建目录 (out/stog)
./tools/skslc_tool.sh build

# 指定构建目录
./tools/skslc_tool.sh build out/SkSL
```

### 1.2 手动编译

```bash
cd skia

# 1. 生成 GN 构建文件（如果目录不存在）
mkdir -p out/stog
echo "skia_compile_sksl_tests = true" > out/stog/args.gn
./bin/gn gen out/stog

# 2. 编译 skslc
ninja -C out/stog skslc

# 3. 编译并复制 SkSL 模块文件（必需）
ninja -C out/stog sksl_modules

# 如果模块文件在 gcc_like_host 子目录中
cp out/stog/gcc_like_host/*.sksl out/stog/
```

### 1.3 构建依赖说明

skslc 编译时会链接 `src/binding_registry` 目录（已在 BUILD.gn 中配置）：
```python
include_dirs = [
  ".",
  "../src/binding_registry/include",
]
```

修改 `src/binding_registry` 或 `tools/skslc` 下的源码后，需要重新运行 `ninja -C out/stog skslc`。

## 二、使用 skslc 编译 SkSL 文件

### 2.1 基本用法

```bash
# 格式
skslc <input_file> <output_file>

# 示例：编译为 GLSL
./skia/out/stog/skslc shader.rts output.glsl
```

### 2.2 支持的输入文件类型

| 扩展名 | 类型 | 说明 |
|--------|------|------|
| `.vert` | Vertex Shader | 顶点着色器 |
| `.frag` | Fragment Shader | 片段着色器 |
| `.sksl` | SkSL Shader | 通用 SkSL 文件（检测是否为 runtime shader） |
| `.mvert` | Mesh Vertex | Mesh 顶点着色器 |
| `.mfrag` | Mesh Fragment | Mesh 片段着色器 |
| `.compute` | Compute Shader | 计算着色器 |
| `.rtb` | Runtime Blender | Runtime Blender |
| `.rtcf` | Runtime Color Filter | Runtime Color Filter |
| `.rts` | Runtime Shader | Runtime Shader（支持 `uniform shader`、`.eval()` 等） |
| `.privrts` | Private Runtime Shader | Private Runtime Shader |
| `.fp` | Fragment Processor | 会自动转换为 `.rts` 或 `.frag` |

### 2.3 支持的输出格式

| 扩展名 | 输出类型 | 说明 |
|--------|----------|------|
| `.glsl` | GLSL 源码 | OpenGL Shading Language |
| `.hlsl` | HLSL 源码 | High-Level Shading Language (DirectX) |
| `.metal` | Metal 源码 | Apple Metal Shading Language |
| `.spirv` | SPIR-V 二进制 | Vulkan SPIR-V |
| `.asm.vert` | SPIR-V 汇编 | 顶点着色器汇编 |
| `.asm.frag` | SPIR-V 汇编 | 片段着色器汇编 |
| `.asm.comp` | SPIR-V 汇编 | 计算着色器汇编 |
| `.wgsl` | WGSL 源码 | WebGPU Shading Language |
| `.ast` | AST 输出 | 抽象语法树文本表示（调试用） |
| `.ir` | IR 输出 | 中间表示文本（调试用） |
| `.skrp` | Raster Pipeline | Raster Pipeline 输出 |
| `.stage` | Pipeline Stage | Pipeline Stage 输出 |

### 2.4 使用脚本编译

```bash
# 编译单个文件
./tools/skslc_tool.sh compile input.sksl output.glsl

# 编译并输出 AST（调试用）
./tools/skslc_tool.sh compile shader.rts debug.ast

# 批量编译目录
./tools/skslc_tool.sh batch

# 指定输入输出目录
./tools/skslc_tool.sh batch ./my_shaders ./output

# 指定输出格式
./tools/skslc_tool.sh batch ./shaders ./output --format spirv
./tools/skslc_tool.sh batch ./shaders ./output --format ast

# 查看当前配置
./tools/skslc_tool.sh info
```

## 三、SkSL 文件扩展名注意事项

### 3.1 Runtime Shader 特性

当 SkSL 文件包含以下特性时，必须使用 `.rts` 扩展名：

```sksl
// 这些特性需要 .rts 扩展名
uniform shader image;    // uniform shader 类型
half4 result = image.eval(coord);  // .eval() 调用
```

如果使用 `.sksl` 或 `.frag` 扩展名，编译器会报错：
```
error: variables of type 'shader' may not be uniform
error: type 'shader' has no method named 'eval'
```

### 3.2 脚本自动检测

`skslc_tool.sh` 会自动检测 `.sksl` 文件是否包含 runtime shader 特性：
- 包含 `uniform shader` 或 `.eval(` 的 `.sksl` 文件会自动转换为 `.rts`
- 普通的 `.sksl` 文件保持原扩展名

### 3.3 手动重命名

```bash
# 如果你的文件包含 runtime shader 特性，手动重命名为 .rts
cp my_shader.sksl my_shader.rts
./skia/out/stog/skslc my_shader.rts output.glsl
```

## 四、常用命令速查

```bash
# ===== 编译 skslc =====
./tools/skslc_tool.sh build

# ===== 编译单个文件 =====
# 到 GLSL
./tools/skslc_tool.sh compile shader.sksl output.glsl

# 到 HLSL
./tools/skslc_tool.sh compile shader.sksl output.hlsl

# 到 Metal
./tools/skslc_tool.sh compile shader.sksl output.metal

# 到 SPIR-V 汇编
./tools/skslc_tool.sh compile shader.sksl output.asm.frag

# 输出 AST（调试）
./tools/skslc_tool.sh compile shader.sksl output.ast

# 输出 IR（调试）
./tools/skslc_tool.sh compile shader.sksl output.ir

# ===== 批量编译 =====
./tools/skslc_tool.sh batch                              # 默认目录
./tools/skslc_tool.sh batch ./shaders ./output glsl      # 指定目录和格式
./tools/skslc_tool.sh batch ./shaders ./output --format spirv

# ===== 查看配置 =====
./tools/skslc_tool.sh info

# ===== 直接使用 skslc =====
./skia/out/stog/skslc input.rts output.glsl
```

## 五、问题排查

### 5.1 "Error reading sksl_shared.sksl"

**原因**：缺少预编译的 SkSL 模块文件

**解决**：
```bash
ninja -C skia/out/stog sksl_modules
cp skia/out/stog/gcc_like_host/*.sksl skia/out/stog/
```

### 5.2 "variables of type 'shader' may not be uniform"

**原因**：使用了 `.sksl` 或 `.frag` 扩展名，但文件包含 runtime shader 特性

**解决**：将文件扩展名改为 `.rts`，或使用脚本自动处理

### 5.3 找不到 skslc

**原因**：尚未编译 skslc

**解决**：
```bash
./tools/skslc_tool.sh build
```

## 六、开发工作流

### 6.1 修改 GLSL 代码生成器

1. 修改 `skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp` 或相关文件
2. 重新编译：`./tools/skslc_tool.sh build`
3. 测试：`./tools/skslc_tool.sh batch`
4. 检查输出的 `.glsl` 文件

### 6.2 修改 Binding Registry

1. 修改 `src/binding_registry/` 下的文件
2. 重新编译：`bash tools/skslc_tool.sh build`
3. 在代码中引用 binding registry 进行查询

### 6.3 添加新的测试文件

```bash
# 创建新的 SkSL 文件
cat > tests/shaders/my_test.sksl << 'EOF'
half4 main(float2 coord) {
    return half4(1.0, 0.0, 0.0, 1.0);
}
EOF

# 编译测试
./tools/skslc_tool.sh compile tests/shaders/my_test.sksl output.glsl
```
