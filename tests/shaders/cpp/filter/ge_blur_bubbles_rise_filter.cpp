/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "ge_blur_bubbles_rise_filter.h"

#include <algorithm>

#include "ge_log.h"

namespace OHOS {
namespace Rosen {

namespace {
constexpr float BLUR_INTENSITY_SCALE_FACTOR = 10.0f;
struct BlurBubblesRiseProcessContext {
    Drawing::Matrix matrix;
    Drawing::Matrix invertMatrix;
    Drawing::ImageInfo imageInfo;
    float width = 0.0f;
    float height = 0.0f;
};

inline static const std::string g_shaderStringResample = R"(
    uniform shader image;
    uniform half2 srcResolution;
    uniform half2 dstResolution;

    half4 main(float2 fragCoord)
    {
        half2 uv = fragCoord / max(dstResolution, half2(1.0));
        return image.eval(uv * srcResolution);
    }
)";

std::shared_ptr<Drawing::Image> MakeRuntimeImage(Drawing::RuntimeShaderBuilder& builder, Drawing::Canvas& canvas,
    const Drawing::Matrix& matrix, const Drawing::ImageInfo& imageInfo)
{
#ifdef RS_ENABLE_GPU
    return builder.MakeImage(canvas.GetGPUContext().get(), &matrix, imageInfo, false);
#else
    return builder.MakeImage(nullptr, &matrix, imageInfo, false);
#endif
}

std::shared_ptr<Drawing::RuntimeEffect> GetResampleShaderEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_resampleShader = nullptr;
    if (g_resampleShader == nullptr) {
        g_resampleShader = Drawing::RuntimeEffect::CreateForShader(g_shaderStringResample);
    }
    return g_resampleShader;
}

bool PrepareProcessContext(const std::shared_ptr<Drawing::Image>& image, const Drawing::Matrix& canvasMatrix,
    float tranX, float tranY, BlurBubblesRiseProcessContext& context)
{
    context.matrix = canvasMatrix;
    context.matrix.PostTranslate(-tranX, -tranY);
    if (!context.matrix.Invert(context.invertMatrix)) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage invert matrix failed");
        return false;
    }

    context.imageInfo = image->GetImageInfo();
    context.width = context.imageInfo.GetWidth();
    context.height = context.imageInfo.GetHeight();
    if (context.width < 1e-6 || context.height < 1e-6) {
        return false;
    }
    return true;
}

std::shared_ptr<Drawing::ShaderEffect> BuildImageShader(const std::shared_ptr<Drawing::Image>& image,
    Drawing::TileMode tileModeX, Drawing::TileMode tileModeY, const BlurBubblesRiseProcessContext& context)
{
    if (image == nullptr) {
        return nullptr;
    }
    return Drawing::ShaderEffect::CreateImageShader(*image, tileModeX, tileModeY,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), context.invertMatrix);
}

struct DownsampleParams {
    int32_t width = 1;
    int32_t height = 1;
    float widthF = 1.0f;
    float heightF = 1.0f;
    float blurIntensity = 0.0f;
    Drawing::ImageInfo imageInfo;
};

DownsampleParams BuildDownsampleParams(const BlurBubblesRiseProcessContext& context, float blurIntensity)
{
    DownsampleParams params;
    params.width = std::max(1, static_cast<int32_t>(context.width * 0.5f));
    params.height = std::max(1, static_cast<int32_t>(context.height * 0.5f));
    params.widthF = static_cast<float>(params.width);
    params.heightF = static_cast<float>(params.height);
    params.blurIntensity = blurIntensity;
    params.imageInfo = Drawing::ImageInfo(params.width, params.height, context.imageInfo.GetColorType(),
        context.imageInfo.GetAlphaType(), context.imageInfo.GetColorSpace());
    return params;
}

