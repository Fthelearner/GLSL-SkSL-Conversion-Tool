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

#include "ge_sdf_edge_light.h"

#include <unordered_map>

#include "ge_log.h"
#include "ge_mesa_blur_shader_filter.h"
#include "ge_ripple_shader_mask.h"

namespace OHOS::Rosen {
namespace {
constexpr float DEFAULT_RADIUS = 3.0f;

std::shared_ptr<Drawing::Image> GenerateSDFFromShape(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::GESDFShaderShape>& sdfShape, float imageWidth, float imageHeight,
    const Drawing::ImageInfo& imageInfo)
{
    if (!sdfShape) {
        LOGE("GESDFEdgeLight::OnProcessImage sdfShape_ is null");
        return nullptr;
    }
    auto shader = sdfShape->GenerateDrawingShader(canvas, imageWidth, imageHeight);
    if (!shader) {
        LOGE("GESDFEdgeLight::OnProcessImage GenerateDrawingShader failed");
        return nullptr;
    }

    constexpr char passThrough[] = R"(
        uniform shader inputShader;
        vec4 main(vec2 fragCoord) {
            return (inputShader.eval(fragCoord) + 63.5) / 127.5;
        }
    )";
    static auto passThroughEffect = Drawing::RuntimeEffect::CreateForShader(passThrough);

    Drawing::RuntimeShaderBuilder builder(passThroughEffect);
    builder.SetChild("inputShader", shader);

    // Use f16 format to avoid quantization issues with [-64, 64] range
    Drawing::ImageInfo f16ImageInfo = imageInfo;
    f16ImageInfo.SetColorType(Drawing::ColorType::COLORTYPE_RGBA_F16);
    return builder.MakeImage(canvas.GetGPUContext().get(), nullptr, f16ImageInfo, false);
}

constexpr char SHADER[] = R"(
    uniform vec2 iResolution;

    uniform shader sdfImageShader;
    uniform shader blurredSdfImageShader;
    uniform shader lightMaskShader;

    uniform float spreadFactor;

    uniform vec3 lightColor;
    uniform float bloomIntensityCutoff; // at what intensity is bloom supposed to disappear
    uniform float maxIntensity;
    uniform float maxBloomIntensity;
    uniform float bloomFalloffPow;

    uniform float minBorderWidth;
    uniform float maxBorderWidth;

    uniform float innerBorderBloomWidth;
    uniform float outerBorderBloomWidth;

    float bloomMultiplierFromDist(float d) {
        float dNorm = abs(d) / (d > 0 ? outerBorderBloomWidth : innerBorderBloomWidth);
        float falloff = max((1 - dNorm) / pow(1 + dNorm, bloomFalloffPow), 0);
        return maxBloomIntensity * falloff;
    }

    float edgeLightFakeBloom(float intensity, float d, float smoothD) {
        float bloomBorder = 1 - step(outerBorderBloomWidth, d);
        bloomBorder *= step(-innerBorderBloomWidth, d);

        float edgeThickness = smoothstep(0, 1, intensity) *
                              (maxBorderWidth - minBorderWidth) + minBorderWidth;
        float thinBorder = (1 - smoothstep(0, edgeThickness, d)) * smoothstep(-edgeThickness, 0, d);

        float b = intensity * maxIntensity * (thinBorder + smoothstep(bloomIntensityCutoff, 1, intensity) *
                  bloomBorder * bloomMultiplierFromDist(smoothD));
        return b;
    }

    vec4 decodeSdfMap(vec4 inputSdfMap) {
        return vec4(inputSdfMap * 2.0 - vec4(1.0)) * spreadFactor;
    }

    half4 main(vec2 fragCoord)
    {
        float lightMaskValue = lightMaskShader.eval(fragCoord).r;

        vec4 sdfMapSample = decodeSdfMap(sdfImageShader.eval(fragCoord));
        vec4 blurredSdfMapSample = decodeSdfMap(blurredSdfImageShader.eval(fragCoord));

        half3 rgb = lightColor * edgeLightFakeBloom(lightMaskValue, sdfMapSample.a, blurredSdfMapSample.a);
        return half4(rgb, rgb.r);
    }
)";
} // namespace

GESDFEdgeLight::GESDFEdgeLight(const Drawing::GESDFEdgeLightFilterParams& params)
    : sdfSpreadFactor_(params.sdfSpreadFactor), bloomIntensityCutoff_(params.bloomIntensityCutoff),
      maxIntensity_(params.maxIntensity), maxBloomIntensity_(params.maxBloomIntensity),
      bloomFalloffPow_(params.bloomFalloffPow), minBorderWidth_(params.minBorderWidth),
      maxBorderWidth_(params.maxBorderWidth), innerBorderBloomWidth_(params.innerBorderBloomWidth),
      outerBorderBloomWidth_(params.outerBorderBloomWidth), sdfImage_(params.sdfImage), sdfShape_(params.sdfShape),
      lightMask_(params.lightMask)
{}

