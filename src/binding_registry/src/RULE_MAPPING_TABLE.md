# Binding Registry — 规则映射表

本文件由 binding_registry 的三个绑定数组 (`builtin_bindings.cpp`, `intrinsic_bindings.cpp`, `feature_bindings.cpp`) 自动提取生成，共 **43 条规则**，覆盖 3 个语义域。

---

## 1. 内置变量绑定 (BuiltinBindings) — 14 条

### 1.1 片段着色器输入 (Fragment Stage Input)

| # | ID | SkSL 拼写 | GLSL 拼写 | Lowering | RoundTrip | 适用阶段 | Helper Key |
|---|----|-----------|-----------|----------|-----------|----------|------------|
| 1 | `kFragCoord` | `sk_FragCoord` | `gl_FragCoord` | `ComputedAlias` | `ViaProvenance` | Fragment | `fragcoord_setup` |
| 2 | `kClockwise` | `sk_Clockwise` | `gl_FrontFacing` | `ComputedAlias` | `ViaProvenance` | Fragment | `clockwise_setup` |

**GLSL 表面形式 (FragCoord):**
| Kind | Pattern | 说明 |
|------|---------|------|
| `Identifier` | `gl_FragCoord` | 直接 GLSL 内置变量 |
| `BuiltinSetup` | `vec4(gl_FragCoord.x, rtFlip.x + rtFlip.y * gl_FragCoord.y, ...)` | 应用 RTFlip 后的解析形式 |

**GLSL 表面形式 (Clockwise):**
| Kind | Pattern | 说明 |
|------|---------|------|
| `Identifier` | `gl_FrontFacing` | 直接 GLSL 内置变量 |
| `InjectedAlias` | `bool sk_Clockwise = gl_FrontFacing;` | 后端生成的别名（含可选条件反转） |

### 1.2 顶点着色器输出 (Vertex Stage Output)

| # | ID | SkSL 拼写 | GLSL 拼写 | Lowering | RoundTrip | 适用阶段 |
|---|----|-----------|-----------|----------|-----------|----------|
| 3 | `kPosition` | `sk_Position` | `gl_Position` | `DirectName` | `Exact` | Vertex |
| 4 | `kPointSize` | `sk_PointSize` | `gl_PointSize` | `DirectName` | `Normalized` | Vertex |

> **注**: `kPointSize` 标记为 `Normalized` 而非 `Exact`，因为需要目标管线特定的校验。

### 1.3 顶点着色器输入 (Vertex Stage Input)

| # | ID | SkSL 拼写 | GLSL 主拼写 | GLSL 别名 | Lowering | RoundTrip | 适用阶段 |
|---|----|-----------|-------------|-----------|----------|-----------|----------|
| 5 | `kVertexID` | `sk_VertexID` | `gl_VertexID` | `gl_VertexIndex` (Vulkan) | `DirectName` | `Normalized` | Vertex |
| 6 | `kInstanceID` | `sk_InstanceID` | `gl_InstanceID` | `gl_InstanceIndex` (Vulkan) | `DirectName` | `Normalized` | Vertex |

> **注**: 这两个 ID 在 OpenGL/GLES 和 Vulkan 方言下拼写不同，反向映射需将两种拼写归一化到同一个语义 ID。

### 1.4 计算着色器输入 (Compute Stage Input)

| # | ID | SkSL 拼写 | GLSL 拼写 | Lowering | RoundTrip | 适用阶段 |
|---|----|-----------|-----------|----------|-----------|----------|
| 7 | `kGlobalInvocationID` | `sk_GlobalInvocationID` | `gl_GlobalInvocationID` | `DirectName` | `Exact` | Compute |
| 8 | `kLocalInvocationID` | `sk_LocalInvocationID` | `gl_LocalInvocationID` | `DirectName` | `Exact` | Compute |
| 9 | `kLocalInvocationIndex` | `sk_LocalInvocationIndex` | `gl_LocalInvocationIndex` | `DirectName` | `Exact` | Compute |
| 10 | `kNumWorkgroups` | `sk_NumWorkgroups` | `gl_NumWorkGroups` | `DirectName` | `Exact` | Compute |
| 11 | `kWorkgroupID` | `sk_WorkgroupID` | `gl_WorkGroupID` | `DirectName` | `Exact` | Compute |

