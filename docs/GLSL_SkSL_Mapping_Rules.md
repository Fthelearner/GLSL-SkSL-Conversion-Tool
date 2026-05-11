# GLSL ↔ SkSL 映射规则

本文档详细定义 GLSL 与 SkSL (Skia Shading Language) 之间的完整映射规则，用于指导 GLSL→SkSL 和 SkSL→GLSL 双向转换器的实现。

## 1. 类型映射

### 1.1 标量类型

| GLSL | SkSL | 备注 |
|------|------|------|
| `float` | `float` | 32位浮点，直接映射 |
| `double` | `double` | SkSL 有限支持，可能需降级为 `float` |
| `int` | `int` | 32位有符号整数，直接映射 |
| `uint` | `uint` | 32位无符号整数，直接映射 |
| `bool` | `bool` | 布尔类型，直接映射 |
| `float16_t` | `half` | 16位浮点，GLSL→SkSL 映射为 `half` |
| `int8_t` | `byte` | 8位有符号整数 |
| `uint8_t` | `ubyte` | 8位无符号整数 |
| `int16_t` | `short` | 16位有符号整数 |
| `uint16_t` | `ushort` | 16位无符号整数 |
| `int64_t` | `long` | 64位有符号整数 |
| `uint64_t` | `ulong` | 64位无符号整数 |

### 1.2 向量类型

| GLSL | SkSL | 备注 |
|------|------|------|
| `vec2` / `vec3` / `vec4` | `float2` / `float3` / `float4` | **必须转换**，SkSL 不使用 `vecN` 命名 |
| `ivec2` / `ivec3` / `ivec4` | `int2` / `int3` / `int4` | **必须转换** |
| `uvec2` / `uvec3` / `uvec4` | `uint2` / `uint3` / `uint4` | **必须转换** |
| `bvec2` / `bvec3` / `bvec4` | `bool2` / `bool3` / `bool4` | **必须转换** |
| `f16vec2` / `f16vec3` / `f16vec4` | `half2` / `half3` / `half4` | **必须转换** |

### 1.3 矩阵类型

| GLSL | SkSL | 备注 |
|------|------|------|
| `mat2` | `float2x2` | 列主序，**必须转换** |
| `mat3` | `float3x3` | **必须转换** |
| `mat4` | `float4x4` | **必须转换** |
| `matNxM` | `floatNxM` | **必须转换** |
| `f16matNxM` | `halfNxM` | **必须转换** |

### 1.4 采样器/纹理/子着色器类型

**这是 GLSL→SkSL 转换中最关键的映射，直接影响语义正确性。**

| GLSL | SkSL (Runtime Effect) | SkSL (GPU Shader) | 备注 |
|------|----------------------|-------------------|------|
| `sampler2D` | `shader` | `sampler2D` | Runtime Effect 中 sampler2D 被作为子着色器使用 |
| `sampler3D` | `shader` | `sampler3D` | 同上 |
| `samplerCube` | `shader` | `samplerCube` | 同上 |
| `isampler2D` | `shader` | — | 整型采样器在 SkSL 中统一为 `shader` |
| `usampler2D` | `shader` | — | 同上 |
| `sampler2DShadow` | — | `sampler2DShadow` | 阴影采样器 |
| `sampler2DArray` | `shader` | `sampler2DArray` | 数组采样器 |
| `sampler2DMS` | — | `sampler2DMS` | 多重采样 |
| `samplerExternalOES` | `shader` | `samplerExternalOES` | 外部纹理 |
| `sampler2DRect` | `shader` | `sampler2DRect` | 矩形纹理 |
| `texture2D` | `texture2D` | `texture2D` | 纯纹理（读写） |
| `image2D` | `image2D` | `image2D` | 存储图像 |
| `subpassInput` | `subpassInput` | `subpassInput` | 子通道输入 |
| `separate sampler` | `sampler` | `sampler` | 分离采样器 |
| `sampler` (pure) | `sampler` | `sampler` | 纯采样器 |