std::shared_ptr<Drawing::Image> GESDFEdgeLight::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (!image) {
        LOGE("GESDFEdgeLight::OnProcessImage image is null");
        return image;
    }
    if (!lightMask_) {
        LOGE("GESDFEdgeLight::OnProcessImage lightMask_ is null");
        return image;
    }

    const float imageWidth = image->GetWidth();
    const float imageHeight = image->GetHeight();
    // Priority: sdfImage_ > sdfShape_
    // Generate SDF image from shape if needed
    if (!sdfImage_) {
        sdfImage_ = GenerateSDFFromShape(canvas, sdfShape_, imageWidth, imageHeight, image->GetImageInfo());
    }
    if (!blurredSdfImage_) {
        blurredSdfImage_ = BlurSdfMap(canvas, sdfImage_, DEFAULT_RADIUS);
    }
    auto builder = MakeEffectShader(imageWidth, imageHeight);
    if (!builder) {
        LOGE("GESDFEdgeLight::OnProcessImage builder is null");
        return image;
    }

    auto resultImage = builder->MakeImage(canvas.GetGPUContext().get(), nullptr, image->GetImageInfo(), false);
    if (!resultImage) {
        LOGE("GESDFEdgeLight::OnProcessImage resultImage is null");
        return image;
    }

    return MergeImage(canvas, image, resultImage);
}

std::shared_ptr<Drawing::Image> GESDFEdgeLight::MergeImage(
    Drawing::Canvas& canvas, std::shared_ptr<Drawing::Image> image, std::shared_ptr<Drawing::Image> compositeImage)
{
    auto builder = MakeImageMerger(*image, *compositeImage);
    if (builder == nullptr) {
        LOGE("GESDFEdgeLight::MergeImage builder is null");
        return image;
    }
    return builder->MakeImage(canvas.GetGPUContext().get(), nullptr, image->GetImageInfo(), false);
}

std::shared_ptr<Drawing::Image> GESDFEdgeLight::BlurSdfMap(
    Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image> sdfImage, float radius)
{
    if (!sdfImage) {
        LOGE("GESDFEdgeLight::BlurSdfMap input is invalid");
        return nullptr;
    }

    Drawing::Rect imageRect = { 0, 0, sdfImage->GetWidth(), sdfImage->GetHeight() };

    Drawing::GEMESABlurShaderFilterParams params;
    params.radius = radius;
    GEMESABlurShaderFilter blurFilter(params);

    auto blurImage = blurFilter.OnProcessImage(canvas, sdfImage, imageRect, imageRect);
    if (!blurImage) {
        LOGE("GESDFEdgeLight::BlurSdfMap blur error");
        return sdfImage;
    }
    return blurImage;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFEdgeLight::MakeImageMerger(
    Drawing::Image& image, Drawing::Image& composeImage)
{
    constexpr char shadeCode[] = R"(
        uniform shader image;
        uniform shader composeImage;
        vec4 main(vec2 fragCoord) {
            vec4 imageColor = image.eval(fragCoord);
            vec4 composeImageColor = composeImage.eval(fragCoord);
            return vec4(imageColor.rgb + composeImageColor.rgb, imageColor.a);
        }
    )";
    static auto effectShader = Drawing::RuntimeEffect::CreateForShader(shadeCode);
    if (!effectShader) {
        return nullptr;
    }

    auto builder = std::make_shared<Drawing::RuntimeShaderBuilder>(effectShader);

    const Drawing::Matrix matrix;
    const Drawing::SamplingOptions sampling(Drawing::FilterMode::LINEAR);

    builder->SetChild("image", Drawing::ShaderEffect::CreateImageShader(
        image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, sampling, matrix));
    builder->SetChild("composeImage", Drawing::ShaderEffect::CreateImageShader(composeImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, sampling, matrix));

    return builder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFEdgeLight::MakeEffectShader(float imageWidth, float imageHeight)
{
    static auto effectShader = Drawing::RuntimeEffect::CreateForShader(SHADER);
    if (!effectShader) {
        LOGE("MakeEffectShader::RuntimeShader effect error\n");
        return nullptr;
    }

    if (!sdfImage_ || !blurredSdfImage_ || !lightMask_) {
        LOGE("GESDFEdgeLight::MakeEffectShader input is invalid");
        return nullptr;
    }

    const Drawing::Matrix matrix;
    const Drawing::SamplingOptions sampling(Drawing::FilterMode::LINEAR);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(effectShader);

    builder->SetUniform("iResolution", imageWidth, imageHeight);

    builder->SetChild("sdfImageShader", Drawing::ShaderEffect::CreateImageShader(*sdfImage_, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, sampling, matrix));

    builder->SetChild("blurredSdfImageShader", Drawing::ShaderEffect::CreateImageShader(
        *blurredSdfImage_, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, sampling, matrix));
    auto lightMaskShader = lightMask_->GenerateDrawingShader(imageWidth, imageHeight);
    builder->SetChild("lightMaskShader", lightMaskShader);

    builder->SetUniform("spreadFactor", sdfSpreadFactor_);

    builder->SetUniform("lightColor", 1.0f, 1.0f, 1.0f);
    builder->SetUniform("bloomIntensityCutoff", bloomIntensityCutoff_);
    builder->SetUniform("maxIntensity", maxIntensity_);
    builder->SetUniform("maxBloomIntensity", maxBloomIntensity_);
    builder->SetUniform("bloomFalloffPow", bloomFalloffPow_);

    builder->SetUniform("minBorderWidth", minBorderWidth_);
    builder->SetUniform("maxBorderWidth", maxBorderWidth_);

    builder->SetUniform("innerBorderBloomWidth", innerBorderBloomWidth_);
    builder->SetUniform("outerBorderBloomWidth", outerBorderBloomWidth_);

    return builder;
}
} // namespace OHOS::Rosen
