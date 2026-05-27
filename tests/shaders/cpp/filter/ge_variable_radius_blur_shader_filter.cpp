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
#include "ge_variable_radius_blur_shader_filter.h"

#include "ge_log.h"
#include "ge_system_properties.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {

namespace {
thread_local static std::shared_ptr<Drawing::RuntimeEffect> horizontalBoxBlurShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> verticalBoxBlurShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> textureShaderEffect_ = nullptr;
} // namespace

GEVariableRadiusBlurShaderFilter::GEVariableRadiusBlurShaderFilter(
    const Drawing::GEVariableRadiusBlurShaderFilterParams &params)
    : params_(params)
{}

std::shared_ptr<Drawing::Image> GEVariableRadiusBlurShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (!image || params_.blurRadius <= 0 || !params_.mask) {
        return image;
    }
    LOGD("GEVariableRadiusBlurShaderFilter::OnProcessImage radius: %{public}f", params_.blurRadius);
    auto imageInfo = image->GetImageInfo();
    auto maskShader = params_.mask->GenerateDrawingShader(imageInfo.GetWidth(), imageInfo.GetHeight());
    if (maskShader == nullptr) {
        LOGE("GEVariableRadiusBlurShaderFilter::OnProcessImage mask generate failed");
        return image;
    }
    float radius = std::clamp(params_.blurRadius, 0.0f, 60.0f); // 60.0 represents largest blur radius
    radius = radius / 2; // 2 half blur radius
    MakeHorizontalBoxBlurEffect(params_.applyInsideMask);
    MakeVerticalBoxBlurEffect(params_.applyInsideMask);
    MakeTextureShaderEffect();
    return DrawBoxLinearGradientBlur(image, canvas, radius, maskShader, dst);
}

void GEVariableRadiusBlurShaderFilter::MakeTextureShaderEffect()
{
    if (textureShaderEffect_ != nullptr) {
        return;
    }
    static const std::string generateTextureShader(R"(
        uniform shader imageInput;
        half4 main(float2 xy) {
            return imageInput.eval(xy);
        }
    )");
    textureShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(generateTextureShader);
    if (textureShaderEffect_ == nullptr) {
        LOGE("GEVariableRadiusBlurShaderFilter::RuntimeShader textureShaderEffect create failed");
    }
}

void GEVariableRadiusBlurShaderFilter::MakeHorizontalBoxBlurEffect(bool applyInsideMask)
{
    if (horizontalBoxBlurShaderEffect_ != nullptr) {
        return;
    }
    static const std::string HorizontalBlurString(
        R"(
        uniform half r;
        uniform shader imageShader;
        uniform shader gradientShader;
        half4 boxFilter(float2 coord, half radius)
        {
            half4 sum = vec4(0.0);
            half div = 0;
            for (half x = -30.0; x < 30.0; x += 1.0) {
                if (x > radius) {
                    break;
                }
                if (abs(x) < radius) {
                    div += 1;
                    sum += imageShader.eval(coord + float2(x, 0));
                }
            }
            return half4(sum.xyz / div, 1.0);
        }
        half4 main(float2 coord)
        {
            float radius = r * gradientShader.eval(coord).a;
            if (radius < 1.0) {
                return imageShader.eval(coord);
            }
            radius = clamp(radius, 1.0, r);
            return boxFilter(coord, radius);
        }
    )");
    static const std::string HorizontalBlurMaskedString(
        R"(
        uniform half r;
        uniform shader imageShader;
        uniform shader gradientShader;
        half4 boxFilter(float2 coord, half radius)
        {
            half4 sum = vec4(0.0);
            half div = 0;
            for (half x = -30.0; x < 30.0; x += 1.0) {
                if (x > radius) {
                    break;
                }
                if (abs(x) < radius) {
                    float res = gradientShader.eval(coord + float2(x, 0)).a;
                    if (res > 0.0)
                    {
                        div += 1;
                        sum += imageShader.eval(coord + float2(x, 0));
                    }
                }
            }
            return half4(sum.xyz / div, 1.0);
        }
        half4 main(float2 coord)
        {
            float radius = r * gradientShader.eval(coord).a;
            if (radius < 1.0) {
                return imageShader.eval(coord);
            }
            radius = clamp(radius, 1.0, r);
            return boxFilter(coord, radius);
        }
    )");
    horizontalBoxBlurShaderEffect_ = applyInsideMask ?
        Drawing::RuntimeEffect::CreateForShader(HorizontalBlurMaskedString) :
        Drawing::RuntimeEffect::CreateForShader(HorizontalBlurString);
    if (horizontalBoxBlurShaderEffect_ == nullptr) {
        LOGE("GEVariableRadiusBlurShaderFilter::RuntimeShader horizontalBoxBlurShaderEffect create failed");
    }
}

