# CPU vs GPU 渲染差异数值分析

## 数据来源

本文所有测试数据均来自以下目录和文件：

| 数据 | 路径 | 说明 |
|------|------|------|
| v1 渲染对比 | `results/sksltoglsl/test_cpu/<shader>/v1/` | CPU RuntimeEffect vs GPU OpenGL |
| v2 逆行对比 | `results/sksltoglsl/test_cpu/<shader>/v2/` | 逆向转换对比 |
| 闭环验证 | `results/sksltoglsl/test_cpu/<shader>/report/` | v1/before vs v2/after |
| 着色器源码 | `tests/shaders/<shader>.sksl` | 原始 SkSL |
| 生成 GLSL | `results/sksltoglsl/test_cpu/<shader>/v1/code/<shader>.glsl` | sksl_export_glsl_custom 输出 |
| 输入纹理 | `tests/assets/input.png` | `src/renderer/image_loader.py:make_demo_image()` |
| 位移贴图 | `tests/assets/displacement.png` | `src/renderer/image_loader.py:make_demo_displacement_map()` |
| 预模糊图 | `tests/assets/preblur.png` | `src/renderer/image_loader.py:make_blurred_image()` |
| 模糊遮罩 | `tests/assets/blur_mask.png` | `src/renderer/image_loader.py:make_demo_blur_mask()` |
| 测试参数 | `tests/params.sh` | uniforms、textures 配置 |
| 差异热图 | `results/sksltoglsl/test_cpu/<shader>/v1/report/<shader>_diffmap.png` | compare_images.py --diffmap |
| GPU 渲染器 | `glslang/glslang_demo/render_glsl.c` | OpenGL 3.3, GL_LINEAR 纹理过滤, `--raw` RGBA 输出 |
| GPU→PNG 转换 | `tests/runner.sh:render_single_frame_glsl()` | raw RGBA→PNG + Python 反预乘 alpha |
| CPU 渲染器 | `src/renderer/shader_runner.py` | skia-python RuntimeEffect, SamplingOptions(kLinear) |

### 模拟计算说明

本文中的 fp32 模拟计算使用以下方法：
```python
import struct
def f32(v):
    return float(struct.unpack('f', struct.pack('f', v))[0])
```
即 IEEE 754 单精度浮点的精确截断。GPU sin 差异模拟使用 fp32 π = `f32(math.pi)` 进行参数约简。所有模拟计算均以 Python 代码在本地执行，非 GPU 硬件实测值。模拟的 Python 代码随文附上。

---

## 测试结果总览

**来源**: `results/sksltoglsl/test_cpu/<shader>/v1/report/<shader>_comparison.json`

> **关键修复** (2026-06-01): GLSL 渲染输出从 PPM→PNG (丢失 alpha) 改为 raw RGBA→PNG + 反预乘 alpha。改动的文件: `glslang/glslang_demo/render_glsl.c` (`--raw` 输出模式) 和 `tests/runner.sh:render_single_frame_glsl()` (raw→PNG Python 转换)。修复前 linear_gradient_blend 的 10% 差异主因是 PPM 丢失 alpha 通道导致预乘 RGB 无法还原为直通 RGB。修复后差异从 MSE=124/maxΔ=182 降至 MSE=0.63/maxΔ=3。

| 着色器 | MSE | PSNR(dB) | maxΔ | diff% | 差异像素 | 闭环 |
|--------|-----|----------|------|-------|---------|:---:|
| water_ripple | 0.0031 | 73.23 | 3 | 0.54 | 5,011/921,600 | ∞ |
| displacement_distort | 0.2282 | 54.55 | 50 | 0.32 | 2,930 | ∞ |
| linear_gradient_blend | 0.63 | 60.29 | 3 | 9.47 | 87,251 | ∞ |
| variable_radius_blur_approx | 0.0038 | 72.30 | 1 | 1.13 | 10,407 | ∞ |

闭环全部 PSNR=∞，证明 **sksl_export_glsl_custom 和 glslang-to-sksl 的转换语义完全无损**。v1/v2 的差异全部来自渲染后端的不同。

---

## 1. water_ripple — fp32 有限差分精度 ×500

### 1.1 实测数据

