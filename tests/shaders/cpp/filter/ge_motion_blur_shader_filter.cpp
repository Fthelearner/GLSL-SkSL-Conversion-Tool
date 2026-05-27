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

#include "ge_motion_blur_shader_filter.h"

#include <algorithm>
#include <cmath>

#include "draw/surface.h"
#include "ge_log.h"

namespace OHOS {
namespace Rosen {

namespace {
constexpr int32_t MAX_SAMPLE_COUNT = 50;
constexpr int32_t MIN_SAMPLE_COUNT = 1;
}

thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_motionBlurEffect = nullptr;

GEMotionBlurShaderFilter::GEMotionBlurShaderFilter(const Drawing::GEMotionBlurShaderFilterParams& params)
    : radius_(params.radius), anchor_(params.anchor), sampleCount_(params.sampleCount)
{
    if (radius_ < 0.0f) {
        radius_ = 0.0f;
    }
    sampleCount_ = std::clamp(sampleCount_, MIN_SAMPLE_COUNT, MAX_SAMPLE_COUNT);

    anchor_[0] = std::clamp(anchor_[0], 0.0f, 1.0f);
    anchor_[1] = std::clamp(anchor_[1], 0.0f, 1.0f);
}

std::shared_ptr<Drawing::RuntimeEffect> GEMotionBlurShaderFilter::GetMotionBlurEffect()
{
    if (g_motionBlurEffect == nullptr) {
        // Note: The loop upper bound in shader (50) must match MAX_SAMPLE_COUNT
        static const char* motionBlurProg = R"(
            uniform shader srcImageShader;
            uniform float2 scaleAnchor;
            uniform float2 scaleSize;
            uniform float2 rectOffset;
            uniform float radius;
            uniform float sampleCount;

            half4 main(float2 coord)
            {
                float2 scaleSizeStep = (scaleSize - 1.0) / sampleCount * radius;
                float2 rectOffsetStep = rectOffset / sampleCount * radius;
                float2 samplingOffset = (coord - scaleAnchor) * scaleSizeStep + rectOffsetStep;

                half4 color = srcImageShader.eval(coord) * 0.11;

                float remainingWeight = 0.89;
                float baseWeight = remainingWeight * 2.0 / (sampleCount + 1.0);
                float weightStep = baseWeight / sampleCount;
                float weight = baseWeight;
                int sampleCountInt = int(sampleCount);

                for (int i = 0; i < 50; i++) {
                    if (i >= sampleCountInt) break;
                    float2 offsetCoord = coord + samplingOffset * float(i + 1);
                    color += srcImageShader.eval(offsetCoord) * weight;
                    weight -= weightStep;
                }

                return color;
            }
        )";
        g_motionBlurEffect = Drawing::RuntimeEffect::CreateForShader(motionBlurProg);
        if (g_motionBlurEffect == nullptr) {
            LOGE("GEMotionBlurShaderFilter::GetMotionBlurEffect create failed");
            return nullptr;
        }
    }
    return g_motionBlurEffect;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEMotionBlurShaderFilter::MakeMotionBlurShader(
    std::shared_ptr<Drawing::ShaderEffect> srcImageShader,
    const Vector2f& scaleAnchor, const Vector2f& scaleSize, const Vector2f& rectOffset) const
{
    auto effect = GetMotionBlurEffect();
    if (effect == nullptr) {
        return nullptr;
    }

    auto builder = std::make_shared<Drawing::RuntimeShaderBuilder>(effect);
    builder->SetChild("srcImageShader", srcImageShader);
    builder->SetUniform("scaleAnchor", scaleAnchor[0], scaleAnchor[1]);
    builder->SetUniform("scaleSize", scaleSize[0], scaleSize[1]);
    builder->SetUniform("rectOffset", rectOffset[0], rectOffset[1]);
    builder->SetUniform("radius", radius_);
    builder->SetUniform("sampleCount", sampleCount_);
    return builder;
}

void GEMotionBlurShaderFilter::Preprocess(Drawing::Canvas& canvas,
    const Drawing::Rect& src, const Drawing::Rect& dst)
{
    disableMotionBlur_ = canvasInfo_.disableFilterCache;
}

void GEMotionBlurShaderFilter::CalculateRect(const Drawing::Rect& lastRect, const Drawing::Rect& curRect,
    Vector2f& rectOffset, Vector2f& scaleSize, Vector2f& scaleAnchorCoord) const
{
    rectOffset[0] = curRect.GetLeft() - lastRect.GetLeft() +
        (curRect.GetWidth() - lastRect.GetWidth()) * anchor_[0];
    rectOffset[1] = curRect.GetTop() - lastRect.GetTop() +
        (curRect.GetHeight() - lastRect.GetHeight()) * anchor_[1];
    scaleSize[0] = curRect.GetWidth() / lastRect.GetWidth();
    scaleSize[1] = curRect.GetHeight() / lastRect.GetHeight();

    if (curRect.GetWidth() < lastRect.GetWidth() && curRect.GetHeight() < lastRect.GetHeight()) {
        rectOffset[0] = -rectOffset[0];
        rectOffset[1] = -rectOffset[1];
        scaleSize[0] = 1.0f / scaleSize[0];
        scaleSize[1] = 1.0f / scaleSize[1];
    }

    if (scaleSize[0] > FLOAT_SCALE_THRESHOLD || scaleSize[1] > FLOAT_SCALE_THRESHOLD) {
        rectOffset[0] = rectOffset[0] * FLOAT_SCALE_THRESHOLD / scaleSize[0];
        rectOffset[1] = rectOffset[1] * FLOAT_SCALE_THRESHOLD / scaleSize[1];
        scaleSize[0] = FLOAT_SCALE_THRESHOLD;
        scaleSize[1] = FLOAT_SCALE_THRESHOLD;
    }
    rectOffset[0] = rectOffset[0] * FLOAT_IMAGE_SCALE;
    rectOffset[1] = rectOffset[1] * FLOAT_IMAGE_SCALE;

    scaleAnchorCoord = {anchor_[0] * curRect.GetWidth() * FLOAT_IMAGE_SCALE,
        anchor_[1] * curRect.GetHeight() * FLOAT_IMAGE_SCALE};
}

bool GEMotionBlurShaderFilter::RectValid(const Drawing::Rect& rect1, const Drawing::Rect& rect2) const
{
    if (rect1.GetWidth() < 1 || rect1.GetHeight() < 1 || rect2.GetWidth() < 1 || rect2.GetHeight() < 1) {
        return false;
    }

    if (rect1 == rect2) {
        return false;
    }

    return true;
}

bool GEMotionBlurShaderFilter::ValidateInput(const std::shared_ptr<Drawing::Image>& image) const
{
    if (!image || image->GetWidth() == 0 || image->GetHeight() == 0) {
        LOGE("GEMotionBlurShaderFilter::OnProcessImage image error");
        return false;
    }
    return true;
}

Drawing::Rect GEMotionBlurShaderFilter::CalculateCurrentRect(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image>& image) const
{
    Drawing::Matrix mat = canvas.GetTotalMatrix();
    Drawing::Rect rect = Drawing::Rect(0.f, 0.f,
        static_cast<float>(image->GetWidth()), static_cast<float>(image->GetHeight()));
    mat.MapRect(rect, rect);
    return Drawing::Rect(rect.GetLeft(), rect.GetTop(), rect.GetRight(), rect.GetBottom());
}

std::shared_ptr<Drawing::Image> GEMotionBlurShaderFilter::CreateBlurImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image>& image, const Drawing::Rect& src, const Drawing::Rect& dst,
    const Vector2f& rectOffset, const Vector2f& scaleSize, const Vector2f& scaleAnchorCoord) const
{
    Drawing::Matrix inputMatrix;
    inputMatrix.Translate(-src.GetLeft(), -src.GetTop());
    inputMatrix.PostScale(FLOAT_IMAGE_SCALE, FLOAT_IMAGE_SCALE);

    Drawing::Matrix matrix;
    matrix.Translate(dst.GetLeft(), dst.GetTop());
    inputMatrix.PostConcat(matrix);

    auto imageShader = Drawing::ShaderEffect::CreateImageShader(*image,
        Drawing::TileMode::DECAL, Drawing::TileMode::DECAL,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), inputMatrix);