**关键规则**：
- **GLSL→SkSL (Runtime Effect)**: 所有 `sampler2D` 类型的 uniform 必须映射为 `shader` 类型
- **GLSL→SkSL (GPU Shader)**: `sampler2D` 保持 `sampler2D`
- **SkSL `shader` 类型没有 GLSL 等价物**，需要 sideband provenance 记录才能实现双向转换

### 1.5 特殊 SkSL 类型（无 GLSL 对应物）

| SkSL 类型 | 说明 | GLSL→SkSL 处理 |
|-----------|------|----------------|
| `shader` | 运行时效果子着色器 | 从 `sampler2D` uniform 转换 |
| `colorFilter` | 颜色滤镜子效果 | 无直接 GLSL 对应 |
| `blender` | 混合模式子效果 | 无直接 GLSL 对应 |
| `$genType` | 泛型占位符 | 仅内部使用 |

---

## 2. 限定符映射

### 2.1 存储限定符

| GLSL | SkSL | GLSL→SkSL 处理 |
|------|------|----------------|
| `uniform` | `uniform` | **保留**（用于非 sampler 类型） |
| `uniform` + `shader` 类型 | `uniform` | **保留**，类型改为 `shader` |
| `const` | `const` | 直接保留 |
| `in` (全局) | `in` | 保留（vertex input） |
| `out` (全局) | `out` | 保留（vertex output） |
| `out` (fragment) | — | **移除**，fragColor 通过函数返回值传递 |
| `buffer` | — | **移除**或转换为 storage buffer 声明 |
| `shared` | — | **移除**，SkSL 使用 `workgroup` |

### 2.2 布局限定符 — GLSL→SkSL: 全部移除

**SkSL 不支持任何 `layout(...)` 语法。GLSL→SkSL 转换时必须全部移除：**

| GLSL layout | 处理方式 |
|-------------|---------|
| `layout(location = N)` | **移除** |
| `layout(binding = N)` | **移除** |
| `layout(set = N)` | **移除** |
| `layout(std140)` / `layout(std430)` | **移除**，展开 block 成员为独立 uniform |
| `layout(rgba32f)` / 格式限定符 | **移除** |
| `layout(constant_id = N)` | **移除** |
| `layout(early_fragment_tests)` | **移除** |
| `layout(push_constant)` | **移除** |

### 2.3 精度限定符 — GLSL→SkSL: 移除

| GLSL | 处理方式 |
|------|---------|
| `highp` / `mediump` / `lowp` | **移除**。SkSL 使用类型区分精度：`float`=高精, `half`=中精 |
| `precision mediump float;` | **移除**。全局精度声明在 SkSL 中不存在 |

### 2.4 插值限定符 — GLSL→SkSL: 移除

| GLSL | 处理方式 |
|------|---------|
| `flat` | **移除** |
| `smooth` | **移除** |
| `noperspective` | **移除** |
| `centroid` | **移除** |
| `sample` | **移除** |

### 2.5 内存限定符 — GLSL→SkSL: 移除

| GLSL | 处理方式 |
|------|---------|
| `readonly` / `writeonly` | **移除** |
| `coherent` / `volatile` / `restrict` | **移除** |
| `invariant` / `precise` | **移除** |

### 2.6 参数限定符

| GLSL | SkSL | GLSL→SkSL 处理 |
|------|------|----------------|
| `in` | (默认) | **移除**，SkSL 参数默认为 in |
| `out` | `out` | 保留 |
| `inout` | `inout` | 保留 |

---

## 3. 内置变量映射

### 3.1 片段着色器

| GLSL | SkSL | 备注 |
|------|------|------|
| `gl_FragCoord` | `sk_FragCoord` | 坐标原点可能不同 |
| `gl_FrontFacing` | `sk_Clockwise` | **注意**：语义反转（Face orientation vs winding） |
| `gl_FragColor` | `sk_FragColor` | 输出颜色 |
| `gl_PointCoord` | — | SkSL 无直接对应 |

### 3.2 顶点着色器

| GLSL | SkSL | 备注 |
|------|------|------|
| `gl_Position` | `sk_Position` | 裁剪空间位置 |
| `gl_PointSize` | `sk_PointSize` | 点精灵大小 |
| `gl_VertexID` / `gl_VertexIndex` | `sk_VertexID` | 顶点索引 |
| `gl_InstanceID` / `gl_InstanceIndex` | `sk_InstanceID` | 实例索引 |

