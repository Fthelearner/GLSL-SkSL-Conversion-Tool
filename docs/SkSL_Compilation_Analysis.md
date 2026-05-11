# SkSL 到 GLSL 编译问题分析

## 问题描述

你观察到用 `skslc` 编译 `displacement_distort.sksl` 得到的 `displacement_distort.glsl` 中，某些表达式被直接留空处理，例如：

```glsl
// 原应为：vec4 displacement = displacementMap.eval(fragCoord);
vec4 displacement = ;

// 原应为：return image.eval(iResolution * refracted_uv);
return ;
```

## 根本原因分析

### 1. **SkSL 特有类型在 GLSL 中不支持**

你的 SkSL 代码使用了以下 SkSL 特有特性：
- `shader` 类型（着色器类型）
- `.eval()` 方法调用（计算着色器的调用方式）
- 函数参数为 `float2 fragCoord`（作为隐式的内置变量）

这些特性在标准 GLSL 中没有直接对应实现。

### 2. **代码生成过程中的失败处理**

编译过程分为两个阶段：

**第一阶段：语义分析 → 中间代码（IR）**
- `.ir` 文件中代码是完整的
- 这意味着解析和早期编译阶段成功

**第二阶段：IR → GLSL 代码生成**
- `GLSLCodeGenerator` 在生成 GLSL 时无法处理某些表达式
- 对于不支持的类型或操作，代码生成器很可能：
  - 跳过生成表达式体
  - 留下空白（只有 `=` 和 `;` 之间为空）
  - 继续编译而不中断（为了收集更多错误）

### 3. **为什么没有看到错误信息**

- `.ir` 文件生成成功，所以编译没有返回失败状态
- `.glsl` 文件生成时虽然有问题，但编译器可能仍然返回成功代码
- 编译器的错误信息可能被输出到 stderr，但未被保存到文件

---

## Skia 源码中可用的诊断工具

### 1. **AST/IR 导出工具** ✅ 已有

你已经使用过的 `export_sksl_ir.sh` 可以输出三种中间格式：

```bash
# 已在使用
$SKSLC shader.rts shader.ast    # 抽象语法树
$SKSLC shader.rts shader.ir     # 中间代码表示
$SKSLC shader.rts shader.glsl   # GLSL 输出
```

**用途**：比较 `.ir` 和 `.glsl` 可以看出代码生成失败的确切位置

### 2. **错误诊断日志**

在 [skia/src/sksl/SkSLCompiler.h](skia/src/sksl/SkSLCompiler.h) 中有错误报告机制：

```cpp
ErrorReporter& errorReporter() { return *fContext->fErrors; }
```

编译器在 `compiler.errorText()` 中收集所有错误信息。

**如何访问**：修改 `skslc` 或运行时来捕获完整的 `errorText()`

### 3. **Poison 表达式标记**

错误的表达式会被标记为 `Poison` 类型（定义在 [skia/src/sksl/ir/SkSLPoison.h](skia/src/sksl/ir/SkSLPoison.h)）

- 在 IR 中应该能看到 `<POISON>` 标记
- 但在 GLSL 输出中被处理为空

### 4. **代码生成调试**

查看 [skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp](skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp)：

在 `writeExpression()` 方法（第 415 行）中，存在对所有表达式类型的处理。对于不支持的表达式，可能会：
- 命中 `default` 分支中的 `SkDEBUGFAILF()` （仅在 Debug 模式下）
- 对某些表达式类型（如 `kEmpty`）写入默认值

### 5. **SkSL 运行时效果工具** ✅ 可用

在 [skia/tools/runtime_shader_runner.cpp](skia/tools/runtime_shader_runner.cpp) 中：

```cpp
auto [effect, errorText] = SkRuntimeEffect::MakeForShader(shaderText);
if (!effect) {
    fprintf(stderr, "failed to compile runtime shader\n%s\n", errorText.c_str());
}
```

这可以捕获更详细的错误信息！

---

## 建议的解决方案

### 短期诊断

1. **运行 runtime_shader_runner 获取错误信息**：
   ```bash
   # 编译后查看详细错误
   /path/to/runtime_shader_runner --shader displacement_distort.sksl --output test.png
   ```

2. **比较 IR 和 GLSL 文件**：
   - 在 `.ir` 中看完整代码
   - 在 `.glsl` 中看留空处
   - 这能明确指出代码生成哪些部分失败

3. **检查 shader 类型是否受支持**：
   - SkSL 的 `shader` 类型是运行时效果专用
   - GLSL 不支持嵌套着色器采样
   - 需要使用其他方式（如使用 texture 采样）

### 长期修复

1. **修改 SkSL 代码以适应 GLSL 限制**：
   - 不使用 `shader.eval()`
   - 改用 texture 采样
   - 处理 fragCoord 的明确传入

2. **为特定平台选择正确的编译目标**：
   - SkSL 可编译到：GLSL、Metal、SPIR-V、WGSL 等
   - 某些特性可能只在特定目标受支持

3. **启用详细编译日志**：
   - 修改 `Main.cpp` 在生成 GLSL 前输出 `compiler.errorText()`
   - 或使用 Debug 构建捕获更多诊断信息

---

## 相关源代码位置

| 路径 | 作用 |
|------|------|
| `skia/src/sksl/SkSLCompiler.h` | 编译器接口，包含 `errorText()` |
| `skia/src/sksl/ir/SkSLPoison.h` | Poison 表达式定义 |
| `skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp` | GLSL 代码生成器实现 |
| `skia/tools/skslc/Main.cpp` | skslc 命令行工具 (第 822-843 行是错误处理) |
| `skia/tools/runtime_shader_runner.cpp` | 运行时效果编译工具 |

---

## 结论

留空的表达式通常表示：
1. **SkSL 使用了 GLSL 不支持的特性**（如 `shader` 类型）
2. **代码生成器无法处理的类型转换**
3. **编译未彻底失败，但代生成不完整**

使用 `.ir` 文件对比、运行时工具诊断，以及审查源代码中的代码生成逻辑，是找出具体原因的最佳方式。
