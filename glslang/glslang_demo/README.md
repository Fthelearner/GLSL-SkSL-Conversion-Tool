# glslang_demo

This directory contains GLSL fragment-shader translations of the five SkSL demo shaders from
`skip_demo/shaders/`.

Translation rules used here:

- The shaders are written as standard GLSL 450 fragment shaders.
- `uniform shader ...` SkSL children are translated to `sampler2D` uniforms plus helper sampling
  functions.
- Numeric uniforms are grouped into a `layout(std140, binding = ...)` uniform block so the files
  compile cleanly with Vulkan SPIR-V generation via `glslangValidator -V`.
- SkSL `main(float2)` entry points are lowered to GLSL `void main()` and use `gl_FragCoord.xy`
  as the fragment-space coordinate.
- These shaders are intended for parsing/compilation in `glslang`. They are not exact runtime
  replacements for Skia's runtime-shader execution model.

Generate SPIR-V and glslang AST dumps:

```bash
cd glslang
bash glslang_demo/run_glslang_demo.sh
```

Artifacts are written to:

- `glslang_demo/generated/spv/*.spv`
- `glslang_demo/generated/ast/*.ast`