void GEVariableRadiusBlurShaderFilter::MakeVerticalBoxBlurEffect(bool applyInsideMask)
{
    if (verticalBoxBlurShaderEffect_ != nullptr) {
        return;
    }
    static const std::string VerticalBlurString(
        R"(
        uniform half r;
        uniform shader imageShader;
        uniform shader gradientShader;
        half4 boxFilter(float2 coord, half radius)
        {
            half4 sum = vec4(0.0);
            half div = 0;
            for (half y = -30.0; y < 30.0; y += 1.0) {
                if (y > radius) {
                    break;
                }
                if (abs(y) < radius) {
                    div += 1;
                    sum += imageShader.eval(coord + float2(0, y));
                }
            }
            return half4(sum.xyz / div, 1.0);
        }
        half4 main(float2 coord)
        {
            float radius = r * gradientShader.eval(coord).a;
            if (radius < 1.0) {
                return imageShader.eval(coord);
            }
            radius = clamp(radius, 1.0, r);
            return boxFilter(coord, radius);
        }
    )");
    static const std::string VerticalBlurMaskedString(
        R"(
        uniform half r;
        uniform shader imageShader;
        uniform shader gradientShader;
        half4 boxFilter(float2 coord, half radius)
        {
            half4 sum = vec4(0.0);
            half div = 0;
            for (half y = -30.0; y < 30.0; y += 1.0) {
                if (y > radius) {
                    break;
                }
                if (abs(y) < radius) {
                    float res = gradientShader.eval(coord + float2(0, y)).a;
                    if (res > 0.0)
                    {
                        div += 1;
                        sum += imageShader.eval(coord + float2(0, y));
                    }
                }
            }
            return half4(sum.xyz / div, 1.0);
        }
        half4 main(float2 coord)
        {
            float radius = r * gradientShader.eval(coord).a;
            if (radius < 1.0) {
                return imageShader.eval(coord);
            }
            radius = clamp(radius, 1.0, r);
            return boxFilter(coord, radius);
        }
    )");    
    verticalBoxBlurShaderEffect_ = applyInsideMask ?
        Drawing::RuntimeEffect::CreateForShader(VerticalBlurMaskedString) :
        Drawing::RuntimeEffect::CreateForShader(VerticalBlurString);
    if (verticalBoxBlurShaderEffect_ == nullptr) {
        LOGE("GEVariableRadiusBlurShaderFilter::RuntimeShader verticalBoxBlurShaderEffect create failed");
    }
}