### 3.3 计算着色器

| GLSL | SkSL | 备注 |
|------|------|------|
| `gl_GlobalInvocationID` | `sk_GlobalInvocationID` | 全局调用 ID |
| `gl_LocalInvocationID` | `sk_LocalInvocationID` | 本地调用 ID |
| `gl_LocalInvocationIndex` | `sk_LocalInvocationIndex` | 本地调用索引 |
| `gl_NumWorkGroups` | `sk_NumWorkgroups` | 工作组数量 |
| `gl_WorkGroupID` | `sk_WorkgroupID` | 工作组 ID |

---

## 4. 语法结构映射

### 4.1 前处理指令 — GLSL→SkSL: 全部移除

| GLSL | 处理方式 |
|------|---------|
| `#version 450 core` | **移除**。SkSL 没有版本指令 |
| `#extension ...` | **移除**。SkSL 不支持扩展指令 |
| `#pragma ...` | **移除** |

### 4.2 入口函数签名

| GLSL | SkSL | 备注 |
|------|------|------|
| `void main()` (fragment) | `half4 main(float2 fragCoord)` | **必须转换**：返回颜色值而非写入全局变量 |
| `void mainImage(out vec4 fragColor, in vec2 fragCoord)` (ShaderToy) | `half4 main(float2 fragCoord)` | ShaderToy 片段着色器入口 |
| `void main()` (vertex) | `float4 main(...)` | 根据具体需求转换 |
| `void main()` (compute) | `void main()` | 计算着色器保持 |

### 4.3 uniform block 展开 — GLSL→SkSL: 必须展开

GLSL interface block：
```glsl
layout(std140, binding = 2) uniform Params {
    vec2 iResolution;
    float strength;
} u;
```

SkSL（展开为独立 uniform，**去掉 block 实例名前缀**）：
```glsl
uniform float2 iResolution;
uniform float strength;
```

**关键规则**：
- 展开所有 struct/interface block 成员为独立的顶层 uniform 声明
- **删除** block 实例名前缀（`u.iResolution` → `iResolution`）
- 所有对 `u.member` 的引用转换为直接引用 `member`

### 4.4 片段输出处理

GLSL 片段着色器使用输出变量写入颜色：
```glsl
layout(location = 0) out vec4 outColor;
void main() {
    outColor = texture(...);
}
```

SkSL 通过函数返回值传递：
```glsl
half4 main(float2 fragCoord) {
    return image.eval(fragCoord);
}
```

**转换规则**：
1. 移除 `out vec4 outColor` 声明（包括其 layout 限定符）
2. 在入口函数中声明 `float4 outColor;` 局部变量
3. 所有 `outColor = expr;` 赋值保持不变（写入局部变量）
4. `outColor = expr; return;` 模式 → `return half4(expr);`（早期返回优化）
5. 函数末尾添加 `return half4(outColor);`

> **已知限制**：SkSL `.eval()` 和 GLSL `texture()` 使用不同的坐标空间。
> - GLSL `texture(sampler2D, uv)` 使用规范化 UV 坐标 [0,1]
> - SkSL `shader.eval(coord)` 使用像素坐标（与 fragCoord 相同坐标系）
>
> 当前自动转换直接传递坐标参数不做变换。对于 GLSL 中使用了 `coord / resolution` 规范化坐标的 helper 函数，转换后的 `.eval()` 调用会传入未乘回分辨率的坐标。这是一个需要手动修正或后续改进的语义问题。

### 4.5 子着色器采样调用

GLSL 的 texture 采样：
```glsl
uniform sampler2D image;
void main() {
    vec4 color = texture(image, uv);
}
```

SkSL 使用 `.eval()` 方法：
```glsl
uniform shader image;
half4 main(float2 fragCoord) {
    half4 color = image.eval(fragCoord);
}
```

**转换规则**：
- `texture(shaderVar, coord)` → `shaderVar.eval(coord)`（**函数调用改写**）
- 仅当第一个参数是 sampler2D 类型的 uniform 时才转换
- 其他 texture 函数（`textureLod`, `textureGrad` 等）在 SkSL 中使用不同方法