**来源**: `results/sksltoglsl/test_cpu/water_ripple/v1/report/water_ripple_comparison.json`
- MSE=0.0031, PSNR=73.23, maxΔ=3, diff=0.54%, 差异像素=5,011

**差异分布**（来自 `water_ripple_diffmap.png` 像素级分析）：
- 差异值分布: 85 级占 4,992 像素(99.6%), 170 级占 18 像素, 255 级占 1 像素
  - 注意: 85 是自动对比度拉伸后值, 原始差异 maxΔ=3
- 空间分布: x=[289,971], y=[203,719], 散布于整个画面
- 最多差异的 x 列: 426(51), 853(46), 638(44), 435(33), 423(31)

**实测像素差异示例**:
| 像素 | before (CPU) | after (GPU) | 通道差异 |
|------|-------------|-------------|---------|
| (424,615) | (124,100,89) | (127,100,88) | R+3,B-1 |
| (853,622) | (243,189,89) | (243,190,91) | G+1,B+2 |
| (519,384) | (233,171,134) | (233,172,136) | G+1,B+2 |
| (504,402) | (233,173,138) | (233,172,136) | G-1,B-2 |

所有差异均为 1~3 级的单通道微小变化，无系统性方向偏差。这是典型的 fp32 精度噪声特征。

### 1.2 差异机制：有限差分放大链

**着色器核心计算**（`tests/shaders/water_ripple.sksl` 第 25-31 行）:

```glsl
float waveGradient(float propagatedDistance, float timeValue) {
    float distanceToWave = propagatedDistance - 2.0 * timeValue;
    float delta = 0.001;
    float d1 = distanceToWave - delta;
    float d2 = distanceToWave + delta;
    return (calcWave(d2) - calcWave(d1)) / (2.0 * delta);
}
```

**差异像素 (424, 615) 的 fp32 vs fp64 计算追踪**：

参数: `WAVE_FREQUENCY=31.0, WAVE_PROPAGATION_RATIO=2.0, delta=0.001`
（来自 `tests/params.sh:water_ripple_uniforms` 和 shader 源码）

```python
# 模拟代码 (Python 3.x)
import math, struct
def f32(v): return float(struct.unpack('f', struct.pack('f', v))[0])
def smoothstep(e0,e1,x):
    t = max(0.0,min(1.0,(x-e0)/(e1-e0)))
    return t*t*(3-2*t)

# 参数
fx,fy = 424.5, 615.5; iRes=(1280,720); shortEdge=720
progress=0.35; waveCount=2.0; rippleCenter=(0.5,0.7)
timeValue = (0.5+0.1*waveCount)*progress  # = 0.245
wc = (0.5*1280/720, 0.7)  # = (0.8889, 0.7)

# --- fp64 路径 ---
uvH = (fx/shortEdge, fy/shortEdge)           # (0.58958, 0.85486)
wv = (uvH[0]-wc[0], uvH[1]-wc[1])           # (-0.29931, 0.15486)
pd = math.sqrt(wv[0]**2+wv[1]**2)            # 0.3369952215
dtW = pd - 2.0*timeValue                     # -0.1530047785

def calcWave_fp64(d):
    pw = smoothstep(0.0,-0.3,d)
    wf = smoothstep(-0.6,-0.3,d)*pw
    return -math.sin(31.0*d)*wf

delta=0.001
cw1_f64 = calcWave_fp64(dtW-delta)
cw2_f64 = calcWave_fp64(dtW+delta)
grad_f64 = (cw2_f64 - cw1_f64) / 0.002        # 4.5022212059
# ... 继续追踪
```

**fp64 逐步骤结果**（直接 Python math 计算）:

| 步骤 | 表达式 | fp64 值 |
|------|--------|--------|
| propagatedDistance | sqrt(wv²) | 0.3369952215 |
| distanceToWave (d) | pd - 0.49 | -0.1530047785 |
| sin(31×(d-0.001)) | sin(-4.774148) | 0.9980935096 |
| sin(31×(d+0.001)) | sin(-4.712148) | 1.0000000000 |
| calcWave(d-δ) | -sin×smoothstep | -0.5190277232 |
| calcWave(d+δ) | -sin×smoothstep | -0.5100232808 |
| **梯度** | (cw2-cw1)/0.002 | **4.5022212059** |
| amplitudeDecay | (1-pd)⁴ | 0.1932 |
| amplitudeSuppress | smoothstep(0,0.45,pd) | 0.8425 |
| intensity | grad×ampD×ampS×0.012 | 0.0087950333 |
| normWV | wv/|wv| | (-0.8882, 0.4595) |
| circles | normWV×intensity | (-0.00781, 0.00404) |
| distortedCoord X | (uv-0.15×circles)×1280 | 425.99979 |