> **注**: 注意大小写差异 — `sk_NumWorkgroups` vs `gl_NumWorkGroups`，`sk_WorkgroupID` vs `gl_WorkGroupID`。

### 1.5 Capabilities 字段 (CapsField)

| # | ID | SkSL 拼写 | GLSL 拼写 | Lowering | RoundTrip | Helper Key |
|---|----|-----------|-----------|----------|-----------|------------|
| 12 | `kCapsFloatIs32Bits` | `sk_Caps.floatIs32Bits` | `__sksl_caps_floatIs32Bits` | `CompileTimeConstant` | `ViaProvenance` | `caps_constant` |
| 13 | `kCapsIntegerSupport` | `sk_Caps.integerSupport` | `__sksl_caps_integerSupport` | `CompileTimeConstant` | `ViaProvenance` | `caps_constant` |
| 14 | `kCapsBuiltinDeterminantSupport` | `sk_Caps.builtinDeterminantSupport` | `__sksl_caps_builtinDeterminantSupport` | `CompileTimeConstant` | `ViaProvenance` | `caps_constant` |

> **注**: Caps 字段在 SkSL 侧是 `MemberAccess` 形式 (`sk_Caps.xxx`)，在 GLSL 侧作为 `InjectedAlias` 常量注入，精确恢复需要 provenance。

### 1.6 按类别汇总

| 类别 | 数量 | 条目 |
|------|------|------|
| `StageInput` | 10 | FragCoord, Clockwise, VertexID, InstanceID, GlobalInvocationID, LocalInvocationID, LocalInvocationIndex, NumWorkgroups, WorkgroupID |
| `StageOutput` | 2 | Position, PointSize |
| `CapsField` | 3 | CapsFloatIs32Bits, CapsIntegerSupport, CapsBuiltinDeterminantSupport |
| `PipelinePseudoVar` | 0 | _(已定义枚举值但无条目)_ |

---

## 2. 内置函数绑定 (IntrinsicBindings) — 13 条

### 2.1 同族数值函数 (HomogeneousNumeric)

| # | ID | SkSL | GLSL | 签名族 | Lowering | RoundTrip |
|---|----|------|------|--------|----------|-----------|
| 1 | `kSin` | `sin` | `sin` | `genType -> genType` | `DirectBuiltinCall` | `Exact` |
| 2 | `kCos` | `cos` | `cos` | `genType -> genType` | `DirectBuiltinCall` | `Exact` |

> **注**: 同族函数不需要标量到向量的规范化。

### 2.2 混合标量/向量函数 (MixedScalarVector)