    auto builder = MakeMotionBlurShader(imageShader, scaleAnchorCoord, scaleSize, rectOffset);
    if (builder == nullptr) {
        LOGE("GEMotionBlurShaderFilter::OnProcessImage shader builder is nullptr");
        return nullptr;
    }

    auto originImageInfo = image->GetImageInfo();
    auto scaledInfo = Drawing::ImageInfo(
        static_cast<int>(std::ceil(image->GetWidth() * FLOAT_IMAGE_SCALE)),
        static_cast<int>(std::ceil(image->GetHeight() * FLOAT_IMAGE_SCALE)),
        originImageInfo.GetColorType(), originImageInfo.GetAlphaType(),
        originImageInfo.GetColorSpace());

#ifdef RS_ENABLE_GPU
    auto tmpBlur = builder->MakeImage(canvas.GetGPUContext().get(), nullptr, scaledInfo, false);
#else
    auto tmpBlur = builder->MakeImage(nullptr, nullptr, scaledInfo, false);
#endif
    if (tmpBlur == nullptr) {
        LOGE("GEMotionBlurShaderFilter::OnProcessImage blur image is nullptr");
    }

    return tmpBlur;
}

std::shared_ptr<Drawing::Image> GEMotionBlurShaderFilter::CreateUpscaledImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image>& blurImage) const
{
    if (!blurImage || blurImage->GetWidth() == 0 || blurImage->GetHeight() == 0) {
        LOGE("GEMotionBlurShaderFilter::CreateUpscaledImage blurImage error");
        return nullptr;
    }

    auto surface = canvas.GetSurface();
    if (surface == nullptr) {
        LOGE("GEMotionBlurShaderFilter::CreateUpscaledImage surface is nullptr");
        return nullptr;
    }

    float invBlurScale = 1.0f / FLOAT_IMAGE_SCALE;
    int fullWidth = static_cast<int>(std::ceil(blurImage->GetWidth() * invBlurScale));
    int fullHeight = static_cast<int>(std::ceil(blurImage->GetHeight() * invBlurScale));

    auto offscreenSurface = surface->MakeSurface(fullWidth, fullHeight);
    if (offscreenSurface == nullptr) {
        LOGE("GEMotionBlurShaderFilter::CreateUpscaledImage offscreenSurface is nullptr");
        return nullptr;
    }

    Drawing::Matrix invBlurMatrix;
    invBlurMatrix.PostScale(invBlurScale, invBlurScale);

    Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    auto blurShader = Drawing::ShaderEffect::CreateImageShader(*blurImage,
        Drawing::TileMode::DECAL, Drawing::TileMode::DECAL, linear, invBlurMatrix);

    Drawing::Brush brush;
    brush.SetShaderEffect(blurShader);
    brush.SetBlendMode(Drawing::BlendMode::SRC);

    auto offscreenCanvas = offscreenSurface->GetCanvas();
    offscreenCanvas->DrawBackground(brush);

    return offscreenSurface->GetImageSnapshot();
}

