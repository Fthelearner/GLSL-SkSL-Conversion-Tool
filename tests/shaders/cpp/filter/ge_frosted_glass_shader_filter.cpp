/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ge_frosted_glass_shader_filter.h"

#include "ge_log.h"
#include "ge_mesa_blur_shader_filter.h"
#include "ge_sdf_rrect_shader_shape.h"

namespace OHOS {
namespace Rosen {
constexpr size_t NUM_0 = 0;
constexpr size_t NUM_1 = 1;
constexpr size_t NUM_2 = 2;
static constexpr char MAIN_SHADER_PROG[] = R"(
    // -----------------------------------------------------------------------------
    // Rounded-rect frosted glass style shader with toggleable effects.
    // Groups are ordered as: COMMON → BACKGROUND → INNER SHADOW → ENV LIGHT → HIGHLIGHT → main
    // Each effect block puts "parameters first, then functions", so you can splice blocks easily.
    // -----------------------------------------------------------------------------

    // ============================================================================
    // 0) COMMON PARAMS & FUNCTIONS (shared by multiple effects)
    // ============================================================================
    uniform shader image;
    uniform shader edgeBlurredImg;
    uniform shader bgBlurredImg;
    uniform shader sdfNormalImg;
    uniform vec2 iResolution;
    // ----- Shape Core -----
    uniform float borderWidth;   // SDF band width for emboss math
    uniform float offset;        // inner band offset for embossing

    // ----- AA / Numeric -----
    const float ANTI_ALIASING = 0.5;  // global AA softness scaling
    const float PI = 3.14159265358979323846;
    const float EPSILON = 1e-4;
    const float N_EPS = 1e-6;

    // ----- Buffers / Scale -----
    uniform float downSampleFactor;  // full-res : blur buffer res ratio (1 = same)

    // ----- Common SDF & Geometry helpers -----
    float SdfRRect(vec2 p, vec2 b, float r)
    {
        vec2 q = abs(p) - b + r;
        return length(max(q, 0.0)) - r;
    }

    vec2 SafeNormalize(vec2 v)
    {
        return v / max(length(v), N_EPS);
    }

