# SKSL 着色器渲染结果

本目录包含 `tests/shaders/` 下所有 SKSL 着色器文件的渲染输出。每个 `.sksl` 文件对应一个 `.params.json` 配置文件和一个 `.png` 渲染结果。

## 渲染概况

- **SKSL 文件总数**: 121（原始 119 + 2 个从片段组合的完整着色器）
- **成功渲染（有可见内容）**: 107 个
- **无法渲染**: 9 个
  - 2 个含 `float3x3` 矩阵 uniform（Skia 不支持）：`sdf_transform_shader_shape/prog`, `prog_1`
  - 5 个需要 SDF RuntimeEffect 子着色器（PNG 纹理无法提供 SDF 距离数据）：`sdf_border_shader/code`, `sdf_clip_shader/code`, `sdf_color_shader/code`, `sdf_path_shader_shape/normal-calculation-shader`, `filter/sdf_edge_light/shader`
  - 2 个需要多通道管线中间输出（前序 pass 的特定数据格式）：`contour_diagonal_flow_light_shader/flow-light-prog`, `particle_circular_halo_shader/main-shader-prog`
- **着色器片段（已组合）**: `color_gradient_effect` 的 5 个片段已根据 C++ 源码拼接为 2 个完整着色器
- **已修复的数组 uniform 问题**: `circle_flowlight_effect`（2 个）、`color_gradient_shader_filter`（2 个）、`radial_gradient_shader_mask`（2 个）共 6 个文件的数组 uniform 已展开为独立命名 uniform
- **已修复的参数问题**: `border_light_shader`、`spatial_point_light`（2 个）、`double_ripple_shader_mask`（2 个）、`mesa_blur/mix-mesa` 的 uniform 值已调整至可见范围

## 生成的中间资源

为了让多通道管线着色器能够独立渲染，生成了以下中间资源（存放于 `tests/assets/`）：

| 资源文件 | 说明 |
|---|---|
| `blur_level1~4.png` | 四级不同半径的模糊图像（由 kawase blur 生成） |
| `blur_edge.png` | 大半径模糊（用于边缘光泛光） |
| `blur_bg.png` | 小半径模糊（用于背景模糊） |
| `edge_detect.png` | 边缘检测结果 |
| `edge_blur0~4.png` | 五级边缘模糊（用于 bloom 合成） |
| `sdf_rrect.png` | 圆角矩形 SDF 形状 |
| `sdf_triangle.png` | 三角形 SDF 形状 |
| `ripple_normal.png` | 波纹法线贴图 |
| `wave_normal.png` | 波浪渐变法线贴图 |

## 参数配置说明

每个着色器的 `.params.json` 根据其 C++ 源码中的实际用途进行配置：

- **简单图像滤镜**：使用 `assets/input.png` 作为输入
- **模糊通道**：使用原图或边缘检测结果作为输入
- **合成/混合通道**：使用对应的已渲染中间图像作为各层输入
- **SDF 依赖着色器**：使用生成的 `sdf_rrect.png` 或 `sdf_triangle.png` 作为 SDF 形状输入
- **磨砂玻璃/空间玻璃**：使用模糊图像 + 法线贴图作为子输入
- **数组 uniform**（如 `color[4]`）：已转换为索引格式（`color[0]`, `color[1]` 等）以支持当前渲染管线

---

## Filter（滤镜）

### aibar_shader_filter
- **prog.sksl** — AI Bar 二值化滤镜。将输入图像转换为二值（高/低）表示，支持饱和度调节。预期效果：带色彩饱和度控制的色调分离/二值化图像。

### blur_bubbles_rise_filter
- **gaussian-blur.sksl** — 高斯模糊通道（水平/垂直）。预期效果：模糊后的输入图像。
- **mask-mix.sksl** — 使用模糊遮罩混合原图与模糊图像。预期效果：模糊区域与原图区域之间的过渡。
- **resample.sksl** — 图像重采样。预期效果：重采样后的图像。

### color_gradient_shader_filter
- **prog.sksl** — 多色渐变混合。在图像上放置最多 12 个彩色光球并混合。预期效果：彩色渐变叠加层。
- **with-mask.sksl** — 同上，带遮罩控制。使用 blur_mask.png 作为遮罩输入。预期效果：被遮罩控制的彩色渐变。