std::shared_ptr<Drawing::ShaderEffect> BuildDownsampledShader(Drawing::Canvas& canvas,
    const BlurBubblesRiseProcessContext& context,
    const std::shared_ptr<Drawing::ShaderEffect>& sourceShader,
    const std::shared_ptr<Drawing::RuntimeEffect>& resampleEffect,
    const DownsampleParams& params)
{
    Drawing::RuntimeShaderBuilder downsampleBuilder(resampleEffect);
    downsampleBuilder.SetChild("image", sourceShader);
    downsampleBuilder.SetUniform("srcResolution", context.width, context.height);
    downsampleBuilder.SetUniform("dstResolution", params.widthF, params.heightF);

    auto downsampledImage = MakeRuntimeImage(downsampleBuilder, canvas, context.matrix, params.imageInfo);
    if (downsampledImage == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage downsample image build failed");
        return nullptr;
    }

    auto downsampledShader = BuildImageShader(downsampledImage, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        context);
    if (downsampledShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage downsample shader create failed");
    }
    return downsampledShader;
}

std::shared_ptr<Drawing::Image> BuildHalfResBlurredImage(Drawing::Canvas& canvas,
    const BlurBubblesRiseProcessContext& context,
    const std::shared_ptr<Drawing::ShaderEffect>& downsampledShader,
    const std::shared_ptr<Drawing::RuntimeEffect>& blurEffect,
    const DownsampleParams& params)
{
    Drawing::RuntimeShaderBuilder blurBuilderX(blurEffect);
    blurBuilderX.SetChild("image", downsampledShader);
    blurBuilderX.SetUniform("iResolution", params.widthF, params.heightF);
    blurBuilderX.SetUniform("blurIntensity", params.blurIntensity * BLUR_INTENSITY_SCALE_FACTOR);
    blurBuilderX.SetUniform("horizontal", 1.0f);
    auto blurredImageX = MakeRuntimeImage(blurBuilderX, canvas, context.matrix, params.imageInfo);
    if (blurredImageX == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage blur X image build failed");
        return nullptr;
    }

    auto blurXShader = BuildImageShader(blurredImageX, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, context);
    if (blurXShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage blur X shader create failed");
        return nullptr;
    }

    Drawing::RuntimeShaderBuilder blurBuilderY(blurEffect);
    blurBuilderY.SetChild("image", blurXShader);
    blurBuilderY.SetUniform("iResolution", params.widthF, params.heightF);
    blurBuilderY.SetUniform("blurIntensity", params.blurIntensity * BLUR_INTENSITY_SCALE_FACTOR);
    blurBuilderY.SetUniform("horizontal", 0.0f);
    auto blurredImage = MakeRuntimeImage(blurBuilderY, canvas, context.matrix, params.imageInfo);
    if (blurredImage == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage blur Y image build failed");
    }
    return blurredImage;
}

std::shared_ptr<Drawing::ShaderEffect> BuildUpsampledBlurredShader(Drawing::Canvas& canvas,
    const BlurBubblesRiseProcessContext& context,
    const std::shared_ptr<Drawing::Image>& downsampledBlurredImage,
    const std::shared_ptr<Drawing::RuntimeEffect>& resampleEffect,
    const DownsampleParams& params)
{
    auto downsampledBlurredShader = BuildImageShader(downsampledBlurredImage,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, context);
    if (downsampledBlurredShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage downsample blurred shader create failed");
        return nullptr;
    }

    Drawing::RuntimeShaderBuilder upsampleBuilder(resampleEffect);
    upsampleBuilder.SetChild("image", downsampledBlurredShader);
    upsampleBuilder.SetUniform("srcResolution", params.widthF, params.heightF);
    upsampleBuilder.SetUniform("dstResolution", context.width, context.height);
    auto upsampledImage = MakeRuntimeImage(upsampleBuilder, canvas, context.matrix, context.imageInfo);
    if (upsampledImage == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage upsample image build failed");
        return nullptr;
    }

    auto blurredShader = BuildImageShader(upsampledImage, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        context);
    if (blurredShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage blurred shader create failed");
    }
    return blurredShader;
}