| # | ID | SkSL | GLSL | 签名族 | Lowering | RoundTrip | Helper Key |
|---|----|------|------|--------|----------|-----------|------------|
| 3 | `kClamp` | `clamp` | `clamp` | `genType x genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 4 | `kMin` | `min` | `min` | `genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 5 | `kMax` | `max` | `max` | `genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 6 | `kMod` | `mod` | `mod` | `genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 7 | `kStep` | `step` | `step` | `genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 8 | `kSmoothstep` | `smoothstep` | `smoothstep` | `genType x genType x genType` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |
| 9 | `kMix` | `mix` | `mix` | `genType x genType x genType\|bvec` | `CanonicalizedBuiltinCall` | `Normalized` | `scalar_vector_splat` |

> **注**: 当目标消费者拒绝标量/向量混合重载时，前向发射会将标量参数规范化为 `vecN(scalar)`。每个函数有 2 种 GLSL 形式：直接函数调用 + 规范化构造函数形式。

### 2.3 位转换函数 (BitCast)

| # | ID | SkSL | GLSL | 签名族 | Lowering | RoundTrip |
|---|----|------|------|--------|----------|-----------|
| 10 | `kFloatBitsToInt` | `floatBitsToInt` | `floatBitsToInt` | `float genType -> int genType` | `DirectBuiltinCall` | `Exact` |
| 11 | `kIntBitsToFloat` | `intBitsToFloat` | `intBitsToFloat` | `int genType -> float genType` | `DirectBuiltinCall` | `Exact` |

> **注**: 位转换不应基于返回类型规范化参数。

### 2.4 几何函数 (Geometric)

| # | ID | SkSL | GLSL | 签名族 | Lowering | RoundTrip |
|---|----|------|------|--------|----------|-----------|
| 12 | `kReflect` | `reflect` | `reflect` | `genType x genType -> genType` | `DirectBuiltinCall` | `Exact` |
| 13 | `kRefract` | `refract` | `refract` | `genType x genType x scalar -> genType` | `DirectBuiltinCall` | `Exact` |

> **注**: `refract` 的第三个参数 `eta` 必须保持标量，不能被折叠为 `vecN(eta)`。

### 2.5 按族汇总

| 族 | 数量 | 条目 |
|----|------|------|
| `HomogeneousNumeric` | 2 | sin, cos |
| `MixedScalarVector` | 7 | clamp, min, max, mod, step, smoothstep, mix |
| `BitCast` | 2 | floatBitsToInt, intBitsToFloat |
| `Geometric` | 2 | reflect, refract |
| `BooleanSelector` | 0 | _(已定义枚举值但无条目)_ |

---

## 3. 特性绑定 (FeatureBindings) — 16 条

### 3.1 预处理器特性 (Preprocessor)

| # | ID | 类别 | 前向处理 | 反向处理 | RoundTrip | 适用方言 |
|---|----|------|----------|----------|-----------|----------|
| 1 | `kVersionDirective` | `Preprocessor` | `Direct` | `Direct` | `Exact` | 全部 |
| 2 | `kExtensionDirective` | `Preprocessor` | `Direct` | `Direct` | `Normalized` | 全部 |

> **注**: 版本指令是唯一标记为 Exact 的特性。扩展指令为 Normalized。

### 3.2 限定词语法特性 (QualifierSyntax)

| # | ID | 类别 | 前向 | 反向 | RoundTrip | 适用阶段 | 适用方言 |
|---|----|------|------|------|-----------|----------|----------|
| 3 | `kLayoutQualifier` | `QualifierSyntax` | `Direct` | `Direct` | `Normalized` | 全部 | 全部 |
| 4 | `kPrecisionQualifier` | `QualifierSyntax` | `Direct` | `Direct` | `Normalized` | 全部 | **仅 GLES** |
| 5 | `kStorageQualifier` | `QualifierSyntax` | `Direct` | `Direct` | `Normalized` | 全部 | 全部 |
| 6 | `kParameterQualifier` | `QualifierSyntax` | `Direct` | `Direct` | `Normalized` | 全部 | 全部 |
| 7 | `kInterpolationQualifier` | `QualifierSyntax` | `Direct` | `Direct` | `Normalized` | **Vertex+Fragment** | 全部 |
| 8 | `kMemoryQualifier` | `QualifierSyntax` | `Direct` | `Direct` | **`Lossy`** | 全部 | 全部 |
| 9 | `kInvariantPreciseQualifier` | `QualifierSyntax` | `Direct` | `Direct` | **`Lossy`** | 全部 | 全部 |

**GLSL 表面形式详情:**

| 特性 | GLSL 表面形式数 | 覆盖的 Pattern |
|------|----------------|---------------|
| `kLayoutQualifier` | 2 | `layout(location = N)`, `layout(binding = N, set = M, std140\|std430, constant_id = K)` |
| `kPrecisionQualifier` | 1 | `precision mediump float;` |
| `kStorageQualifier` | 4 | `in`, `out`, `uniform`, `buffer` |
| `kParameterQualifier` | 3 | `in`, `out`, `inout` |
| `kInterpolationQualifier` | 5 | `smooth`, `flat`, `noperspective`, `centroid`, `sample` |
| `kMemoryQualifier` | 5 | `readonly`, `writeonly`, `coherent`, `volatile`, `restrict` |
| `kInvariantPreciseQualifier` | 2 | `invariant`, `precise` |

### 3.3 接口块 (InterfaceSyntax)

| # | ID | 类别 | 前向 | 反向 | RoundTrip | GLSL 表面形式数 |
|---|----|------|------|------|-----------|----------------|
| 10 | `kInterfaceBlock` | `InterfaceSyntax` | `Direct` | **`Rewrite`** | **`Lossy`** | 3 (`uniform BlockName {...}`, `buffer BlockName {...}`, `in BlockName {...}`) |

> **注**: 接口块在反向转换时需要特殊处理（展开、扁平化或拒绝），标记为 Lossy。

### 3.4 运行时效果特性 (RuntimeEffect)

| # | ID | 类别 | 前向 | 反向 | RoundTrip | 适用阶段 | Helper Key |
|---|----|------|------|------|-----------|----------|------------|
| 11 | `kUniformShader` | `RuntimeEffect` | `Rewrite` | **`SidebandOnly`** | `ViaProvenance` | **Fragment only** | `runtime_effect_child` |
| 12 | `kChildEval` | `RuntimeEffect` | `Rewrite` | **`SidebandOnly`** | `ViaProvenance` | **Fragment only** | `runtime_effect_child_eval` |

**GLSL 表面形式:**
| 特性 | SkSL 形式 | GLSL 形式 |
|------|-----------|-----------|
| `kUniformShader` | `uniform shader child;` (ResourceDeclaration) | `uniform sampler2D child;` / `uniform sampler2D child; + auxiliary metadata` (ResourceDeclaration) |
| `kChildEval` | `child.eval(coords)` (FunctionCall) | `texture(child, uv)` (FunctionCall) / `skslEvalChild(child, coords)` (HelperCall) |

> **注**: 这两个是反向转换的关键。单独的 `texture(...)` 不足以证明来自 `child.eval(...)`，必须通过 provenance 确认。

### 3.5 内置设置特性 (BuiltinSetup)

| # | ID | 类别 | 前向 | 反向 | RoundTrip | 适用阶段 | Helper Key |
|---|----|------|------|------|-----------|----------|------------|
| 13 | `kFragCoordResolvedAlias` | `BuiltinSetup` | `RewriteWithHelper` | `Rewrite` | `ViaProvenance` | Fragment | `fragcoord_setup` |
| 14 | `kClockwiseAlias` | `BuiltinSetup` | `RewriteWithHelper` | `Rewrite` | `ViaProvenance` | Fragment | `clockwise_setup` |

> **注**: 这两个与内置变量 `kFragCoord` 和 `kClockwise` 共享相同的 helper_key，形成交叉引用。

### 3.6 规范化特性 (Canonicalization)

| # | ID | 类别 | 前向 | 反向 | RoundTrip | Helper Key |
|---|----|------|------|------|-----------|------------|
| 15 | `kScalarToVectorCanonicalization` | `Canonicalization` | `Rewrite` | `Rewrite` | `Normalized` | `scalar_vector_splat` |

**GLSL 表面形式:** `vecN(scalar)` (ConstructorCall)

> **注**: 由 7 个 MixedScalarVector 内置函数共享 `scalar_vector_splat` helper key。

### 3.7 资源模型特性 (ResourceModel)

| # | ID | 类别 | 前向 | 反向 | RoundTrip |
|---|----|------|------|------|-----------|
| 16 | `kSamplerImageResourceModel` | `ResourceModel` | `Direct` | `Direct` | `Normalized` |

**GLSL 表面形式:** `uniform sampler2D tex;` (ResourceDeclaration), `layout(rgba32f) uniform coherent image2D image;` (ResourceDeclaration)

### 3.8 未使用的 FeatureCategory

| 类别 | 状态 |
|------|------|
| `DeclarationSyntax` | _(已定义枚举值但无条目)_ |

---

## 4. Helper Key 交叉引用

Helper key 用于关联不同语义域的条目，确保前向发射和反向恢复的一致性。

| Helper Key | 关联的 Builtin | 关联的 Intrinsic | 关联的 Feature | 说明 |
|------------|---------------|------------------|---------------|------|
| `fragcoord_setup` | `kFragCoord` | — | `kFragCoordResolvedAlias` | 片段坐标 + RTFlip 处理 |
| `clockwise_setup` | `kClockwise` | — | `kClockwiseAlias` | 顺时针/面朝向别名处理 |
| `caps_constant` | `kCapsFloatIs32Bits`, `kCapsIntegerSupport`, `kCapsBuiltinDeterminantSupport` | — | — | Caps 编译时常量 |
| `runtime_effect_child` | — | — | `kUniformShader` | shader child 类型映射 |
| `runtime_effect_child_eval` | — | — | `kChildEval` | child.eval() → texture() 映射 |
| `scalar_vector_splat` | — | `kClamp`, `kMin`, `kMax`, `kMod`, `kStep`, `kSmoothstep`, `kMix` | `kScalarToVectorCanonicalization` | 标量到向量规范化 |

---

## 5. RoundTrip 策略分布

| RoundTripKind | Builtin | Intrinsic | Feature | 合计 | 占比 |
|---------------|---------|-----------|---------|------|------|
| `Exact` | 7 | 6 | 1 | **14** | 32.6% |
| `Normalized` | 2 | 7 | 8 | **17** | 39.5% |
| `ViaProvenance` | 5 | 0 | 4 | **9** | 20.9% |
| `Lossy` | 0 | 0 | 3 | **3** | 7.0% |
| `Unsupported` | 0 | 0 | 0 | **0** | 0% |
| **合计** | **14** | **13** | **16** | **43** | 100% |

---

## 6. 表面形式覆盖矩阵 (按 SurfaceFormKind)

| SurfaceFormKind | 出现于 Builtin | 出现于 Intrinsic | 出现于 Feature | 合计 |
|-----------------|---------------|-----------------|---------------|------|
| `Identifier` | 14 (SkSL+GLSL) | — | — | 14 |
| `MemberAccess` | 3 (SkSL Caps) | — | — | 3 |
| `FunctionCall` | — | 13 (SkSL+GLSL) | 2 (GLSL) | 15 |
| `ConstructorCall` | — | 7 (GLSL canonicalized) | 1 (GLSL) | 8 |
| `LayoutQualifier` | — | — | 2 | 2 |
| `StorageQualifier` | — | — | 4 | 4 |
| `ParameterQualifier` | — | — | 3 | 3 |
| `InterpolationQualifier` | — | — | 5 | 5 |
| `MemoryQualifier` | — | — | 5 | 5 |
| `AuxiliaryQualifier` | — | — | 2 | 2 |
| `InterfaceBlock` | — | — | 3 | 3 |
| `Directive` | — | — | 2 | 2 |
| `ResourceDeclaration` | — | — | 4 | 4 |
| `InjectedAlias` | 4 (GLSL Caps + Clockwise) | — | 1 (GLSL) | 5 |
| `HelperCall` | — | — | 1 (GLSL) | 1 |
| `BuiltinSetup` | 1 (GLSL FragCoord) | — | 1 (GLSL) | 2 |
| `UnsupportedPattern` | 0 | 0 | 0 | 0 |

---

## 7. 潜在缺口分析

基于上述映射表的完整性审查，以下方面可能需要补充规则：

### 7.1 内置变量缺口

| 缺口 | 说明 |
|------|------|
| `gl_FragDepth` / `gl_FragStencilRef` | 片段着色器深度/模板输出，SkSL 中无直接对应 |
| `gl_PointCoord` | 点精灵坐标，SkSL 中无直接对应 |
| `gl_PrimitiveID` / `gl_Layer` / `gl_ViewportIndex` | 几何/曲面细分阶段变量，SkSL 有限支持 |
| `gl_HelperInvocation` / `gl_DeviceIndex` | 较新的 GLSL 内置变量 |
| `sk_FragColor` | SkSL 旧版内置变量，在文档中提到但未注册 |

### 7.2 内置函数缺口

| 缺口 | 说明 |
|------|------|
| 三角函数族 | `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `atan2` — 均未注册 |
| 指数/幂函数 | `pow`, `exp`, `log`, `exp2`, `log2`, `sqrt`, `inversesqrt` — 均未注册 |
| 通用数值函数 | `abs`, `sign`, `floor`, `trunc`, `round`, `ceil`, `fract`, `modf` — 均未注册 |
| 向量/矩阵函数 | `length`, `distance`, `dot`, `cross`, `normalize`, `faceforward`, `outerProduct`, `determinant`, `inverse`, `transpose` — 均未注册 |
| 导数函数 | `dFdx`, `dFdy`, `fwidth` — 均未注册 |
| 纹理采样函数 | `textureLod`, `textureGrad`, `textureProj`, `texelFetch`, `textureSize` — 均未注册 |
| 原子操作 | `atomicAdd`, `atomicMin`, `atomicMax` 等 — 均未注册 |
| `kBooleanSelector` 族函数 | `lessThan`, `greaterThan`, `equal`, `notEqual` — 族已定义但无条目 |

