/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "ge_magnifier_shader_filter.h"
#include "ge_system_properties.h"

#include "ge_log.h"

namespace OHOS {
namespace Rosen {

namespace {
constexpr static uint8_t COLOR_CHANNEL = 4; // 4 len of rgba
static constexpr char MAGNIFIER_SHADER_WITH_SDF_PROG[] = R"(
    uniform shader imageShader;
    uniform shader sdfShader;
    uniform half2 iResolution;

    uniform half factor;
    uniform half2 zoomOffset;
    uniform half borderSize;
    uniform half4 borderColor;
    uniform half shadowSize;
    uniform half shadowStrength;

    const half2 boxPos = half2(0.5, 0.5);
    const half3 incident = half3(0.0, 0.0, -1.0);
    const half aa = 1.0;
    const half dispScale = 0.5;
    const half dispersion = 0.15;
    const half index = 1.5;
    const half thickness = 8.0;
    const half baseHeight = 64.0;

    // 常量预计算（编译期计算，无运行时开销）
    const half thicknessSq = thickness * thickness;
    const half invIndex = 1.0 / index;
    const half invIndexSubDisp = 1.0 / (index - dispersion);
    const half invIndexAddDisp = 1.0 / (index + dispersion);
    const half minNdotV = 0.05;

    half height(half sd)
    {
        if (sd >= 0.0) return 0.0;
        if (sd < -thickness) return thickness;
        half x = thickness + sd;
        return sqrt(thicknessSq - x * x); // 用预计算平方
    }

    half4 bg(half2 uv)
    {
        return imageShader.eval(uv * iResolution);
    }

    half4 getRefractColor(half2 fragCoord, half3 normal, half h)
    {
        half2 res = iResolution.xy;
        half3 refr = refract(incident, normal, invIndex); // 预计算除法
        half ndotv = max(dot(incident, refr), minNdotV);
        half refractLen = (h + baseHeight) / ndotv;

        half2 uv = (fragCoord + refr.xy * refractLen) / res;
        uv = (uv - boxPos + zoomOffset) / factor + boxPos;
        return bg(uv);
    }

    half4 getDispColor(half2 fragCoord, half3 normal, half refractLen)
    {
        half2 res = iResolution.xy;

        half3 refrR = refract(incident, normal, invIndexSubDisp);
        half3 refrB = refract(incident, normal, invIndexAddDisp);

        half2 uvR = (fragCoord + refrR.xy*refractLen)/res;
        half2 uvB = (fragCoord + refrB.xy*refractLen)/res;

        uvR = (uvR - boxPos + zoomOffset)/factor + boxPos;
        uvB = (uvB - boxPos + zoomOffset)/factor + boxPos;

        return half4(bg(uvR).r, 0.0, bg(uvB).b, 1.0);
    }

