# Binding Registry

这个目录定义的是一套面向双向转换的语义 binding registry，目标不是做
`AST kind -> AST kind` 的节点级映射，而是为下面两条链路共享同一套语义规则：

- `SkSL AST -> GLSL`
- `GLSL AST -> SkSL`

## 设计目标

- 用统一的语义 ID 表示 builtin / intrinsic / feature
- 前向转换和反向转换共享同一套规则表
- 区分“名字映射”、“调用映射”和“语法特性映射”
- 显式标记 round-trip 可逆性
- 给需要 provenance 的降级保留统一记录结构

## 目录结构

- `include/binding_registry/binding_types.h`
  共享基础类型：stage、dialect、surface form、round-trip、provenance。
- `include/binding_registry/builtin_bindings.h`
  内置变量和 capability 字段的共享规则表。
- `include/binding_registry/intrinsic_bindings.h`
  内置函数 / intrinsic 的共享规则表。
- `include/binding_registry/feature_bindings.h`
  语法和语义特性的共享规则表。
- `src/*.cpp`
  对应表项和查询实现。

## 为什么三张表要分开

它们不是同一个维度：

- builtin
  处理 `sk_FragCoord`、`sk_Clockwise`、`gl_Position`、`sk_Caps.*` 这类符号语义。
- intrinsic
  处理 `sin`、`clamp`、`mix`、`floatBitsToInt` 这类调用语义。
- feature
  处理 `uniform shader`、`.eval(...)`、`#version`、`#extension`、`layout(...)`、
  `in/out/inout`、interface block、memory qualifier、RTFlip fragcoord workaround、
  scalar-to-vector canonicalization 这类 surface syntax 和不可直译语义。

如果把这些揉成一张表，字段会迅速稀疏化，正反两条链路也会越来越难维护。

## 双向共享方式

这套 registry 是共享的，但两条链路的用法不同。

### SkSL AST -> GLSL

前向链路按“语义 ID + 上下文”查询：

1. 识别当前 AST 节点对应的 builtin / intrinsic / feature 语义
2. 查询 binding 规则
3. 根据 lowering kind 选择：
   - 直接发射 GLSL 名字
   - 发射 helper / alias
   - 做 canonicalization / rewrite
   - 写入 provenance

### GLSL AST -> SkSL

反向链路按“surface form + 上下文”查询：

1. 从 GLSL AST 中识别 surface form
2. 用 `glsl_forms` 在表里匹配候选规则
3. 根据 round-trip 策略判断：
   - 直接恢复 SkSL builtin / intrinsic
   - 仅恢复规范化语义
   - 必须结合 provenance
   - 当前不支持无损恢复

## 反向链路需要写入表的 GLSL surface syntax

除了直接的 builtin 名字，至少需要把这些 GLSL 侧写法记入 registry：

- 直接 builtin 标识符
  - `gl_FragCoord`
  - `gl_FrontFacing`
  - `gl_Position`
  - `gl_PointSize`
  - `gl_VertexID`
  - `gl_VertexIndex`
  - `gl_InstanceID`
  - `gl_InstanceIndex`
  - `gl_GlobalInvocationID`
  - `gl_LocalInvocationID`
  - `gl_LocalInvocationIndex`
  - `gl_NumWorkGroups`
  - `gl_WorkGroupID`
- builtin setup / workaround 形式
  - `vec4(gl_FragCoord.x, rtFlip.x + rtFlip.y * gl_FragCoord.y, ...)`
  - `bool sk_Clockwise = gl_FrontFacing;` 及其条件翻转形式
- scalar-to-vector canonicalization
  - `vec2(0.001)`、`vec3(1.0)`、`vec4(0.5)` 这类为 mixed overload 引入的 splat
- resource / layout syntax
  - `layout(location = ...)`
  - `layout(binding = ...)`
  - `layout(set = ...)`
  - `layout(std140)` / `layout(std430)`
  - `layout(constant_id = ...)`
  - `readonly` / `writeonly` / `coherent`
- declaration / qualifier syntax
  - `in` / `out` / `uniform` / `buffer`
  - function parameter `in` / `out` / `inout`
  - `flat` / `smooth` / `noperspective` / `centroid` / `sample`
  - `volatile` / `restrict`
  - `invariant` / `precise`
- interface syntax
  - `uniform Block { ... } name;`
  - `buffer Block { ... } name;`
  - `in/out Block { ... } name;`
- preprocessor / profile syntax
  - `#version`
  - `#extension`
  - `precision highp/mediump/lowp`
- runtime-effect lowering surface forms
  - `uniform sampler2D ...`
  - `texture(...)` when it originates from `.eval(...)`

另外，GLSL 中一些“必须显式落表但不一定能回到 SkSL”的特性，也建议在
feature registry 里保留条目并标成 `Lossy` 或 `Unsupported`，例如：

- SSBO / image load-store 资源模型
- separate texture + sampler model
- geometry / tessellation 专属 stage builtin 和 layout
- subgroup / advanced memory model 扩展

这些 pattern 不是说反向解析时只能靠字符串匹配，而是说 registry 里必须显式记录：
“这种 GLSL 表面写法对应哪一类 SkSL 语义”。

## Round-trip 约定

- `Exact`
  前向和反向都可以无损恢复。
- `Normalized`
  可以恢复语义，但不保证恢复原始源码写法。
- `ViaProvenance`
  反向必须依赖前向生成的 provenance 记录。
- `Lossy`
  允许前向降级，反向只能 best-effort。
- `Unsupported`
  当前不承诺这条路径。

## 当前条目范围

当前骨架先覆盖第一批高价值条目：

- builtin
  - `sk_FragCoord`
  - `sk_Clockwise`
  - `sk_Position`
  - `sk_PointSize`
  - `sk_VertexID`
  - `sk_InstanceID`
  - compute stage builtins
  - `sk_Caps.floatIs32Bits`
  - `sk_Caps.integerSupport`
  - `sk_Caps.builtinDeterminantSupport`
- intrinsic
  - `sin`, `cos`
  - `clamp`, `min`, `max`, `mod`
  - `step`, `smoothstep`, `mix`
  - `floatBitsToInt`, `intBitsToFloat`
  - `reflect`, `refract`
- feature
  - `uniform shader`
  - `.eval(...)`
  - `#version`
  - `#extension`
  - `layout(...)`
  - storage / parameter / interpolation / memory qualifiers
  - interface blocks
  - `invariant` / `precise`
  - RTFlip fragcoord alias
  - clockwise alias
  - scalar-to-vector canonicalization

这不是最终全集，但已经足够支撑双向设计继续推进。