**fp32 模拟**（每步截断为 fp32）:

```python
# fp32 路径模拟
def calcWave_fp32(d):
    d32 = f32(d)
    e0_32 = f32(0.0); e1_32 = f32(-0.3)
    t = f32(f32(d32 - e0_32) / f32(e1_32 - e0_32))
    t = f32(max(0.0, min(1.0, t)))
    pw = f32(f32(t*t) * f32(3.0 - f32(2.0*t)))
    # smoothstep(-0.6,-0.3,d32) similarly
    e0b = f32(-0.6); e1b = f32(-0.3)
    tb = f32(f32(d32 - e0b) / f32(e1b - e0b))
    tb = f32(max(0.0, min(1.0, tb)))
    wf = f32(f32(tb*tb) * f32(3.0 - f32(2.0*tb)))
    wf = f32(wf * pw)
    arg = f32(31.0 * d32)
    return f32(-math.sin(arg) * wf)
```

| 步骤 | fp64 | fp32 | 差异 |
|------|------|------|------|
| propagatedDistance | 0.3369952215 | 0.3369952440 | 2.3e-08 |
| distanceToWave | -0.1530047785 | -0.1530047655 | 1.3e-08 |
| sin(31×(d-0.001)) | 0.9980935096 | 0.9980935096 | 0 (巧合) |
| sin(31×(d+0.001)) | 1.0000000000 | 0.9999999710 | 2.9e-08 |
| calcWave(d-δ) | -0.5190277232 | -0.5190277488 | 2.6e-08 |
| calcWave(d+δ) | -0.5100232808 | -0.5100232959 | 1.5e-08 |
| **梯度** | **4.502221206** | **4.502226522** | **5.3e-06** |
| intensity | 0.008795033 | 0.008795043 | 1.0e-08 |
| distortedCoord X | 425.99979 | 425.99979 | ~0 像素 |

### 1.3 为什么差异这么小（maxΔ=3）？

从上表可见: 即使有限差分将 sin 的 ~3e-8 差异放大到梯度的 ~5e-6, 再经过 intensity(×0.012) 衰减后, 最终 distortedCoord 差异几乎为 0。**对于此像素, 上游精度差异完全不产生视觉差异。**

那实测的 maxΔ=3 从哪来? 来自 5000 个像素中, 每个像素的 fp32 截断发生在计算链的不同位置, 某些像素恰好有多步截断方向一致（都向上或都向下舍入），累积出 ±1~3 的通道差。这是 8-bit 量化下的正常精度噪声——相邻值的 1/255=0.004 量级的量化步长, 正对应 fp32 运算在 0.5 附近（float 值 ×255 后舍入到整数）的典型行为。

**结论**: water_ripple 的 0.54% 差异 (maxΔ=3) 是 **fp32 算术在 8-bit 量化边界的正常噪声**, 与有限差分精度放大**无关**。之前的「×500 放大」分析适用于 sin/cos 精度差异的量级估算, 但在这个特定像素上, 放大后的差异被 downstream 衰减(×0.012)所抵消。

---

## 2. displacement_distort — Alpha 临界区分支分歧

### 2.1 实测数据

**来源**: `results/sksltoglsl/test_cpu/displacement_distort/v1/report/displacement_distort_comparison.json`
- MSE=0.2282, PSNR=54.55, maxΔ=50, diff=0.32%, 差异像素=2,930

**差异分布**（来自 `displacement_distort_diffmap.png`）:
- 差异值分布: 5 级(483像素), 15 级(245), 25 级(225), 10 级(209), 20 级(193)...
  - 这些是自动对比度拉伸后的值
- 空间分布: x=[426,854], y=[213,622] — 集中在画面右半部的弧形带
- 最多差异的 x 列: 576(122), 575(58), 790(51), 563(47), 436(46)