    half4 main(float2 fragCoord)
    {
        half4 sdfResult = sdfShader.eval(fragCoord);
        half rawDist = sdfResult.w;
        half sd = rawDist * dispScale;

        // 1. 阴影
        half shadow = 1.0 - smoothstep(0.0, shadowSize, rawDist);
        half4 fragColor = half4(0.0, 0.0, 0.0, shadow * shadowStrength);

        // 2. 只在内部计算
        if (sd < 0.0)
        {
            if (sd < -thickness)
            {
                half2 uv = fragCoord / iResolution.xy;
                half2 zoomUV = (uv - boxPos + zoomOffset) / factor + boxPos;
                return bg(zoomUV);
            }
            half shapeMask = 1.0 - smoothstep(-aa, aa, sd);
            half2 preNormal = normalize(sdfResult.xy);
            half nc = clamp((thickness + sd) / thickness, 0.0, 1.0);
            half3 normal = normalize(half3(preNormal * nc, sqrt(1.0 - nc * nc)));

            half h = height(sd);
            fragColor = getRefractColor(fragCoord, normal, h);

            // 色散只在边缘计算
            half edgeFactor = smoothstep(-15.0, 0.0, sd);
            if (edgeFactor > 0.001)
            {
                half3 refr = refract(incident, normal, invIndex);
                half ndotv = max(dot(incident, refr), minNdotV);
                half refractLen = (h + baseHeight) / ndotv;
                half4 dispColor = getDispColor(fragCoord, normal, refractLen);
                dispColor.g = fragColor.g;
                fragColor = mix(fragColor, dispColor, edgeFactor);
            }

            fragColor *= shapeMask;
        }

        // 3. 边框
        half border = smoothstep(-borderSize, -borderSize + aa, rawDist) * smoothstep(aa, 0.0, rawDist);
        fragColor = mix(fragColor, half4(borderColor.rgb, 1.0), border * borderColor.a);

        return fragColor;
    }
)";
} // namespace

std::shared_ptr<Drawing::RuntimeEffect> GEMagnifierShaderFilter::g_magnifierShaderEffectWithSDF = nullptr;

GEMagnifierShaderFilter::GEMagnifierShaderFilter(const Drawing::GEMagnifierShaderFilterParams& params)
{
    magnifierPara_ = std::make_shared<GEMagnifierParams>();
    if (!magnifierPara_) {
        return;
    }
    magnifierPara_->factor_ = params.factor;
    magnifierPara_->width_ = params.width;
    magnifierPara_->height_ = params.height;
    magnifierPara_->cornerRadius_ = params.cornerRadius;
    magnifierPara_->borderWidth_ = params.borderWidth;
    magnifierPara_->zoomOffsetX_ = params.zoomOffsetX;
    magnifierPara_->zoomOffsetY_ = params.zoomOffsetY;
    magnifierPara_->shadowOffsetX_ = params.shadowOffsetX;
    magnifierPara_->shadowOffsetY_ = params.shadowOffsetY;
    magnifierPara_->shadowSize_ = params.shadowSize;
    magnifierPara_->shadowStrength_ = params.shadowStrength;
    magnifierPara_->gradientMaskColor1_ = params.gradientMaskColor1;
    magnifierPara_->gradientMaskColor2_ = params.gradientMaskColor2;
    magnifierPara_->outerContourColor1_ = params.outerContourColor1;
    magnifierPara_->outerContourColor2_ = params.outerContourColor2;
    magnifierPara_->rotateDegree_ = params.rotateDegree;
    sdfShape_ = params.sdfShape;
}

std::shared_ptr<Drawing::Image> GEMagnifierShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEMagnifierShaderFilter::OnProcessImage image is null");
        return image;
    }

    float imageWidth = image->GetWidth();
    float imageHeight = image->GetHeight();
    if (!ValidateMagnifierParams(imageWidth, imageHeight)) {
        LOGE("GEMagnifierShaderFilter::OnProcessImage validate params failed");
        return image;
    }

    Drawing::Matrix matrix = canvasInfo_.mat;
    matrix.PostTranslate(-canvasInfo_.materialDst.GetLeft(), -canvasInfo_.materialDst.GetTop());
    Drawing::Matrix invertMatrix;

    if (!matrix.Invert(invertMatrix)) {
        LOGE("GEColorGradientShaderFilter::OnProcessImage Invert matrix failed");
        return image;
    }

    auto imageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);

    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder;
    builder = MakeMagnifierShaderWithSDFShape(imageShader, imageWidth, imageHeight);
    if (builder == nullptr) {
        LOGE("GEMagnifierShaderFilter::OnProcessImage builder is null");
        return image;
    }

#ifdef RS_ENABLE_GPU
    auto resultImage = builder->MakeImage(canvas.GetGPUContext().get(), &matrix, image->GetImageInfo(), false);
#else
    auto resultImage = builder->MakeImage(nullptr, &matrix, image->GetImageInfo(), false);
#endif
    if (resultImage == nullptr) {
        LOGE("GEMagnifierShaderFilter::OnProcessImage resultImage is null");
        return image;
    }

    return resultImage;
}