### 4.6 纹理采样函数完整映射

| GLSL | SkSL (Runtime Effect) | 备注 |
|------|----------------------|------|
| `texture(s, coord)` | `s.eval(coord)` | shader child 采样 |
| `texture(s, coord, bias)` | `s.eval(coord)` | bias 在 SkSL 中通常忽略或独立实现 |
| `textureLod(s, coord, lod)` | `s.eval(coord)` | 或使用 sampleLod |
| `textureGrad(s, coord, dpdx, dpdy)` | `s.eval(coord)` | 或使用 sampleGrad |
| `textureProj(s, coord)` | `s.eval(coord.xy / coord.w)` | 透视除法需显式 |
| `texelFetch(s, coord, lod)` | 不支持 | 移除或用 textureRead |
| `textureSize(s)` | 不支持 | SkSL runtime effect 不支持查询纹理大小 |

---

## 5. 函数和表达式映射

### 5.1 内置数学函数

以下函数在 GLSL 和 SkSL 中**名称和签名完全相同**，直接保留：

`radians`, `degrees`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `pow`, `exp`, `log`, `exp2`, `log2`, `sqrt`, `inversesqrt`, `abs`, `sign`, `floor`, `trunc`, `round`, `ceil`, `fract`, `mod`, `modf`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `faceforward`, `reflect`, `refract`, `outerProduct`, `determinant`, `inverse`, `transpose`, `dFdx`, `dFdy`, `fwidth`

### 5.2 函数签名差异

| 方面 | GLSL | SkSL | 处理 |
|------|------|------|------|
| 参数默认值 | 无 | 无 | 一致 |
| `in` 限定符 | 可显式标注 | 隐式（不可标注） | **移除 `in`** |
| 函数重载 | 支持 | 部分支持 | 需检查 |
| 参数名 mangling | glslang 内部使用 | 无 | 清理括号后缀 |

### 5.3 向量分量访问

| GLSL | SkSL | GLSL→SkSL |
|------|------|-----------|
| `v.xyz` / `v.rgb` | **相同** | 直接保留 |
| `v[0]` / `v[1]` | **相同** | 直接保留 |
| `v.a` (alpha) | **相同** | 直接保留 |

**注意**：glslang AST 可能将 `.a` 编码为 `EOpIndexDirect` 而非 `EOpVectorSwizzle`。转换器需要将向量上的 `EOpIndexDirect` 还原为 `.rgba` 或 `.xyzw` 格式。

### 5.4 结构体成员访问

| GLSL | SkSL | 处理 |
|------|------|------|
| `struct.member` | **相同** | 直接保留 |
| `blockInstance.member` (uniform block) | `member` | **展开并去除前缀** |

---

## 6. GLSL→SkSL 移除/抑制清单（完整总结）

以下是 GLSL→SkSL 转换时需要**完全移除或抑制**的语法元素：

### 6.1 顶层指令
- `#version ...` — SkSL 无版本概念
- `#extension ...` — SkSL 无扩展机制
- `#pragma ...` — SkSL 不支持

### 6.2 布局限定符
- `layout(location = N)` — SkSL 通过顺序或语义绑定
- `layout(binding = N)` — SkSL 通过编译器自动分配
- `layout(set = N)` — Vulkan 专属
- `layout(std140)` / `layout(std430)` — block 已展开
- `layout(rgba32f)` / 图像格式 — 不适用
- `layout(push_constant)` / `constant_id` — 不适用

### 6.3 存储和限定符
- `out` 变量（fragment 着色器输出）— 通过函数返回值
- `in` 参数限定符 — 参数默认为 in
- `highp` / `mediump` / `lowp` — 使用 `half` 类型代替
- `flat` / `smooth` / `noperspective` — 不适用
- `centroid` / `sample` — 不适用
- `readonly` / `writeonly` / `coherent` / `volatile` / `restrict` — 不适用
- `invariant` / `precise` — 不适用
- `buffer` 存储限定符 — 不适用
- `shared` 存储限定符 — 不适用