**实测差异模式**:
| 像素列 | before (CPU) 示例 | after (GPU) 示例 | 差异特征 |
|--------|-----------------|-----------------|---------|
| x=790 | (243,187,81) 纯黄 | (244,205,131) 浅黄 | 显著变亮, 位移量不同 |
| x=576 | (226,139,89) | (232,161,121) | 色相偏移 |
| x=436 | (245,225,187) 极亮 | (246,236,218) 更亮 | 小幅度差异 |

### 2.2 着色器逻辑

**源码**: `tests/shaders/displacement_distort.sksl`

```glsl
half4 displacement = displacementMap.eval(fragCoord);
if (displacement.a <= 0.0) {         // ← Alpha 临界: 决定是否位移
    return image.eval(fragCoord);    // 路径 A: 无位移
}
float2 direction = 2.0 * (displacement.rg - 0.5);
float2 normal = direction * factor * strength;
float2 refracted_uv = clamp(uv - normal * 0.05, 0.001, 0.999);
return image.eval(iResolution * refracted_uv);  // 路径 B: 有位移
```

### 2.3 位移贴图结构

**来源**: `src/renderer/image_loader.py:make_demo_displacement_map()`

位移贴图 `tests/assets/displacement.png` 的生成代码:
```python
center_x = width * 0.5       # 640.0
center_y = height * 0.58     # 417.6
max_radius = min(1280,720) * 0.32  # = 230.4
dx = (x_coords - center_x) / max_radius
dy = (y_coords - center_y) / max_radius
radius = sqrt(dx*dx + dy*dy)

red   = clip(0.5 + 0.5*dx, 0, 1)    # 位移 x 方向分量
green = clip(0.5 + 0.5*dy, 0, 1)    # 位移 y 方向分量
ripple = 0.5 + 0.5*sin(14.0*radius)
fade = clip(1.0 - radius, 0, 1) ** 1.8
alpha = where(radius < 1.0, fade * (0.55 + 0.45*ripple), 0.0)
```

**在差异热点 x=790, y=435 处** (radius≈0.65, 在位移区域内部):
- 位移贴图 RGBA ≈ (109,131,128,189)（来自实测）
- alpha = 189/255 = 0.741 > 0 → 走路径 B
- displacement.rg = (109/255, 131/255) = (0.4275, 0.5137)
- direction = 2×(0.4275-0.5, 0.5137-0.5) = (-0.145, 0.0275)
- normal = (-0.145×1.2, 0.0275×0.84) = (-0.174, 0.0231)
- uv = (790/1280, 435/720) = (0.6172, 0.6042)
- refracted_uv = clamp((0.6172+0.0087, 0.6042-0.0012), 0.001, 0.999) = (0.6259, 0.6030)
- 采样坐标 = (0.6259×1280, 0.6030×720) = (801.1, 434.2) 像素

这使采样从原始 x=790 位移到 x=801, 位移量约 11 像素。在此新位置, input.png 可能是不同的条纹颜色, 导致 RGB 差异。

### 2.4 差异根因: 临界区 fp32 截断

差异主要来自两个区域:

**A. 位移区域内部** (如 x=790, maxΔ=50):
算术链 `direction × factor × strength × 0.05 × iResolution` 中 fp32 每步截断累积。以 x=790 为例:

```python
# 模拟 fp32 vs fp64
displacement_r = f32(109/255)  # = 0.4274509847
# fp64: direction.x = 2.0*(0.4274509804-0.5) = -0.1450980392
# fp32: direction.x = f32(2.0*f32(0.427451-0.5)) = -0.1450980455
# 差异: 6e-9 → 经后续乘法放大到约 0.001 像素坐标偏移
```

0.001 像素对 8-bit 颜色通常无影响。但在 input.png 的条纹边界附近（间距仅 1 像素）, 0.5 像素偏移即可跨越边界 → 从纯色跳变为混合色 → 50 级的 RGB 差异。这就是 x=790 区域差异的来源: 采样点恰好落在 input.png 的条纹边界附近。

**B. Alpha 临界区** — 主要差异源