std::shared_ptr<Drawing::Image> GEVariableRadiusBlurShaderFilter::DrawBoxLinearGradientBlur(
    const std::shared_ptr<Drawing::Image>& image, Drawing::Canvas& canvas, float radius,
    std::shared_ptr<Drawing::ShaderEffect> alphaGradientShader, const Drawing::Rect& dst)
{
    GE_TRACE_NAME_FMT("GEVariableRadiusBlurShaderFilter::DrawBoxLinearGradientBlur, Type: %s, radius: %g "\
        "Width: %g, Height: %g", Drawing::GE_FILTER_VARIABLE_RADIUS_BLUR, radius, dst.GetWidth(), dst.GetHeight());
    if (!horizontalBoxBlurShaderEffect_ || !verticalBoxBlurShaderEffect_ || !image) {
        return image;
    }
    Drawing::Matrix blurMatrix;
    blurMatrix.PostTranslate(dst.GetLeft(), dst.GetTop());
    Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    auto tmpBlur4 = BuildBoxLinearGradientBlur(image, canvas, radius, alphaGradientShader, blurMatrix);
    Drawing::Matrix invBlurMatrix;
    auto blurShader = Drawing::ShaderEffect::CreateImageShader(
        *tmpBlur4, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, linear, invBlurMatrix);
    Drawing::RuntimeShaderBuilder builder(textureShaderEffect_);
    builder.SetChild("imageInput", blurShader);
    auto imageInfo = Drawing::ImageInfo(dst.GetWidth(), dst.GetHeight(), image->GetImageInfo().GetColorType(),
        image->GetImageInfo().GetAlphaType(), image->GetImageInfo().GetColorSpace());
#ifdef RS_ENABLE_GPU
    auto blurImage = builder.MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
#else
    auto blurImage = builder.MakeImage(nullptr, nullptr, imageInfo, false);
#endif
    if (!blurImage) {
        LOGE("GEVariableRadiusBlurShaderFilter::DrawBoxLinearGradientBlur fail to make final image");
        return image;
    }
    return blurImage;
}
 
std::shared_ptr<Drawing::Image> GEVariableRadiusBlurShaderFilter::BuildBoxLinearGradientBlur(
    const std::shared_ptr<Drawing::Image>& image, Drawing::Canvas& canvas, float radius,
    std::shared_ptr<Drawing::ShaderEffect> alphaGradientShader, Drawing::Matrix blurMatrix)
{
    auto width = image->GetWidth();
    auto height = image->GetHeight();
    auto originImageInfo = image->GetImageInfo();
    auto scaledInfo = Drawing::ImageInfo(std::ceil(width), std::ceil(height),
        originImageInfo.GetColorType(), originImageInfo.GetAlphaType(), originImageInfo.GetColorSpace());
    Drawing::Matrix m;
    Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    Drawing::RuntimeShaderBuilder hBlurBuilder(horizontalBoxBlurShaderEffect_);
    hBlurBuilder.SetUniform("r", radius);
    hBlurBuilder.SetChild("gradientShader", alphaGradientShader);
 
    Drawing::RuntimeShaderBuilder vBlurBuilder(verticalBoxBlurShaderEffect_);
    vBlurBuilder.SetUniform("r", radius);
    vBlurBuilder.SetChild("gradientShader", alphaGradientShader);
    constexpr int blurTimes = 4;
    auto localImage = image;
    auto blurImageShader = Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, linear, blurMatrix);
    bool isHorizontal = true;
    for (int i = 0; i < blurTimes; i++) {
        Drawing::RuntimeShaderBuilder blurBuilder = isHorizontal ? hBlurBuilder : vBlurBuilder;
        isHorizontal = !isHorizontal;
        blurBuilder.SetChild("imageShader", blurImageShader);
        
#ifdef RS_ENABLE_GPU
        localImage = blurBuilder.MakeImage(canvas.GetGPUContext().get(), nullptr, scaledInfo, false);
#else
        localImage = blurBuilder.MakeImage(nullptr, nullptr, scaledInfo, false);
#endif
        if (!localImage) {
            LOGE("GEVariableRadiusBlurShaderFilter::BuildBoxLinearGradientBlur fail to make blur image");
            return image;
        }
        blurImageShader = Drawing::ShaderEffect::CreateImageShader(
            *localImage, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, linear, m);
    }
    return localImage;
}

std::string GEVariableRadiusBlurShaderFilter::GetDescription()
{
    return "GEVariableRadiusBlurShaderFilter";
}

std::string GEVariableRadiusBlurShaderFilter::GetDetailedDescription()
{
    return "GEVariableRadiusBlurShaderFilterBlur, radius: " + std::to_string(params_.blurRadius);
}
} // namespace Rosen
} // namespace OHOS