### content_light_shader_filter
- **content-light.sksl** — 内容光照效果。模拟从 3D 位置照射的光。预期效果：带方向光照高亮的图像。

### direction_light_shader_filter
- **direction-light-no-normal.sksl** — 无 normal map 的方向光。预期效果：简单方向光叠加。
- **direction-light.sksl** — 带 normal map 的方向光。使用 blur_mask.png 作为 normal 输入。预期效果：带法线细节的光照。
- **normal-mask.sksl** — 从遮罩提取 normal map。预期效果：RGB normal map 图像。

### dispersion_shader_filter
- **dispersion.sksl** — 色散效果。R/G/B 通道沿不同方向偏移。使用 blur_mask.png 作为遮罩。预期效果：彩色镶边图像。

### displacement_distort_shader_filter
- **displacement-distort.sksl** — 位移扭曲。使用 displacement.png 作为扭曲贴图。预期效果：扭曲变形的图像。

### distortion_collapse_filter
- **shader.sksl** — 透视变形加桶形校正。预期效果：带可选桶形畸变的透视变形图像。

### edge_light_shader_filter（多通道管线）
- **convert-frag.sksl** — 图像直通。预期效果：原图输出。
- **detect-frag.sksl** — 边缘检测。预期效果：高亮显示图像边缘。
- **gaussian-frag.sksl** — 边缘泛光模糊。使用 edge_detect.png 作为输入。预期效果：边缘的模糊版本。
- **composite-frag.sksl** — 5 级模糊合成 bloom。使用 edge_blur0~4.png。预期效果：多级 bloom/glow 效果。
- **alpha-gradient.sksl** — 泛光与原图混合。使用 input.png + edge_blur2.png。预期效果：泛光增强图像。
- **add-mask.sksl** — 遮罩光照叠加。使用 blur_mask.png 作为遮罩。预期效果：遮罩控制强度的泛光。

### frosted_glass_shader_filter（多通道管线）
- **main-shader-prog.sksl** — 完整磨砂玻璃效果。使用 blur_edge.png（边缘折射）、blur_bg.png（背景模糊）、ripple_normal.png（SDF normal）。预期效果：逼真磨砂玻璃材质。

### grey_shader_filter
- **grey-gradation.sksl** — 灰度阶调滤镜。使用 YUV 色彩空间调整亮度。预期效果：色调调整后的图像。

### heat_distortion_filter
- **heat-distortion.sksl** — 热浪扭曲。使用 Perlin 噪声产生热浪效果。预期效果：热浪扭曲图像。

### kawase_blur_shader_filter
- **blur.sksl** — Kawase 模糊通道。预期效果：模糊图像。
- **blur-af.sksl** — 高级 Kawase 模糊（多偏移采样）。预期效果：高质量模糊。
- **mix.sksl** — 模糊与原图混合。使用 blur_level2.png + input.png。预期效果：清晰与模糊之间的过渡。
- **simple.sksl** — 简单直通。预期效果：未修改的输入。

### linear_gradient_blur_shader_filter
- **prog.sksl** — 渐变控制模糊过渡。使用 blur_level2.png + blur_mask.png。预期效果：渐变控制的模糊。

### magnifier_shader_filter
- **magnifier-shader-with-sdf-prog.sksl** — 放大镜效果。使用 sdf_rrect.png 作为镜片形状。预期效果：SDF 镜片内放大的图像。

### mask_transition_shader_filter
- **prog.sksl** — 图层过渡。使用 input.png（顶层）+ blur_level3.png（底层）+ blur_mask.png（遮罩）。预期效果：遮罩控制的交叉淡化。

### mesa_blur_shader_filter
- **blur-mesa.sksl** — MESA 模糊。预期效果：模糊图像。
- **direction-blur-mesa.sksl** — 方向性 MESA 模糊。预期效果：方向模糊。
- **grey-x.sksl** — 灰度调节 + 模糊。预期效果：颜色调整并模糊的图像。
- **mix-mesa.sksl** — 混合通道。使用 blur_level2.png。预期效果：颜色因子调整的模糊。
- **simple.sksl** — 简单直通。预期效果：未修改的输入。

### motion_blur_shader_filter
- **motion-blur.sksl** — 运动模糊。预期效果：方向性涂抹效果。