### 7.3 特性缺口

| 缺口 | 说明 |
|------|------|
| `DeclarationSyntax` 类别 | 已定义但无任何条目 — 可能需要添加声明语法规则（如 `const` 声明、初始化列表等） |
| 结构体声明 | 结构体在 SKSL/GLSL 间的映射未作为特性注册 |
| 函数声明/重载 | 函数签名差异（参数名 mangling 等）未注册 |
| 入口函数签名转换 | `void main()` ↔ `half4 main(float2)` 转换未作为特性注册 |
| 类型名转换 | `vecN` ↔ `floatN`, `matN` ↔ `floatNxM` 等类型名转换未作为绑定注册（可能在 CodeGenerator 中硬编码） |
| 数组语法差异 | GLSL 和 SkSL 数组声明语法差异未注册 |
| `precision` 默认值 | 全局精度声明的默认值处理未注册 |
| `subpassInput` | 子通道输入类型未注册 |
| `samplerExternalOES` | 外部纹理类型未注册 |

### 7.4 交叉引用完整性

| 检查项 | 状态 |
|--------|------|
| 所有 helper_key 在至少 2 个条目中出现 | `caps_constant` 仅在 Builtin 侧出现（3 个条目），Feature 侧无对应 |
| `runtime_effect_child` / `runtime_effect_child_eval` 仅在 Feature 侧出现，Builtin/Intrinsic 侧无对应 | 合理（运行时效果没有对应的内置变量/函数） |
| `clockwise_setup` 跨 Builtin+Feature | 正常 |
| `fragcoord_setup` 跨 Builtin+Feature | 正常 |
| `scalar_vector_splat` 跨 Intrinsic+Feature | 正常 |