std::shared_ptr<Drawing::ShaderEffect> BuildBlurredShader(Drawing::Canvas& canvas,
    const BlurBubblesRiseProcessContext& context,
    const std::shared_ptr<Drawing::ShaderEffect>& sourceShader,
    const std::shared_ptr<Drawing::RuntimeEffect>& blurEffect, float blurIntensity)
{
    auto resampleEffect = GetResampleShaderEffect();
    if (resampleEffect == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage resample effect create failed");
        return nullptr;
    }
    auto params = BuildDownsampleParams(context, blurIntensity);
    auto downsampledShader = BuildDownsampledShader(canvas, context, sourceShader, resampleEffect, params);
    if (downsampledShader == nullptr) {
        return nullptr;
    }

    auto downsampledBlurredImage = BuildHalfResBlurredImage(canvas, context, downsampledShader, blurEffect, params);
    if (downsampledBlurredImage == nullptr) {
        return nullptr;
    }

    return BuildUpsampledBlurredShader(canvas, context, downsampledBlurredImage, resampleEffect, params);
}

} // namespace

GEBlurBubblesRiseFilter::GEBlurBubblesRiseFilter(const Drawing::GEBlurBubblesRiseFilterParams& params)
    : blurIntensity_(params.blurIntensity), mixStrength_(params.mixStrength), progress_(params.progress),
    maskImage_(params.maskImage)
{}

std::shared_ptr<Drawing::Image> GEBlurBubblesRiseFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& /*src*/, const Drawing::Rect& /*dst*/)
{
    if (image == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    BlurBubblesRiseProcessContext context;
    if (!PrepareProcessContext(image, canvasInfo_.mat, canvasInfo_.tranX, canvasInfo_.tranY, context)) {
        return image;
    }

    auto sourceShader = BuildImageShader(image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, context);
    if (sourceShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage source shader create failed");
        return image;
    }

    auto blurEffect = GetBlurShaderEffect();
    auto mixEffect = GetMaskMixShaderEffect();
    if (blurEffect == nullptr || mixEffect == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage runtime effect create failed");
        return image;
    }

    CheckBlurBubblesRiseParams();

    auto blurredShader = BuildBlurredShader(canvas, context, sourceShader, blurEffect, blurIntensity_);
    if (blurredShader == nullptr) {
        return image;
    }

    auto maskImage = (maskImage_ != nullptr) ? maskImage_ : image;
    auto maskShader = BuildImageShader(maskImage, Drawing::TileMode::MIRROR, Drawing::TileMode::MIRROR, context);
    if (maskShader == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage mask shader create failed");
        return image;
    }

    Drawing::RuntimeShaderBuilder mixBuilder(mixEffect);
    mixBuilder.SetChild("blur_tex", blurredShader);
    mixBuilder.SetChild("original_tex", sourceShader);
    mixBuilder.SetChild("blur_mask", maskShader);
    mixBuilder.SetUniform("iResolution", context.width, context.height);
    mixBuilder.SetUniform("mixStrength", mixStrength_);
    mixBuilder.SetUniform("progress", progress_ * timeScale_);

    auto outputImage = MakeRuntimeImage(mixBuilder, canvas, context.matrix, context.imageInfo);
    if (outputImage == nullptr) {
        LOGE("GEBlurBubblesRiseFilter::OnProcessImage output image build failed");
        return image;
    }

    return outputImage;
}

void GEBlurBubblesRiseFilter::CheckBlurBubblesRiseParams()
{
    blurIntensity_ = std::clamp(blurIntensity_, 0.0f, 1.0f);
    mixStrength_ = std::clamp(mixStrength_, 0.0f, 1.0f);
    progress_ = std::clamp(progress_, 0.0f, 1.0f);
}

std::shared_ptr<Drawing::RuntimeEffect> GEBlurBubblesRiseFilter::GetBlurShaderEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_blurShader = nullptr;
    if (g_blurShader == nullptr) {
        g_blurShader = Drawing::RuntimeEffect::CreateForShader(shaderStringGaussianBlur);
    }
    return g_blurShader;
}

std::shared_ptr<Drawing::RuntimeEffect> GEBlurBubblesRiseFilter::GetMaskMixShaderEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_maskMixShader = nullptr;
    if (g_maskMixShader == nullptr) {
        g_maskMixShader = Drawing::RuntimeEffect::CreateForShader(shaderStringMaskMix);
    }
    return g_maskMixShader;
}

} // namespace Rosen
} // namespace OHOS