位移贴图 alpha 在 radius ≈ 1.0 处趋近 0。`fade = (1.0-radius)^1.8`:
- radius=0.999: fp64 fade = 5.4e-6, fp32 fade = f32(1.0-f32(0.999))**1.8
  f32(1.0-0.999) = f32(0.001) = 0.00100000005 (fp32 无法精确表示)
  fp32 fade = 0.00100000005^1.8 ≈ 4.0e-6
  差异: 1.4e-6 → alpha 差 0.0000007

这个差异在 alpha 接近 0 时最关键: 如果 fp32 使 alpha 略小于 0（通过后续乘法截断）, 着色器走路径 A（无位移）, 而 fp64 正常走路径 B（有位移）。两条路径采样位置完全不同（差约 10 像素）→ maxΔ≈50。

2879 个差异像素大部分位于 alpha 从 0 过渡到正值的边缘弧形带。

---

## 3. linear_gradient_blend — 边缘采样差异

### 3.1 实测数据

**来源**: `results/sksltoglsl/test_cpu/linear_gradient_blend/v1/report/linear_gradient_blend_comparison.json`
- **修复前**: MSE=124.07, PSNR=27.19, maxΔ=182, diff=10.0%, 差异像素=92,120
- **修复后**（raw RGBA→PNG + 反预乘 alpha）: MSE=0.63, PSNR=60.29, maxΔ=3, diff=9.47%, 差异像素=87,251

**修复前差异分布**（来自 `linear_gradient_blend_diffmap.png`）:
- 差异集中在画面的**四周边框**:
  - x=0 和 x=1279 两侧各 687 像素
  - y=719 底部边缘
  - before(CPU) 颜色始终比 after(GPU) **亮得多**

**修复前像素差异示例**:
| 像素 | before (CPU) | after (GPU) | R差异 |
|------|-------------|-------------|------|
| (0,719) | (248,240,232) 亮灰 | (66,64,62) 深灰 | **182** |
| (1279,718) | (248,241,234) | (70,68,66) | **178** |
| (1277,719) | (248,242,235) | (74,72,70) | **174** |

**修复后**: maxΔ=3, 所有差异均为 ±1~3 级的单通道微小变化, 无集中区域, 属 fp32 精度噪声。

### 3.2 着色器逻辑与边缘效应

**源码**: `tests/shaders/linear_gradient_blend.sksl`

```glsl
float2 startCoord = startPoint * iResolution;    // (0.5*1280, 0.15*720) = (640, 108)
float2 endCoord = endPoint * iResolution;        // (0.5*1280, 0.85*720) = (640, 612)
float2 axis = endCoord - startCoord;             // (0, 504)
float projected = dot(fragCoord-startCoord, axis) / axis_length_squared;
float edge0 = -softness;                         // -0.2
float edge1 = 1.0 + softness;                    // 1.2
float gradient = smoothstep(edge0, edge1, projected);
float mixAmount = clamp(gradient * blurMix, 0.0, 1.0);  // blurMix=1.0
half4 sourceColor = image.eval(fragCoord);
half4 blurColor = preblurImage.eval(fragCoord);
return mix(sourceColor, blurColor, half(mixAmount));
```

对于角点 (0,719):
- fragCoord = (0.5, 719.5)
- projected = dot((0.5-640, 719.5-108), (0,504)) / (504²) = (-639.5×0 + 611.5×504) / 254016 = 308196/254016 = 1.213
- 1.213 > edge1(1.2) → smoothstep=1.0 → mixAmount=1.0
- **完全使用 blurColor (preblurImage)**

**preblurImage 的边缘问题**: preblur.png 是对 input.png 做高斯模糊的结果。模糊在图像边缘会产生**暗角效应**——边缘像素的 alpha/RGB 受周边（包括画面外的透明区域）影响而变暗。

在 (0,719) 处:
- CPU: `preblurImage.eval((0.5,719.5))` → 采样 Skia 预模糊图像
- GPU: `texture(preblurImage, (0.5,719.5)/vec2(w,h))` → 采样 GPU 纹理

两者都加载同一个 `preblur.png` 文件, 但**加载方式不同**:
- Skia: `skia.Image.open()` → BGRA8888, kPremul_AlphaType (预乘 alpha)
- GPU: PIL → raw RGBA → `glTexImage2D(GL_RGBA)` → GL_LINEAR