---

## 8. 枚举定义参考

### 8.1 BuiltinLoweringKind
| 值 | 含义 |
|----|------|
| `kDirectName` | 直接名称映射 |
| `kComputedAlias` | 计算别名（需要 setup 代码） |
| `kCompileTimeConstant` | 编译时常量 |
| `kInjectedHelper` | 注入辅助函数 |
| `kSidebandOnly` | 仅通过 sideband 携带 |
| `kUnsupported` | 不支持 |

### 8.2 IntrinsicLoweringKind
| 值 | 含义 |
|----|------|
| `kDirectBuiltinCall` | 直接内置函数调用 |
| `kCanonicalizedBuiltinCall` | 规范化内置函数调用（标量→向量 splat） |
| `kOperatorBuiltin` | 运算符内置函数 |
| `kHelperRewrite` | 辅助函数重写 |
| `kUnsupported` | 不支持 |

### 8.3 FeatureHandling
| 值 | 含义 |
|----|------|
| `kDirect` | 直接穿透 |
| `kRewrite` | AST 重写 |
| `kRewriteWithHelper` | 带辅助函数的 AST 重写 |
| `kSidebandOnly` | 仅通过 provenance sideband 传递 |
| `kReject` | 明确拒绝 |

### 8.4 RoundTripKind
| 值 | 含义 |
|----|------|
| `kExact` | 精确可逆，无需额外信息 |
| `kNormalized` | 规范化后可逆（如类型名变换） |
| `kViaProvenance` | 必须通过 provenance 文件才能完全恢复 |
| `kLossy` | 有损转换，部分信息不可恢复 |
| `kUnsupported` | 不支持双向转换 |

---

*生成时间: 2026-05-15 | 数据来源: `builtin_bindings.cpp`, `intrinsic_bindings.cpp`, `feature_bindings.cpp`*