void GEMotionBlurShaderFilter::UpdateCache(const Drawing::Rect& rect)
{
    MotionBlurCacheData newCacheData;
    newCacheData.lastRect = Drawing::Rect(rect.GetLeft(), rect.GetTop(), rect.GetRight(), rect.GetBottom());
    newCacheData.radius = radius_;
    newCacheData.anchor = anchor_;
    newCacheData.sampleCount = sampleCount_;
    SetCache(std::make_shared<std::any>(newCacheData));
}

Drawing::Rect GEMotionBlurShaderFilter::GetLastRectFromCache()
{
    auto cache = GetCache();
    if (!cache || !cache->has_value()) {
        return Drawing::Rect(0.f, 0.f, 0.f, 0.f);
    }

    auto* cachedData = std::any_cast<MotionBlurCacheData>(&*cache);
    if (cachedData == nullptr ||
        cachedData->radius != radius_ ||
        cachedData->anchor[0] != anchor_[0] ||
        cachedData->anchor[1] != anchor_[1] ||
        cachedData->sampleCount != sampleCount_) {
        return Drawing::Rect(0.f, 0.f, 0.f, 0.f);
    }
    return cachedData->lastRect;
}

std::shared_ptr<Drawing::Image> GEMotionBlurShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (!ValidateInput(image)) {
        UpdateCache(Drawing::Rect(0.f, 0.f, 0.f, 0.f));
        return nullptr;
    }

    if (radius_ <= 0) {
        UpdateCache(Drawing::Rect(0.f, 0.f, 0.f, 0.f));
        return image;
    }

    if (disableMotionBlur_) {
        return image;
    }

    Drawing::Rect lastRect = GetLastRectFromCache();
    Drawing::Rect curRect = CalculateCurrentRect(canvas, image);
    if (!RectValid(lastRect, curRect)) {
        UpdateCache(curRect);
        return image;
    }

    Vector2f rectOffset;
    Vector2f scaleSize;
    Vector2f scaleAnchorCoord;
    CalculateRect(lastRect, curRect, rectOffset, scaleSize, scaleAnchorCoord);

    auto tmpBlur = CreateBlurImage(canvas, image, src, src, rectOffset, scaleSize, scaleAnchorCoord);
    if (tmpBlur == nullptr) {
        return image;
    }

    UpdateCache(curRect);
    auto finalImage = CreateUpscaledImage(canvas, tmpBlur);
    return finalImage != nullptr ? finalImage : tmpBlur;
}
} // namespace Rosen
} // namespace OHOS