bool GEMagnifierShaderFilter::ValidateMagnifierParams(float imageWidth, float imageHeight) const
{
    if (GE_LE(imageHeight, 0.0f) || GE_LE(imageWidth, 0.0f)) {
        LOGE("GEMagnifierShaderFilter::ValidateMagnifierParams invalid image size: width=%{public}f, height=%{public}f",
            imageWidth, imageHeight);
        return false;
    }
    if (magnifierPara_ == nullptr) {
        LOGE("GEMagnifierShaderFilter::ValidateMagnifierParams magnifierPara is null");
        return false;
    }
    if (GE_LE(magnifierPara_->factor_, 0.0f)) {
        LOGE("GEMagnifierShaderFilter::ValidateMagnifierParams invalid factor: %{public}f", magnifierPara_->factor_);
        return false;
    }
    if (sdfShape_ == nullptr) {
        LOGE("GEMagnifierShaderFilter::ValidateMagnifierParams sdfShape is null");
        return false;
    }
    return true;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEMagnifierShaderFilter::MakeMagnifierShaderWithSDFShape(
    std::shared_ptr<Drawing::ShaderEffect> imageShader, float imageWidth, float imageHeight)
{
    if (imageShader == nullptr) {
        LOGE("GEMagnifierShaderFilter::MakeMagnifierShaderWithSDFShape imageShader is null");
        return nullptr;
    }

    if (g_magnifierShaderEffectWithSDF == nullptr) {
        g_magnifierShaderEffectWithSDF = Drawing::RuntimeEffect::CreateForShader(MAGNIFIER_SHADER_WITH_SDF_PROG);
        if (g_magnifierShaderEffectWithSDF == nullptr) {
            LOGE("GEMagnifierShaderFilter::MakeMagnifierShaderWithSDFShape failed to create RuntimeEffect");
            return nullptr;
        }
    }

    auto sdfShader = sdfShape_->GenerateDrawingShaderHasNormal(imageWidth, imageHeight);
    if (sdfShader == nullptr) {
        LOGE("GEMagnifierShaderFilter::MakeMagnifierShaderWithSDFShape failed to generate SDF shader, "
            "imageWidth=%{public}f, imageHeight=%{public}f", imageWidth, imageHeight);
        return nullptr;
    }

    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(g_magnifierShaderEffectWithSDF);

    builder->SetChild("imageShader", imageShader);
    builder->SetChild("sdfShader", sdfShader);
    builder->SetUniform("iResolution", canvasInfo_.geoWidth, canvasInfo_.geoHeight);

    builder->SetUniform("factor", magnifierPara_->factor_);
    builder->SetUniform("zoomOffset", magnifierPara_->zoomOffsetX_ / (imageWidth > 0.0f ? imageWidth : 1.0f),
        magnifierPara_->zoomOffsetY_ / (imageHeight > 0.0f ? imageHeight : 1.0f));
    builder->SetUniform("borderSize", magnifierPara_->borderWidth_);

    float borderColor[COLOR_CHANNEL] = {0.0f};
    ConvertToRgba(magnifierPara_->outerContourColor1_, borderColor, COLOR_CHANNEL);
    builder->SetUniform("borderColor", borderColor, COLOR_CHANNEL);

    builder->SetUniform("shadowSize", magnifierPara_->shadowSize_);
    builder->SetUniform("shadowStrength", magnifierPara_->shadowStrength_);

    return builder;
}

void GEMagnifierShaderFilter::ConvertToRgba(uint32_t rgba, float* color, int tupleSize)
{
    if (!color || tupleSize < 4) { // 4 len of rgba
        return;
    }
    int16_t alpha = static_cast<int16_t>(rgba & 0xFF);               // 0xff byte mask
    int16_t red = static_cast<int16_t>((rgba & 0xFF000000) >> 24);   // 0xff000000 red mask, 24 red shift
    int16_t green = static_cast<int16_t>((rgba & 0x00FF0000) >> 16); // 0x00ff0000 green mask, 16 green shift
    int16_t blue = static_cast<int16_t>((rgba & 0x0000FF00) >> 8);   // 0x0000ff00 blue mask, 8 blue shift

    color[0] = red * 1.0f / 255.0f;     // 255.0f is the max value, 0 red
    color[1] = green * 1.0f / 255.0f;   // 255.0f is the max value, 1 green
    color[2] = blue * 1.0f / 255.0f;    // 255.0f is the max value, 2 blue
    color[3] = alpha * 1.0f / 255.0f;   // 255.0f is the max value, 3 alpha
}

const std::string GEMagnifierShaderFilter::GetDescription() const
{
    return "GEMagnifierShaderFilter";
}

} // namespace Rosen
} // namespace OHOS