### 6.4 语法结构
- 接口块（`uniform BlockName { ... } instance;`）— 展开为独立 uniform
- `sampler2D` 类型名 → `shader` 类型名
- `vecN` 类型名 → `floatN` 类型名
- `ivecN` 类型名 → `intN` 类型名

---

## 7. 双向转换可逆性

### 7.1 精确可逆（Exact）
- 数学内置函数名称和签名
- `const` 常量声明
- 控制流语句（`if`, `for`, `while`, `do-while`, `switch`, `return`, `break`, `continue`）
- 表达式运算符
- 结构体成员访问

### 7.2 规范化可逆（Normalized）
- 类型名：`vec4` ↔ `float4`
- 矩阵名：`mat3` ↔ `float3x3`
- 采样器类型：`sampler2D` ↔ `shader`（标记 provenance）

### 7.3 通过 Provenance 可逆（ViaProvenance）
- `gl_FragCoord` ↔ `sk_FragCoord`（坐标原点处理）
- `gl_FrontFacing` ↔ `sk_Clockwise`（面朝向语义）
- `shader child` ↔ `sampler2D` + provenance
- `.eval()` ↔ `texture()` + provenance
- `sk_Caps.*` ↔ 编译时常量 + provenance

### 7.4 有损转换（Lossy）
- `layout()` 限定符 — 包含后不可恢复
- 精度限定符 — 信息丢失
- 插值/内存限定符 — 信息丢失
- uniform block 结构 — 展开后无法还原
- `#version` / `#extension` — 无法恢复
- 函数参数 `in` 限定符 — 不可恢复

---

## 8. 转换流程建议

### GLSL → SkSL 前向转换步骤

1. **去除前处理指令**：移除 `#version`, `#extension`
2. **展开 uniform blocks**：将 `layout(std140) uniform Block { ... } instance` 展开为独立的 `uniform` 声明，去掉实例名前缀
3. **转换类型名**：`vecN`→`floatN`，`ivecN`→`intN`，`sampler2D`→`shader`
4. **去除所有 layout() 限定符**
5. **去除精度/插值/内存限定符**
6. **移除 `in` 参数限定符**
7. **转换 fragment 入口**：`out vec4 X; void main()` → `half4 main(float2 fragCoord)`
8. **转换采样调用**：`texture(s, uv)` → `s.eval(uv)`
9. **转换内置变量**：`gl_FragCoord` → `fragCoord`, `gl_FrontFacing` → `sk_Clockwise`
10. **记录 provenance** 用于反向转换

### SkSL → GLSL 反向转换步骤

1. **注入版本和扩展指令**
2. **恢复类型名**：`floatN`→`vecN`，`shader`→`sampler2D`
3. **恢复 fragment 入口**：`half4 main(float2 fragCoord)` → `void main()` + `out vec4`
4. **恢复采样调用**：`s.eval(uv)` → `texture(s, uv)`（需 provenance）
5. **恢复内置变量**：`sk_FragCoord` → `gl_FragCoord`
6. **添加必要的 layout() 限定符**
7. **使用 provenance 恢复有损信息**

---

## 9. 参考锚点

### glslang 侧源码
- AST 节点定义: `glslang/glslang/Include/intermediate.h`
- 类型定义: `glslang/glslang/Include/Types.h`
- 基础类型枚举: `glslang/glslang/Include/BaseTypes.h`
- GLSL→SkSL 代码生成器: `glslang/glslang/GenericCodeGen/GLSLSkSLCodeGenerator.cpp`

### SkSL 侧源码
- 类型系统: `skia/src/sksl/ir/SkSLType.h`
- 内置类型注册: `skia/src/sksl/SkSLBuiltinTypes.cpp`
- GLSL 代码生成器: `skia/src/sksl/codegen/SkSLGLSLCodeGenerator.cpp`

### 绑定注册表
- 内置变量绑定: `src/binding_registry/src/builtin_bindings.cpp`
- 内置函数绑定: `src/binding_registry/src/intrinsic_bindings.cpp`
- 语法特性绑定: `src/binding_registry/src/feature_bindings.cpp`
- Provenance 记录: `src/binding_registry/src/provenance.cpp`

---

## 10. 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-06 | 初始版本：收集并整理 GLSL↔SkSL 完整映射规则 |