    vec2 GradRRect(vec2 p, vec2 b, float r)
    {
        float rr = min(r, min(b.x, b.y));
        vec2 w = abs(p) - b - vec2(rr);
        float g = max(w.x, w.y);
        vec2 q = max(w, 0.0);
        float l = length(q);

        vec2 grad;
        if (g > 0.0) {
            grad = (l > N_EPS) ? (q / l) : vec2(1.0, 0.0);
        } else {
            grad = (w.x > w.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
        }
        grad *= sign(p);
        return grad;  // outward (unnormalized)
    }

    vec2 ToDownsamplePx(vec2 deltaFullPx, float downFactor)
    {
        return deltaFullPx / max(downFactor, N_EPS);
    }

    // ----- Image sampling helpers -----
    vec4 BaseBlur(vec2 coord)
    {
        return bgBlurredImg.eval(coord);  // pre-blurred background
    }

    // Optional original-image pixel sampling (not used in main but kept for reuse)
    vec4 SampleUV(vec2 uv, vec2 res)
    {
        return image.eval(uv + 0.5 * res);
    }

    // ----- Common Color Utilities (Sat / KBS / CompareBlend) -----
    vec3 Sat(vec3 src255, float n, float p1r, float p2r, float p1g, float p2g, float p1b, float p2b)
    {
        // `src255` in [0,255]; `n` is base saturation factor
        float r = src255.r;
        float g = src255.g;
        float b = src255.b;
        float rnn = r * (0.2412016 * (1. - n) + n) + g * (0.6922296 * (1. - n)) + b * (0.0665688 * (1. - n));
        float gnn = r * (0.2412016 * (1. - n)) + g * (0.6922296 * (1. - n) + n) + b * (0.0665688 * (1. - n));
        float bnn = r * (0.2412016 * (1. - n)) + g * (0.6922296 * (1. - n)) + b * (0.0665688 * (1. - n) + n);

        float dr = rnn - r;
        float grt = step(0., dr);
        float rr = (r + dr * p1r) * grt + (r + dr * p2r) * (1. - grt);

        float dg = gnn - g;
        grt = step(0., dg);
        float gg = (g + dg * p1g) * grt + (g + dg * p2g) * (1. - grt);

        float db = bnn - b;
        grt = step(0., db);
        float bb = (b + db * p1b) * grt + (b + db * p2b) * (1. - grt);

        return vec3(rr, gg, bb);
    }

    vec3 ApplyKBS(vec3 c01, float K, float B, float S, float p1r, float p2r, float p1g, float p2g,
        float p1b, float p2b)
    {
        vec3 x = c01 * 255.0;
        x = x * K + vec3(B);
        x = Sat(x, S, p1r, p2r, p1g, p2g, p1b, p2b);
        return clamp(x / 255.0, 0.0, 1.0);
    }

    vec3 CompareBlend(vec3 src, vec3 dst)
    {
        const vec3 grayBase = vec3(0.2412016, 0.6922296, 0.0665688);
        float lb = dot(src, grayBase);
        float le = dot(dst, grayBase);
        return mix(src, dst, le / max((lb + le), EPSILON));
    }

    // ============================================================================
    // 1) BACKGROUND (DARKEN + VIBRANCY)
    //    Parameters first, then functions that specifically shape the background.
    // ============================================================================

    // ----- Params -----
    uniform float bgFactor;  // global attenuation for blurred background

    // ----- Functions specific to background shaping -----
    vec3 BlurVibrancy(vec3 c01)
    {
        vec3 x = c01 * 255.0;
        x = -0.0000289 * pow(x, vec3(3.)) + 0.0108341 * pow(x, vec3(2.)) + 0.0073494 * x + 25.4709106;
        x = Sat(x, 1.2, 0.3, 0.5, 0.5, 0.5, 1., 1.);
        return clamp(x / 255.0, 0.0, 1.0);
    }

    // ============================================================================
    // 2) INNER EDGE SHADOW
    //    Parameters first, then functions used only by inner shadow.
    //    (Shared geometry helpers already live in COMMON.)
    // ============================================================================

    // ----- Params -----
    uniform float innerShadowRefractPx;  // inward offset along outward normal (px, full-res)
    uniform float innerShadowWidth;
    uniform float innerShadowExp;
    uniform float sdK;                   // K/B/S for inner shadow color
    uniform float sdB;
    uniform float sdS;

    // ----- Functions (wrappers specific to inner shadow coloring) -----
    vec3 InnerShadowVibrancy(vec3 c01)
    {
        return ApplyKBS(c01, sdK, sdB, sdS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
    }

    // Optional extra inner-shadow exponential fade (not used in your main now)
    float EdgeExpInAA(vec2 p, vec2 b, float r, float decayLenPx, float aaPx)
    {
        float sd = SdfRRect(p, b, r);
        float x = max(0.0, -sd);
        float core = exp(-x / max(decayLenPx, N_EPS));
        float cover = smoothstep(aaPx, -aaPx, sd);
        return core * cover;
    }

    // ============================================================================
    // 3) ENVIRONMENTAL EDGE LIGHT (outward refraction brightening)
    //    Parameters first, then functions used only by env light.
    // ============================================================================

    // ----- Params -----
    uniform float refractOutPx;  // outward offset along outward normal (px, full-res)
    uniform float envK;          // K/B/S for env light color
    uniform float envB;
    uniform float envS;

    // ----- Functions (wrapper specific to env light coloring) -----
    vec3 EdgeLightVibrancy(vec3 c01)
    {
        return ApplyKBS(c01, envK, envB, envS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
    }

    // ============================================================================
    // 4) DIAGONAL EDGE HIGHLIGHT (band ∩ fan mask)
    //    Parameters first, then functions used only by highlight.
    // ============================================================================

    // ----- Params -----
    uniform float highLightAngleDeg;
    uniform float highLightFeatherDeg;
    uniform float highLightWidthPx;
    uniform float highLightFeatherPx;
    uniform float highLightShiftPx;
    uniform vec2 highLightDirection;
    uniform float hlK;  // K/B/S for highlight color
    uniform float hlB;
    uniform float hlS;

    // ----- Functions (highlight-specific) -----
    float DiagonalFanMask(vec2 posFromCenter, vec2 dir, float angleDeg, float featherDeg)
    {
        vec2 p = normalize(posFromCenter);
        vec2 d = normalize(dir);
        float angle = angleDeg * (PI / 180.0);
        float feather = max(1e-4, featherDeg * (PI / 180.0));
        float c1 = clamp(dot(d, p), -1.0, 1.0);
        float c2 = clamp(dot(-d, p), -1.0, 1.0);
        float theta1 = acos(c1);
        float theta2 = acos(c2);
        float lobe1 = 1.0 - smoothstep(angle * 0.5, angle * 0.5 + feather, theta1);
        float lobe2 = 1.0 - smoothstep(angle * 0.5, angle * 0.5 + feather, theta2);
        return clamp(lobe1 + lobe2, 0.0, 1.0);
    }

    float EdgeBandAA(float sd, float widthPx, float featherPx, float shiftPx)
    {
        float a = max(featherPx, N_EPS);
        float coverOuter = smoothstep(a, -a, sd + shiftPx);
        float coverInner = smoothstep(a, -a, sd + shiftPx + max(widthPx, 0.0));
        return clamp(coverOuter - coverInner, 0.0, 1.0);
    }

    vec3 EdgeHighlightVibrancy(vec3 c01)
    {
        return ApplyKBS(c01, hlK, hlB, hlS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
    }

    // ============================================================================
    // 5) MAIN (you already split logic per-effect; kept intact, only depends on above)
    // ============================================================================
    vec4 main(vec2 fragCoord)
    {
        // Primary & inner-offset SDFs
        vec4 sdfNormal = sdfNormalImg.eval(fragCoord);
        float sd = sdfNormal.a;
        float sdBlack = sd + offset;
        // Two-sided "border" masks → signed emboss pair (pos/neg) bands
        float border =
            smoothstep(-1.0 + ANTI_ALIASING, max(1.0, borderWidth * ANTI_ALIASING * 0.5), -sd * ANTI_ALIASING) -
            smoothstep(min(-borderWidth * ANTI_ALIASING * 0.5, -1.), 1.0 - ANTI_ALIASING,
                       (-sd - borderWidth) * ANTI_ALIASING);
        float borderBlack =
            smoothstep(-1.0 + ANTI_ALIASING, max(1.0, borderWidth * ANTI_ALIASING * 0.5), -sdBlack * ANTI_ALIASING) -
            smoothstep(min(-borderWidth * ANTI_ALIASING * 0.5, -1.), 1.0 - ANTI_ALIASING,
                       (-sdBlack - borderWidth) * ANTI_ALIASING);
        // ------------------------------- BACKGROUND -------------------------------
        vec4 blurredBgColor = BaseBlur(fragCoord) * bgFactor;
        blurredBgColor.rgb = BlurVibrancy(blurredBgColor.rgb);

        // ------------------------------- INNER SHADOW -----------------------------
        float embossNeg = exp(innerShadowExp * (sdBlack - 1.0 + innerShadowWidth));
        if (embossNeg > 0.0) {
            // Map to blur-buffer pixel grid
            vec2 tileSize = iResolution.xy / max(downSampleFactor, 1e-6);
            vec2 uvInTile = fragCoord / iResolution.xy;
            vec2 pixelDS = uvInTile * (tileSize - 1.0) + 0.5;
            // Inward refraction sample
            vec2 nOut = sdfNormal.xy;
            vec2 deltaInDS = ToDownsamplePx(nOut * innerShadowRefractPx, downSampleFactor);
            vec2 negCoord = pixelDS + deltaInDS;
            vec4 refractionNeg = edgeBlurredImg.eval(negCoord) * bgFactor;
            refractionNeg.rgb = BlurVibrancy(refractionNeg.rgb);
            refractionNeg.rgb = CompareBlend(blurredBgColor.rgb, refractionNeg.rgb);
            refractionNeg.rgb = InnerShadowVibrancy(refractionNeg.rgb);
            blurredBgColor = mix(blurredBgColor, refractionNeg, clamp(embossNeg, 0.0, 1.0));
        }

        // ------------------------------- ENV LIGHT --------------------------------
        float embossPos = (border - borderBlack + 1.0) * 0.5 * clamp(border + borderBlack, 0.0, 1.0);
        if (embossPos > 0.0) {
            // Map to blur-buffer pixel grid
            vec2 tileSize = iResolution.xy / max(downSampleFactor, 1e-6);
            vec2 uvInTile = fragCoord / iResolution.xy;
            vec2 pixelDS = uvInTile * (tileSize - 1.0) + 0.5;
            // Outward refraction sample
            vec2 nOut = sdfNormal.xy;
            vec2 deltaOutDS = ToDownsamplePx(nOut * refractOutPx, downSampleFactor);
            vec2 posCoord = pixelDS + deltaOutDS;
            vec4 refractionPos = edgeBlurredImg.eval(posCoord) * bgFactor;
            refractionPos.rgb = CompareBlend(blurredBgColor.rgb, refractionPos.rgb);
            refractionPos.rgb = EdgeLightVibrancy(refractionPos.rgb);
            blurredBgColor = mix(blurredBgColor, refractionPos, clamp(embossPos, 0.0, 1.0));
        }

        // ------------------------------- HIGHLIGHT --------------------------------
        // Centered space for highlight
        vec2 uv = (fragCoord + fragCoord - iResolution.xy) * 0.5;
        float widthClamped = min(highLightWidthPx, max(borderWidth, 0.0));
        float edgeBand = EdgeBandAA(sd, widthClamped, highLightFeatherPx, highLightShiftPx);
        float diagMask = DiagonalFanMask(uv, normalize(highLightDirection), highLightAngleDeg,
            highLightFeatherDeg);
        float edge = edgeBand * diagMask;
        vec3 hlBase = EdgeHighlightVibrancy(blurredBgColor.rgb);
        blurredBgColor = mix(blurredBgColor, vec4(hlBase, 1.0), edge);

        // ------------------------------- BACKGROUND MIX ---------------------------
        blurredBgColor = mix(image.eval(fragCoord), blurredBgColor, clamp(-min(sd, sdBlack), 0.0, 1.0));
        return blurredBgColor.rgb1;
    }
)";

GEFrostedGlassShaderFilter::GEFrostedGlassShaderFilter(const Drawing::GEFrostedGlassShaderFilterParams& params)
{
    frostedGlassParams_ = params;
}

std::shared_ptr<Drawing::ShaderEffect> GEFrostedGlassShaderFilter::CreateLargeRadiusBlurShader(
    Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image>& image, const Drawing::Rect& src,
    const Drawing::Rect& dst, const Drawing::Matrix& invertMatrix)
{
    auto largeRBlurImg = MakeLargeRadiusBlurImg(canvas, src, dst, image);
    if (largeRBlurImg == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::CreateLargeRadiusBlurShader largeRBlurImg is null");
        return nullptr;
    }

    auto largeRBlurShader = Drawing::ShaderEffect::CreateImageShader(*largeRBlurImg, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);
    if (largeRBlurShader == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::CreateLargeRadiusBlurShader create shader failed");
        return nullptr;
    }

    return largeRBlurShader;
}

std::shared_ptr<Drawing::ShaderEffect> GEFrostedGlassShaderFilter::CreateSmallRadiusBlurShader(
    Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image>& image, const Drawing::Rect& src,
    const Drawing::Rect& dst, const Drawing::Matrix& invertMatrix)
{
    auto smallRBlurImg = MakeSmallRadiusBlurImg(canvas, src, dst, image);
    if (smallRBlurImg == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::CreateSmallRadiusBlurShader smallRBlurImg is null");
        return nullptr;
    }

    auto smallRBlurShader = Drawing::ShaderEffect::CreateImageShader(*smallRBlurImg, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);
    if (smallRBlurShader == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::CreateSmallRadiusBlurShader create shader failed");
        return nullptr;
    }

    return smallRBlurShader;
}


namespace {
template<typename V>
inline V Interpolate(const V& base, const V& dark, float t)
{
    return base * (1.0f - t) + dark * t;
}
} // namespace

namespace FrostedGlass {
void InterpolateAdaptiveParams(Drawing::GEFrostedGlassShaderFilterParams& params)
{
    const auto& t = params.darkScale;
    // Only interpolate adaptive parameters defined in the header:
    // Dark-mode set for FrostedGlass shader filter params.
    params.weightsEmboss = Interpolate(params.weightsEmboss, params.darkModeWeightsEmboss, t);
    params.bgRates = Interpolate(params.bgRates, params.darkModeBgRates, t);
    params.bgKBS = Interpolate(params.bgKBS, params.darkModeBgKBS, t);
    params.bgPos = Interpolate(params.bgPos, params.darkModeBgPos, t);
    params.bgNeg = Interpolate(params.bgNeg, params.darkModeBgNeg, t);
    params.edLightAngles = Interpolate(params.edLightAngles, params.darkModeEdLightAngles, t);
    params.edLightKBS = Interpolate(params.edLightKBS, params.darkModeEdLightKBS, t);
}
} // namespace FrostedGlass

bool GEFrostedGlassShaderFilter::PrepareDrawing(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst,
    Drawing::Matrix& matrix, std::shared_ptr<Drawing::RuntimeShaderBuilder>& builder)
{
    if (image->GetHeight() < 1e-6 || image->GetWidth() < 1e-6) {
        LOGE("GEFrostedGlassShaderFilter::PrepareDrawing imageinfo is invalid");
        return false;
    }
    matrix = canvasInfo_.mat;
    matrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix invertMatrix;
    if (!matrix.Invert(invertMatrix)) {
        LOGE("GEFrostedGlassShaderFilter::PrepareDrawing Invert matrix failed");
        return false;
    }
    auto shader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);
    if (shader == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::create shader failed.");
        return false;
    }
    auto largeRBlurShader = CreateLargeRadiusBlurShader(canvas, image, src, dst, invertMatrix);
    auto smallRBlurShader = CreateSmallRadiusBlurShader(canvas, image, src, dst, invertMatrix);
    if (smallRBlurShader == nullptr || largeRBlurShader == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::create largeRBlurShader or smallRBlurShader failed.");
        return false;
    }

    auto sdfNormalShader = MakeSDFNormalShader(canvas, canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    if (!sdfNormalShader) {
        LOGE("GEFrostedGlassShaderFilter::OnProcessImage sdfShapeShader is null");
        return false;
    }

    builder = MakeFrostedGlassShader(shader, largeRBlurShader, smallRBlurShader, sdfNormalShader,
                                     canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    return true;
}

std::shared_ptr<Drawing::Image> GEFrostedGlassShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::OnProcessImage input is invalid");
        return nullptr;
    }
    Drawing::Matrix matrix;
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder;
    if (!PrepareDrawing(canvas, image, src, dst, matrix, builder)) {
        return image;
    }
    if (builder == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::OnProcessImage builder is null");
        return image;
    }
    auto resultImage = builder->MakeImage(canvas.GetGPUContext().get(), &matrix, image->GetImageInfo(), false);
    if (resultImage == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::OnProcessImage resultImage is null");
        return image;
    }
    return resultImage;
}

bool GEFrostedGlassShaderFilter::OnDrawImage(Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image> image,
    const Drawing::Rect& src, const Drawing::Rect& dst, Drawing::Brush& brush)
{
    if (image == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::OnDrawImage input is invalid");
        return false;
    }
    Drawing::Matrix matrix;
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder;
    if (!PrepareDrawing(canvas, image, src, dst, matrix, builder)) {
        return false;
    }
    if (builder == nullptr) {
        LOGE("GEFrostedGlassShaderFilter::OnDrawImage builder is null");
        return false;
    }

    matrix.PostConcat(CreateDestinationTranslateMatrix(dst));
    auto shader = builder->MakeShader(&matrix, image->IsOpaque());
    brush.SetShaderEffect(shader);
    canvas.AttachBrush(brush);
    canvas.DrawRect(dst);
    canvas.DetachBrush();
    return true;
}

std::shared_ptr<Drawing::Image> GEFrostedGlassShaderFilter::MakeLargeRadiusBlurImg(Drawing::Canvas& canvas,
    const Drawing::Rect& src, const Drawing::Rect& dst, std::shared_ptr<Drawing::Image> image)
{
    Drawing::GEMESABlurShaderFilterParams blurImgParas{};
    blurImgParas.radius = frostedGlassParams_.blurParams[0]; // Radius
    auto blurShader_ = std::make_shared<GEMESABlurShaderFilter>(blurImgParas);
    return blurShader_->OnProcessImage(canvas, image, src, dst);
}

std::shared_ptr<Drawing::Image> GEFrostedGlassShaderFilter::MakeSmallRadiusBlurImg(Drawing::Canvas& canvas,
    const Drawing::Rect& src, const Drawing::Rect& dst, std::shared_ptr<Drawing::Image> image)
{
    const float EPS = 1e-12;
    Drawing::GEMESABlurShaderFilterParams blurImgParas{};
    blurImgParas.radius = frostedGlassParams_.blurParams[0] / std::max(frostedGlassParams_.blurParams[1], EPS);
    auto blurShader_ = std::make_shared<GEMESABlurShaderFilter>(blurImgParas);
    return blurShader_->OnProcessImage(canvas, image, src, dst);
}

std::shared_ptr<Drawing::ShaderEffect> GEFrostedGlassShaderFilter::MakeSDFNormalShader(Drawing::Canvas& canvas,
    float width, float height) const
{
    if (auto shape = frostedGlassParams_.sdfShape) {
        return shape->GenerateDrawingShaderHasNormal(canvas, width, height);
    }
    return nullptr;
}

thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_frostedGlassShaderEffect = nullptr;

bool GEFrostedGlassShaderFilter::InitFrostedGlassEffect()
{
    if (g_frostedGlassShaderEffect == nullptr) {
        g_frostedGlassShaderEffect = Drawing::RuntimeEffect::CreateForShader(MAIN_SHADER_PROG);
        if (g_frostedGlassShaderEffect == nullptr) {
            LOGE("InitFrostedGlassEffect::RuntimeShader effect error\n");
            return false;
        }
    }
    return true;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEFrostedGlassShaderFilter::MakeFrostedGlassShader(
    std::shared_ptr<Drawing::ShaderEffect> imageShader, std::shared_ptr<Drawing::ShaderEffect> largeRBlurShader,
    std::shared_ptr<Drawing::ShaderEffect> smallRBlurShader, std::shared_ptr<Drawing::ShaderEffect> sdfNormalShader,
    float imageWidth, float imageHeight)
{
    if (g_frostedGlassShaderEffect == nullptr) {
        if (!InitFrostedGlassEffect()) {
            LOGE("GEFrostedGlassShaderFilter::failed when initializing MagnifierEffect.");
            return nullptr;
        }
    }

    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(g_frostedGlassShaderEffect);
    // Common inputs
    builder->SetChild("image", imageShader);
    builder->SetChild("edgeBlurredImg", largeRBlurShader);
    builder->SetChild("bgBlurredImg", smallRBlurShader);
    builder->SetUniform("iResolution", imageWidth, imageHeight);
    builder->SetChild("sdfNormalImg", sdfNormalShader);
    builder->SetUniform("borderWidth", frostedGlassParams_.envLightParams[NUM_0]);
    builder->SetUniform("offset", 0.0f);
    builder->SetUniform("downSampleFactor", 1.0f);
    // Background darken parameter
    builder->SetUniform("bgFactor", 1.0f);
    // Inner shadow parameters
    builder->SetUniform("innerShadowRefractPx", frostedGlassParams_.sdParams[NUM_0]);
    builder->SetUniform("innerShadowWidth", frostedGlassParams_.sdParams[NUM_1]);
    builder->SetUniform("innerShadowExp", 4.62f);
    builder->SetUniform("sdK", frostedGlassParams_.sdKBS[NUM_0]);
    builder->SetUniform("sdB", frostedGlassParams_.sdKBS[NUM_1]);
    builder->SetUniform("sdS", frostedGlassParams_.sdKBS[NUM_2]);
    // Env refraction parameters
    builder->SetUniform("refractOutPx", frostedGlassParams_.envLightParams[NUM_0]);
    builder->SetUniform("envK", frostedGlassParams_.envLightKBS[NUM_0]);
    builder->SetUniform("envB", frostedGlassParams_.envLightKBS[NUM_1]);
    builder->SetUniform("envS", frostedGlassParams_.envLightKBS[NUM_2]);
    // Edge highlights parameters
    builder->SetUniform("highLightAngleDeg", frostedGlassParams_.edLightAngles[NUM_0]);
    builder->SetUniform("highLightFeatherDeg", frostedGlassParams_.edLightAngles[NUM_1]);
    builder->SetUniform("highLightWidthPx", frostedGlassParams_.edLightParams[NUM_0]);
    builder->SetUniform("highLightFeatherPx", frostedGlassParams_.edLightParams[NUM_1]);
    builder->SetUniform("highLightShiftPx", 0.0f);
    builder->SetUniform("highLightDirection", frostedGlassParams_.edLightDir[NUM_0],
                        frostedGlassParams_.edLightDir[NUM_1]);
    builder->SetUniform("hlK", frostedGlassParams_.edLightKBS[NUM_0]);
    builder->SetUniform("hlB", frostedGlassParams_.edLightKBS[NUM_1]);
    builder->SetUniform("hlS", frostedGlassParams_.edLightKBS[NUM_2]);
    return builder;
}
} // namespace Rosen
} // namespace OHOS