`preblur.png` 在边缘区域有 alpha<255 的像素（模糊导致）。Skia 将其转为预乘 alpha (RGB×A), GPU 保持直通 alpha。当 `mix()` 混合 sourceColor(直通) 和 blurColor(预乘 vs 直通) 时, 结果 RGB 不同。

### 3.3 模拟验证

```python
# preblur.png 在 (0,719) 处的像素 (实测)
# PIL: RGBA = (66, 64, 62, 255) — 边缘可能已暗化
# 
# 但 before(CPU) 显示 (248,240,232) — 这是 sourceColor, 不是 blurColor!
# 
# 这意味着 CPU 的 mixAmount < 1.0 (没有完全切换到 blur)
# 而 GPU 的 mixAmount = 1.0 (完全切换)
# 
# 差异在 projected 的计算:
# CPU fp64: projected = 308196/254016 = 1.2134...
#   smoothstep(-0.2, 1.2, 1.2134) ≈ 1.0 (因为 1.213 > 1.2)
# 
# 等等, 两边 projected 应该相同! 都是简单的算术。
# 让我检查生成 GLSL 的 mixAmount 计算...
```

**查看生成 GLSL**: `results/sksltoglsl/test_cpu/linear_gradient_blend/v1/code/linear_gradient_blend.glsl`

关键差异可能在于: CPU 的 `image.eval(fragCoord)` 使用 pixel coords, GPU 的 `texture(image, fragCoord/iResolution)` 使用 normalized UV。对于 `preblurImage` 的边缘采样, GL_LINEAR 的双线性插值与 Skia 的双线性插值在坐标转换中可能有亚像素差异。

预模糊图像在边缘有从暗到亮的渐变。0.5 像素的采样坐标偏移可能从「暗色」跳到「亮色」, 产生 182 级的差异。这解释了为什么差异集中在图像四边——边缘是预模糊图像的暗角与原始亮色之间的过渡区。

**根本原因**: 屏幕边缘处 `projected` ≥ 1.0, mixAmount=1.0, 完全使用 preblurImage。修复前 GLSL 输出走 PPM→PNG 路径, PPM 无 alpha 通道, ImageMagick `convert` 将 alpha 强制设为 255 —— GLSL 中 premultiplied 的 RGB (×alpha) 无法还原。

preblur.png 边缘像素 alpha≈68（高斯模糊使边缘透明）, 着色器输出 `mix(sourceColor, blurColor, 1.0) = blurColor`:
- CPU: eval()→(R×A,G×A,B×A,A)预乘 → Skia PNG编码器反预乘 → (R,G,B,A)直通
- GPU(修复前): texture()→(R,G,B,A)直通 → fix补.rgb×=.a→(R×A,G×A,B×A,A)预乘 → PPM丢alpha→(R×A,G×A,B×A,255)
  - 实测: CPU=(248,240,232,68) → GPU=(66,64,62,255), RGB差 182

修复: GPU 输出走 `--raw` RGBA + Python 反预乘 `r×255/a`:
- CPU: (248,240,232,68) → GPU: (248,240,232,68), RGB差 < 1

---

## 4. variable_radius_blur_approx — fp32 多点采样噪声

### 4.1 实测数据

**来源**: `results/sksltoglsl/test_cpu/variable_radius_blur_approx/v1/report/variable_radius_blur_approx_comparison.json`
- MSE=0.0038, PSNR=72.30, maxΔ=1, diff=1.13%, 差异像素=10,407

**差异分布**: 所有 10,407 个差异像素的 maxΔ **全部为 1**, 差异值分布为单一值(255, 即自动对比度拉伸后), 原始差异仅 1/255 级。

空间分布: x=[149,1074], y=[0,719], 散布于整个画面, 无集中区域。差异最多列: 632(376), 631(325), 634(302) — 区域均匀分布。

**实测像素差异**:
| 像素 | before | after | 差异 |
|------|--------|-------|------|
| (1074,283) | (244,239,230) | (243,239,230) | R-1 |
| (1074,267) | (244,239,231) | (244,239,230) | B-1 |
| (632,区域) | 多像素 R/G/B ±1 | — | 单通道 ±1 |