### sdf_edge_light（多通道管线）
- **pass-through.sksl** — SDF 形状解码。使用 sdf_rrect.png 作为 SDF 输入。预期效果：SDF 距离可视化。
- **shade-code.sksl** — 图像合成。预期效果：合成图像。
- **shader.sksl** — SDF 边缘光效果。使用 sdf_rrect.png + blur_level2.png + light_mask.png。预期效果：SDF 形状上的发光边缘。

### sdf_from_image_filter（多通道管线）
- **box-blur-prog.sksl** — SDF 预处理盒式模糊。预期效果：模糊图像。
- **jfa-iteration-prog.sksl** — Jump Flood 算法迭代。预期效果：SDF 场。
- **shader-string.sksl** — JFA 准备通道。预期效果：初始 SDF 种子。
- **shader-string_1.sksl** — JFA 处理结果。预期效果：处理后的 SDF。
- **shader-string_2.sksl** — SDF 导数填充。使用 blur_level2.png。预期效果：完整 SDF。

### sound_wave_filter
- **sound-wave.sksl** — 音频可视化。圆形声波 + 色轮 + 冲击波扭曲。预期效果：动画音柱和波纹环。

### variable_radius_blur_shader_filter（多通道管线）
- **generate-texture.sksl** — 纹理准备直通。预期效果：未修改输入。
- **horizontal-blur.sksl** — 水平可变半径模糊。使用 blur_mask.png 作为渐变。预期效果：模糊量变化的水平模糊。
- **horizontal-blur-masked.sksl** — 同上，带遮罩感知。预期效果：遮罩控制的水平模糊。
- **vertical-blur.sksl** — 垂直可变半径模糊。预期效果：模糊量变化的垂直模糊。
- **vertical-blur-masked.sksl** — 同上，带遮罩感知。预期效果：遮罩控制的垂直模糊。

### water_ripple_filter
- **mini-recv.sksl** — 水波纹（接收端）。预期效果：波纹扭曲图像。
- **smrecv.sksl** — 平滑水波纹（接收端）。预期效果：平滑波纹扭曲。
- **smsend.sksl** — 平滑水波纹（发送端）。预期效果：带波传播的扭曲。
- **ssmutual.sksl** — 互反馈水波纹。预期效果：交互式波纹扭曲。

---

## Mask（遮罩）

遮罩着色器产生灰度或法线向量输出，供滤镜着色器作为子输入使用。

### double_ripple_shader_mask
- **prog.sksl** — 双波纹遮罩（遮罩输出）。两个带湍流噪声的同心波纹环。预期效果：灰度双环遮罩。
- **prog_1.sksl** — 双波纹遮罩（法线输出）。预期效果：RGB normal map。

### frame_gradient_shader_mask
- **mask.sksl** — 边框渐变遮罩。圆角矩形边框，带贝塞尔曲线和轴向羽化。预期效果：羽化边框遮罩。

### pixel_map_shader_mask
- **prog.sksl** — 像素图遮罩。将图像映射到目标区域。预期效果：指定区域内的映射图像。

### radial_gradient_shader_mask
- **prog.sksl** — 径向渐变遮罩（遮罩输出）。预期效果：径向灰度渐变。
- **prog_1.sksl** — 径向渐变遮罩（法线输出）。预期效果：RGB normal map。

### ripple_shader_mask
- **prog.sksl** — 单波纹遮罩（遮罩输出）。环形波纹。预期效果：灰度环形遮罩。
- **prog_1.sksl** — 单波纹遮罩（法线输出）。预期效果：RGB normal map。

### use_effect_shader_mask
- **prog.sksl** — 简单图像直通遮罩。预期效果：输入图像透传。

### wave_disturb_shader_mask
- **wave-disturbance-prog.sksl** — 波浪扰动遮罩。从点击位置向外传播的波纹。预期效果：同心波环位移向量。

### wave_gradient_shader_mask
- **prog.sksl** — 波浪渐变遮罩（遮罩输出）。湍流调制波环 + 模糊。预期效果：噪声环渐变。
- **prog_1.sksl** — 波浪渐变遮罩（法线输出）。预期效果：RGB normal map。

---

## Shader（着色器效果）