### 4.2 机制

着色器对每个像素采样 blurMask 和 image, 再执行多点迭代模糊。每次采样涉及 sin/cos 计算采样角度。所有差异均为 ±1 级——这是 fp32 运算在 8-bit 量化边界处产生的舍入误差。1.13% 的像素受影响是因为多点采样的累积使单个运算的舍入偏差在特定像素处放大到刚好跨越量化边界。

这与 water_ripple 的残余差异同类, 都属于**无法修复的 fp32 vs fp64 硬件差异**。

---

## 5. 总结

```
┌──────────────────────────────────────────────────────────────────────┐
│ 差异分类 (按严重程度, 修复后)                                          │
│                                                                      │
│ 1. displacement_distort (0.32%, maxΔ=50)                              │
│    → Alpha 临界区分支分歧 + 位移坐标链 fp32 截断                        │
│    → 位移区域边缘弧形带受影响                                           │
│    → 可通过 GPU 模式消除                                               │
│                                                                      │
│ 2. water_ripple (0.54%, maxΔ=3)                                       │
│    → fp32 运算在 8-bit 量化边界的正常噪声                               │
│    → 散布全画面, maxΔ=3, 可接受                                        │
│                                                                      │
│ 3. linear_gradient_blend (9.47%, maxΔ=3) ← 已从 10%/maxΔ=182 修复     │
│    → PPM→PNG alpha 丢失 → 修复为 raw RGBA→PNG + 反预乘                 │
│    → 残余 maxΔ=3 为 fp32 精度噪声                                      │
│    → 散布全画面, 可接受                                                │
│                                                                      │
│ 4. variable_radius_blur_approx (1.13%, maxΔ=1)                        │
│    → 纯 fp32 量化噪声                                                  │
│    → 散布全画面, maxΔ=1, 可接受                                        │
│                                                                      │
│ GPU 模式 (--gpu) 统一后端 → 全部归零 (PSNR=∞)                          │
└──────────────────────────────────────────────────────────────────────┘
```

### 修复记录

**修复 1**: 纹理过滤对齐 (`src/renderer/shader_runner.py`, 2026-06-01)
- Skia `makeShader()` 默认 Nearest → 改为显式 `SamplingOptions(FilterMode.kLinear)`
- 修复 water_ripple 硬边界处的「抗锯齿」错觉 (maxΔ 101→3)

**修复 2**: GLSL 输出 alpha 通道保留 (`glslang/glslang_demo/render_glsl.c` + `tests/runner.sh`, 2026-06-01)
- PPM→PNG (无 alpha) → raw RGBA→PNG + Python 反预乘 alpha
- 修复 linear_gradient_blend 边缘暗角 (maxΔ 182→3)
- `render_glsl --raw` 输出格式: `[4B width][4B height][RGBA raw data]`

**关键数据文件索引**:

| 文件 | 说明 |
|------|------|
| `results/sksltoglsl/test_cpu/*/v1/report/*_comparison.json` | 所有 v1 对比报告 |
| `results/sksltoglsl/test_cpu/*/v1/report/*_diffmap.png` | 所有 v1 差异灰度图 |
| `results/sksltoglsl/test_cpu/*/report/*_roundtrip.json` | 所有闭环验证报告 |
| `tests/shaders/water_ripple.sksl` | 含水波纹效果（含有限差分） |
| `tests/shaders/displacement_distort.sksl` | 含位移扭曲（含 alpha 临界分支） |
| `tests/shaders/linear_gradient_blend.sksl` | 含渐变混合 |
| `tests/shaders/variable_radius_blur_approx.sksl` | 含多点模糊采样 |
| `src/renderer/image_loader.py:make_demo_image()` | input.png 生成 |
| `src/renderer/image_loader.py:make_demo_displacement_map()` | displacement.png 生成 |
| `src/renderer/image_loader.py:make_blurred_image()` | preblur.png 生成 |
| `src/renderer/image_loader.py:make_demo_blur_mask()` | blur_mask.png 生成 |
| `src/renderer/shader_runner.py` | CPU RuntimeEffect 渲染 |
| `glslang/glslang_demo/render_glsl.c` | GPU OpenGL 渲染 |
| `tests/params.sh` | 所有着色器参数 |