### aurora_noise_shader
- **prog.sksl** — 极光噪声生成器。程序化极光状噪声图案。预期效果：类似北极光的彩色波浪噪声。
- **prog_1.sksl** — 极光垂直模糊。使用 input.png 作为输入纹理。预期效果：垂直模糊极光。
- **prog_2.sksl** — 极光上采样/合成。预期效果：全分辨率极光效果。

### border_light_shader
- **prog.sksl** — 边框光效果。沿圆角矩形边缘的发光光带。预期效果：发光边框。

### border_sdf_shader
- **border-code.sksl** — SDF 边框渲染。使用 sdf_rrect.png 作为 SDF 形状。预期效果：SDF 形状上的彩色边框。

### circle_flowlight_effect
- **circle-flowlight-shader.sksl** — 圆形流光（无遮罩）。旋转渐变色 + 径向扭曲。预期效果：彩色旋转流光。
- **circle-flowlight-shader-with-mask.sksl** — 圆形流光（带遮罩）。使用 blur_mask.png 控制可见性。预期效果：遮罩控制的流光。

### color_gradient_effect
- **color-gradient-shader.sksl** — 完整颜色渐变着色器（由 HEAD + COMMN + END 组合）。12 色渐变混合 + 屏幕空间抖动。预期效果：多彩渐变背景。
- **color-gradient-shader-with-mask.sksl** — 完整带遮罩颜色渐变着色器（由 WITH_MASK_HEAD + COMMN + WITH_MASK_END 组合）。使用 blur_mask.png 控制可见区域。预期效果：遮罩控制的多彩渐变。
- **brightness-shader-code.sksl** — 亮度调节通道。使用子着色器作为颜色渐变输入。预期效果：亮度调整结果。
- **color-gradient-shader-head/commn/end** — 着色器片段（无独立 main 函数），已组合为上述完整版本，原文件保留作为参考。
- **color-gradient-shader-with-mask-head/end** — 着色器片段，已组合为完整版本。

### contour_diagonal_flow_light_shader（多通道管线）
- **precalculationformorecurves-prog.sksl** — 贝塞尔曲线 SDF 预计算。使用 sdf_rrect.png 作为输入。预期效果：复杂路径 SDF。
- **flow-light-prog.sksl** — 沿轮廓的流光。使用 sdf_rrect.png 作为预计算结果。预期效果：发光流线。
- **convert-img-prog.sksl** — SDF 图像转换。使用 sdf_rrect.png。预期效果：逐像素曲线段信息。
- **sdf-mask-prog.sksl** — SDF 遮罩生成。预期效果：SDF 遮罩图像。
- **blend-img-prog.sksl** — 最终混合。使用 blur_edge.png + blur_level1.png。预期效果：完整轮廓流光 + bloom。

### frosted_glass_effect
- **main-shader-prog.sksl** — 磨砂玻璃效果。使用 blur_edge.png + blur_bg.png + wave_normal.png。预期效果：逼真磨砂玻璃。

### particle_circular_halo_shader（多通道管线）
- **glow-halo-prog.sksl** — 光晕生成器。预期效果：柔和圆形光晕。
- **single-particle-halo-prog.sksl** — 单粒子光晕。预期效果：带噪声扰动的发光粒子。
- **particle-halo-prog.sksl** — 粒子群组合。预期效果：多粒子光晕场。
- **main-shader-prog.sksl** — 最终合成。使用 blur_level1.png + blur_level2.png。预期效果：完整圆形光晕。

### sdf_edge_light_shader
- **shader.sksl** — SDF 边缘光。使用 sdf_rrect.png + light_mask.png。预期效果：SDF 形状发光边缘。

### spatial_glass_effect
- **main-shader-prog.sksl** — 空间玻璃效果。使用 blur_bg.png + wave_normal.png。预期效果：空间感知玻璃材质。

### spatial_point_light
- **prog-no-mask.sksl** — 无遮罩点光源。预期效果：带衰减的圆形光斑。
- **prog-with-mask.sksl** — 带遮罩点光源。使用 light_mask.png。预期效果：表面感知点光源。

### wavy_ripple_light_shader
- **prog.sksl** — 波浪涟漓光。同心环光 + 波调制。预期效果：动画波浪环。

---

## Shape（SDF 形状）

SDF 形状着色器产生有符号距离场数据，通常作为滤镜和效果着色器的输入。

### sdf_border_shader
- **code.sksl** — SDF 描边。使用 sdf_rrect.png 作为形状。预期效果：彩色边框轮廓。
- **outline-code.sksl** — SDF 描边（轮廓变体）。预期效果：彩色轮廓描边。

### sdf_clip_shader
- **code.sksl** — SDF 裁剪。使用 sdf_rrect.png。预期效果：SDF 距离值。

### sdf_color_shader
- **code.sksl** — SDF 颜色填充。使用 sdf_rrect.png。预期效果：纯色 SDF 形状。

### sdf_distort_op_shader_shape
- **shader.sksl** — SDF 扭曲操作。使用 sdf_rrect.png 作为形状。预期效果：扭曲变形的 SDF 形状。

### sdf_path_shader_shape（多通道管线）
- **clear-inf-shader.sksl** — 清除为无穷远。预期效果：纯白图像。
- **precalculation-for-sdf-shader.sksl** — 路径 SDF 预计算。使用 sdf_rrect.png。预期效果：路径 SDF 距离场。
- **sdf-propagation-shader.sksl** — Jump-flood SDF 传播。使用 sdf_rrect.png + blur_mask.png。预期效果：传播后的 SDF。
- **normal-calculation-shader.sksl** — 法线计算。使用 sdf_rrect.png。预期效果：RGB normal map。

### sdf_pixelmap_shader_shape
- **prog.sksl** — 像素图转 SDF。使用 input.png。预期效果：图像 alpha 的 SDF 距离场。
- **prog_1.sksl** — 像素图转 SDF（法线输出）。预期效果：normal map。

### sdf_rrect_shader_shape
- **rrect-shader-prog.sksl** — 逐角半径圆角矩形 SDF。预期效果：四角不同半径的圆角矩形 SDF。
- **sdf-grad-prog.sksl** — 圆角矩形 SDF 带法线。预期效果：SDF 梯度/normal map。
- **uniform-rrect-shader-prog.sksl** — 统一半径圆角矩形 SDF。预期效果：四角相同半径的圆角矩形 SDF。
- **uniform-sdf-grad-prog.sksl** — 统一半径圆角矩形带法线。预期效果：normal map。

### sdf_shadow_shader
- **code.sksl** — SDF 阴影。使用 sdf_rrect.png。预期效果：SDF 形状的投影。
- **elevation-code.sksl** — SDF 高程阴影。使用 sdf_rrect.png。预期效果：高程感知阴影。

### sdf_transform_shader_shape
- **prog.sksl** — SDF 矩阵变换（含 float3x3 uniform，当前渲染管线不支持）。
- **prog_1.sksl** — SDF 矩阵变换 + 法线（含 float3x3 uniform，当前渲染管线不支持）。
- **gravity-pull-prog.sksl** — 重力拉扯变形。使用 sdf_rrect.png。预期效果：重力变形 SDF 形状。
- **gravity-pull-normal-prog.sksl** — 重力拉扯变形（法线输出）。预期效果：变形 normal map。

### sdf_triangle_shader_shape
- **prog.sksl** — 三角形 SDF。圆角三角形。预期效果：圆角三角形 SDF。
- **sdf-grad-prog.sksl** — 三角形 SDF 带法线。预期效果：normal map。

### sdf_union_op_shader_shape
- **prog.sksl** — 硬合并。使用 sdf_rrect.png + sdf_triangle.png。预期效果：两个 SDF 形状的并集。
- **prog_1.sksl** — 平滑合并。预期效果：平滑混合并集。
- **prog_2.sksl** — 平滑合并 + 法线。预期效果：平滑并集 normal map。

---

## 多通道管线

以下效果需要多个着色器通道串联执行。在独立渲染时，各通道使用了预生成的中间图像作为子输入：

- **磨砂玻璃**：4 个子输入（原图 → 边缘模糊 → 背景模糊 → SDF normal）
- **边缘光**：6 个通道（convert → detect → gaussian blur → composite → alpha gradient → add mask）
- **从图像生成 SDF**：5 个通道（prepare → box blur → JFA iteration → process → derivative fill）
- **粒子圆环光晕**：4 个通道（glow → single particle → particle collection → main composite）
- **SDF 路径形状**：4 个通道（clear → precalculate → propagate → normal calculation）
- **轮廓对角流光**：5 个通道（precalculate → flow light → convert → SDF mask → blend）
