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

#include "ge_edge_light_shader_filter.h"

#include <array>
#include <vector>

#include "ge_log.h"
#include "ge_system_properties.h"
#include "ge_tone_mapping_helper.h"

namespace OHOS {
namespace Rosen {

namespace {
static constexpr uint32_t MIP_LEVEL = 3;  // blur times
static constexpr float MIN_IMAGE_SIZE = 1.0;
static constexpr float MIN_IMAGE_BLOOM_SIZE = 16.0; // < MIN_IMAGE_BLOOM_SIZE, not bloom, return edgeImage.
static constexpr float ALPHA_MIN = 0.0;
static constexpr float ALPHA_MAX = 1.0;

inline static const std::string g_shaderStringConvertFrag = R"(
    uniform shader image;

    vec4 main(vec2 fragCoord)
    {
        return image.eval(fragCoord).rgba;
    }
)";

inline static const std::string g_shaderStringDetectFrag = R"(
    uniform shader image;
    uniform float edgeThreshold;
    uniform float edgeIntensity;
    uniform float edgeSoftThreshold;
    uniform vec3 edgeDetectColor;
    uniform vec3 edgeColor;
    uniform float ifRawColor;

    vec4 main(vec2 fragCoord)
    {
        float x = 0.0;
        float y = 0.0;

        x += dot(edgeDetectColor, image.eval(fragCoord + vec2(-1, -1)).rgb) * 1.0;
        x += dot(edgeDetectColor, image.eval(fragCoord + vec2(-1, 0)).rgb) * 2.0;
        x += dot(edgeDetectColor, image.eval(fragCoord + vec2(-1, 1)).rgb) * 1.0;
        x -= dot(edgeDetectColor, image.eval(fragCoord + vec2(1, -1)).rgb) * 1.0;
        x -= dot(edgeDetectColor, image.eval(fragCoord + vec2(1, 0)).rgb) * 2.0;
        x -= dot(edgeDetectColor, image.eval(fragCoord + vec2(1, 1)).rgb) * 1.0;

        y += dot(edgeDetectColor, image.eval(fragCoord + vec2(-1, 1)).rgb) * 1.0;
        y += dot(edgeDetectColor, image.eval(fragCoord + vec2(0, 1)).rgb) * 2.0;
        y += dot(edgeDetectColor, image.eval(fragCoord + vec2(1, 1)).rgb) * 1.0;
        y -= dot(edgeDetectColor, image.eval(fragCoord + vec2(-1, -1)).rgb) * 1.0;
        y -= dot(edgeDetectColor, image.eval(fragCoord + vec2(0, -1)).rgb) * 2.0;
        y -= dot(edgeDetectColor, image.eval(fragCoord + vec2(1, -1)).rgb) * 1.0;

        float sobel = sqrt(x * x + y * y);
        sobel = edgeIntensity * smoothstep(
            edgeThreshold - edgeSoftThreshold,
            edgeThreshold + edgeSoftThreshold,
            sobel
        );

        vec3 color = sobel * ((ifRawColor > 0.5) ? image.eval(fragCoord).rgb : edgeColor);
        return vec4(color, 1.0);
    }
)";

inline static const std::string g_shaderStringGaussianFrag = R"(
    uniform shader image;
    uniform float blurDirection;
    uniform float sigma;

    const float pi = 3.1415926;

    float gaussian(float x, float sigma)
    {
        return exp(-(x * x) / (2.0 * sigma * sigma)) / (sigma * sqrt(2.0 * pi));
    }

    vec4 main(vec2 fragCoord)
    {
        vec2 blur_direct = vec2(1.0, 0.0);
        if (blurDirection > 0.5) {
            blur_direct = vec2(0.0, 1.0);
        }

        vec2 newFragCoord = vec2(0.0);
        vec3 dest = vec3(0.0);
        float w_sum = 0.0;
        for (int i = -2; i <= 2; i++) {
            float w = gaussian(float(i), sigma);
            newFragCoord = fragCoord + float(i) * blur_direct;
            dest += w * image.eval(newFragCoord).rgb;
            w_sum += w;
        }
        dest /= w_sum;

        return vec4(dest.rgb, 1.0);
    }
)";

inline static const std::string g_shaderStringCompositeFrag = R"(
    uniform shader imageBlur0;
    uniform shader imageBlur1;
    uniform shader imageBlur2;
    uniform shader imageBlur3;
    uniform shader imageBlur4;
    vec4 main(vec2 fragCoord)
    {
        vec3 composited_color = imageBlur0.eval(fragCoord).rgb;
        float weight = 0.25;
        composited_color += weight * (imageBlur1.eval(fragCoord).rgb
            + imageBlur2.eval(fragCoord).rgb
            + imageBlur3.eval(fragCoord).rgb
            + imageBlur4.eval(fragCoord).rgb
        );
        return vec4(composited_color.rgb, 1.0);
    }
)";

inline static const std::string g_shaderStringAddMask = R"(
    uniform shader imageMask;
    uniform shader image;
    uniform shader imageBloom;
    uniform vec2 iResolution;
    uniform float alphaProgress;
    vec4 main(vec2 fragCoord)
    {
        vec4 image_color = image.eval(fragCoord).rgba;
        float mask = imageMask.eval(fragCoord).a;
        vec4 final_color = image_color;
        vec4 dst = vec4(1.0, 1.0, iResolution.x - 1.0, iResolution.y - 1.0); // Anti-aliasing smoothing
        if (dst.x < fragCoord.x && fragCoord.x < dst.z &&
            dst.y < fragCoord.y && fragCoord.y < dst.w && mask > 0.0) {
            final_color = vec4(alphaProgress * imageBloom.eval(fragCoord).rgb * mask + image_color.rgb, image_color.a);
        }

        return final_color;
    }
)";

inline static const std::string g_shaderStringAlphaGradient = R"(
    uniform shader image;
    uniform shader imageBloom;
    uniform vec2 iResolution;
    uniform float alphaProgress;
    vec4 main(vec2 fragCoord)
    {
        vec4 image_color = image.eval(fragCoord).rgba;
        vec4 final_color = image_color;
        vec4 dst = vec4(1.0, 1.0, iResolution.x - 1.0, iResolution.y - 1.0); // Anti-aliasing smoothing
        if (dst.x < fragCoord.x && fragCoord.x < dst.z &&
            dst.y < fragCoord.y && fragCoord.y < dst.w) {
            final_color = vec4(alphaProgress * imageBloom.eval(fragCoord).rgb + image_color.rgb, image_color.a);
        }
        return final_color;
    }
)";

// for init shader effect only once.
// thread_local for thread safety and freeing variables.
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_convertShaderEffect = nullptr;
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_detectShaderEffect = nullptr;
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_gaussShaderEffect = nullptr;
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_compShaderEffect = nullptr;
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_addMaskEffect = nullptr;
static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_alphaShaderEffect = nullptr;
static thread_local uint32_t g_originalImageID = 0; // When the ID is the same as the last one, the cached image is used
static thread_local std::shared_ptr<Drawing::Image> g_afterCompositeImage = nullptr; // cache calculated image
static thread_local Vector4f g_color; // support Hdr, maybe > 1
} // namespace

bool GEEdgeLightShaderFilter::InitConvertFragShaderEffect()
{
    if (g_convertShaderEffect != nullptr) {
        return true;
    }

    g_convertShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringConvertFrag);
    if (g_convertShaderEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_convertShaderEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::InitDetectFragShaderEffect()
{
    if (g_detectShaderEffect != nullptr) {
        return true;
    }

    g_detectShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringDetectFrag);
    if (g_detectShaderEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_detectShaderEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::InitGaussShaderEffect()
{
    if (g_gaussShaderEffect != nullptr) {
        return true;
    }

    g_gaussShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringGaussianFrag);
    if (g_gaussShaderEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_gaussShaderEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::InitCompositeShaderEffect()
{
    if (g_compShaderEffect != nullptr) {
        return true;
    }

    g_compShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringCompositeFrag);
    if (g_compShaderEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_compShaderEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::InitMaskShaderEffect()
{
    if (g_addMaskEffect != nullptr) {
        return true;
    }

    g_addMaskEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringAddMask);
    if (g_addMaskEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_addMaskEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::InitMergeImageShaderEffect()
{
    if (g_alphaShaderEffect != nullptr) {
        return true;
    }

    g_alphaShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringAlphaGradient);
    if (g_alphaShaderEffect == nullptr) {
        LOGE("GEEdgeLightShaderFilter::RuntimeShader g_alphaShaderEffect create failed.");
        return false;
    }

    return true;
}

bool GEEdgeLightShaderFilter::IsShaderEffectInitValid()
{
    if (!g_convertShaderEffect || !g_detectShaderEffect || !g_gaussShaderEffect ||
        !g_compShaderEffect || !g_addMaskEffect || !g_alphaShaderEffect) {
        LOGE("GEEdgeLightShaderFilter::IsShaderEffectInitValid failed.");
        return false;
    }
    return true;
}

std::shared_ptr<Drawing::Image> GEEdgeLightShaderFilter::ConvertColorSpace(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> image, std::shared_ptr<Drawing::ColorSpace> dstColorSpace)
{
    auto imageInfo = image->GetImageInfo();
    if (imageInfo.GetColorSpace()->Equals(dstColorSpace)) {
        LOGE("GEEdgeLightShaderFilter::ConvertColorSpace imageInfo.colorSpace equals target.");
        return image;
    }
    auto convertImageInfo = Drawing::ImageInfo(imageInfo.GetWidth(), imageInfo.GetHeight(), imageInfo.GetColorType(),
        imageInfo.GetAlphaType(), dstColorSpace);
    Drawing::Matrix matrix;
    auto srcImageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    if (srcImageShader == nullptr) {
        LOGE("GEEdgeLightShaderFilter::ConvertColorSpace create srcImageShader failed.");
        return nullptr;
    }
    auto convertBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_convertShaderEffect);
    convertBuilder->SetChild("image", srcImageShader);
#ifdef RS_ENABLE_GPU
    auto convertedImage = convertBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, convertImageInfo, false);
#else
    auto convertedImage = convertBuilder->MakeImage(nullptr, nullptr, convertImageInfo, false);
#endif
    LOGD("GEEdgeLightShaderFilter::ConvertColorSpace finished."); // if nullptr in ProcessImage;
    return convertedImage;
}

std::shared_ptr<Drawing::Image> GEEdgeLightShaderFilter::DetectEdge(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> image)
{
    // check image width and height in IsInputImageValid;
    auto imageInfo = image->GetImageInfo();
    float imageWidth = imageInfo.GetWidth();
    float imageHeight = imageInfo.GetHeight();
    float newWidth = std::ceil(imageWidth);
    float newHeight = std::ceil(imageHeight);
    Drawing::Matrix detectEdgeShaderMatrix;
    auto srcImageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), detectEdgeShaderMatrix);
    if (srcImageShader == nullptr) {
        LOGE("GEEdgeLightShaderFilter::DetectEdge create srcImageShader failed.");
        return nullptr;
    }
    auto detectBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_detectShaderEffect);
    detectBuilder->SetChild("image", srcImageShader);
    detectBuilder->SetUniform("edgeThreshold", 0.3f);
    detectBuilder->SetUniform("edgeIntensity", 0.8f);
    detectBuilder->SetUniform("edgeSoftThreshold", 0.3f);
    detectBuilder->SetUniform("edgeDetectColor", 0.2126729f, 0.7151522f, 0.0721750f);
    detectBuilder->SetUniform("edgeColor", color_.x_, color_.y_, color_.z_); // x_-red. y_-green. z_-blue
    detectBuilder->SetUniform("ifRawColor", useRawColor_);
    auto detectImageInfo = Drawing::ImageInfo(newWidth, newHeight,
        imageInfo.GetColorType(), imageInfo.GetAlphaType(), imageInfo.GetColorSpace());
#ifdef RS_ENABLE_GPU
    auto edgeImage = detectBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, detectImageInfo, false);
#else
    auto edgeImage = detectBuilder->MakeImage(nullptr, nullptr, detectImageInfo, false);
#endif
    LOGD("GEEdgeLightShaderFilter::DetectEdge finished."); // if nullptr in ProcessImage;
    return edgeImage;
}

std::shared_ptr<Drawing::Image> GEEdgeLightShaderFilter::GaussianBlur(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> edgeImage)
{
    // check image width and height in IsInputImageValid;
    auto imageInfo = edgeImage->GetImageInfo();
    float imageHeight = imageInfo.GetHeight();
    float imageWidth = imageInfo.GetWidth();
    if (imageHeight < MIN_IMAGE_BLOOM_SIZE || imageWidth < MIN_IMAGE_BLOOM_SIZE) {
        LOGD("GEEdgeLightShaderFilter::GaussianBlur image size is too small to bloom, return edgeImage. "
            "H:[%{public}f] W:[%{public}f]", imageHeight, imageWidth);
        return edgeImage;
    }

    if (!bloom_) {
        return edgeImage;
    }

    auto gaussianBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_gaussShaderEffect);
    auto compositeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_compShaderEffect);

    Drawing::Matrix edgeMatrix;
    auto edgeShader = Drawing::ShaderEffect::CreateImageShader(*edgeImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), edgeMatrix);
    if (edgeShader == nullptr) {
        LOGE("GEEdgeLightShaderFilter::GaussianBlur create edgeShader failed.");
        return nullptr;
    }
    compositeBuilder->SetChild("imageBlur0", edgeShader);
    float imageScale = 1.0f;
    auto blurImageV = edgeImage;
    auto scaledInfo = imageInfo;
    for (uint16_t i = 1; i <= MIP_LEVEL; i++) {
        Drawing::Matrix matrixV;
        Drawing::Matrix matrixH;
        Drawing::Matrix imageBlurShaderMatrix;

        float newWidth = std::ceil(imageWidth / imageScale);
        float newHeight = std::ceil(imageHeight / imageScale);

        scaledInfo = Drawing::ImageInfo(newWidth, newHeight,
            imageInfo.GetColorType(), imageInfo.GetAlphaType(), imageInfo.GetColorSpace());

        // step 1: Horizontal Gaussian blur
        auto srcImageShaderV = Drawing::ShaderEffect::CreateImageShader(*blurImageV, Drawing::TileMode::CLAMP,
            Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrixV);
        if (srcImageShaderV == nullptr) {
            LOGE("GEEdgeLightShaderFilter::GaussianBlur create srcImageShaderV failed.");
            return nullptr;
        }
        gaussianBuilder->SetChild("image", srcImageShaderV);
        gaussianBuilder->SetUniform("blurDirection", 0.0f);
        gaussianBuilder->SetUniform("sigma", 3.0f);
#ifdef RS_ENABLE_GPU
        auto blurImageH = gaussianBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, scaledInfo, false);
#else
        auto blurImageH = gaussianBuilder->MakeImage(nullptr, nullptr, scaledInfo, false);
#endif
        if (blurImageH == nullptr) {
            LOGE("GEEdgeLightShaderFilter::GaussianBlur blurImageH make image failed.");
            return nullptr;
        }
        // step 2: Vertical Gaussian blur
        auto srcImageShaderH = Drawing::ShaderEffect::CreateImageShader(*blurImageH, Drawing::TileMode::CLAMP,
            Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrixH);
        if (srcImageShaderH == nullptr) {
            LOGE("GEEdgeLightShaderFilter::GaussianBlur create srcImageShaderH failed.");
            return nullptr;
        }
        gaussianBuilder->SetChild("image", srcImageShaderH);
        gaussianBuilder->SetUniform("blurDirection", 1.0f);
        gaussianBuilder->SetUniform("sigma", 3.0f);
#ifdef RS_ENABLE_GPU
        blurImageV = gaussianBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, scaledInfo, false);
#else
        blurImageV = gaussianBuilder->MakeImage(nullptr, nullptr, scaledInfo, false);
#endif
        if (blurImageV == nullptr) {
            LOGE("GEEdgeLightShaderFilter::GaussianBlur blurImageV make image failed.");
            return nullptr;
        }
        // step 3: Composite Vertical and Horizontal Gaussian Blur
        imageBlurShaderMatrix.SetScale(std::ceil(imageWidth / newWidth), std::ceil(imageHeight / newHeight));
        auto imageBlurShader = Drawing::ShaderEffect::CreateImageShader(*blurImageV, Drawing::TileMode::CLAMP,
            Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), imageBlurShaderMatrix);
        if (imageBlurShader == nullptr) {
            LOGE("GEEdgeLightShaderFilter::GaussianBlur create imageBlurShader failed.");
            return nullptr;
        }
        compositeBuilder->SetChild("imageBlur" + std::to_string(i), imageBlurShader);
    }
    auto compImageInfo = Drawing::ImageInfo(imageInfo.GetWidth(), imageInfo.GetHeight(), imageInfo.GetColorType(),
        imageInfo.GetAlphaType(), imageInfo.GetColorSpace());
#ifdef RS_ENABLE_GPU
    auto compImage = compositeBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, compImageInfo, false);
#else
    auto compImage = compositeBuilder->MakeImage(nullptr, nullptr, compImageInfo, false);
#endif
    LOGD("GEEdgeLightShaderFilter::GaussianBlur finished."); // if nullptr in ProcessImage;
    return compImage;
}

std::shared_ptr<Drawing::Image> GEEdgeLightShaderFilter::MergeImage(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> image, const std::shared_ptr<Drawing::Image> compositeImage)
{
    // check image width and height in IsInputImageValid;
    auto imageInfo = image->GetImageInfo();
    float imageWidth = imageInfo.GetWidth();
    float imageHeight = imageInfo.GetHeight();
    auto mergeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_alphaShaderEffect);

    if (mask_) { // mask_: optional maybe == nullptr.
        LOGD("GEEdgeLightShaderFilter::MergeImage AddMask not null.");
        mergeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_addMaskEffect);
        auto maskEffectShader = mask_->GenerateDrawingShader(canvasInfo_.geoWidth, canvasInfo_.geoHeight);
        if (maskEffectShader == nullptr) {
            LOGE("GEEdgeLightShaderFilter::MergeImage AddMask GenerateDrawingShader failed.");
            return nullptr;
        }
        mergeBuilder->SetChild("imageMask", maskEffectShader);
    }

    // mergeBuilder set child src image
    Drawing::Matrix imageMatrix = canvasInfo_.mat;
    imageMatrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix invertImageMatrix;
    if (!imageMatrix.Invert(invertImageMatrix)) {
        LOGE("GEEdgeLightShaderFilter::MergeImage invert imageMatrix failed.");
        return image;
    }
    auto imageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertImageMatrix);
    if (imageShader == nullptr) {
        LOGE("GEEdgeLightShaderFilter::MergeImage create imageShader failed.");
        return nullptr;
    }
    mergeBuilder->SetChild("image", imageShader);

    // set mergeMatrix
    Drawing::Matrix mergeMatrix = canvasInfo_.mat;
    mergeMatrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);

    // mergeBuilder set child bloom image
    auto compositeImageInfo = compositeImage->GetImageInfo();
    float compositeImageWidth = compositeImageInfo.GetWidth();
    float compositeImageHeight = compositeImageInfo.GetHeight();
    auto scaleWRatio = compositeImageWidth / imageWidth;
    auto scaleHRatio = compositeImageHeight / imageHeight;
    Drawing::Matrix compositeMatrix = canvasInfo_.mat;

    compositeMatrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    compositeMatrix.PostScale(scaleWRatio, scaleHRatio);
    Drawing::Matrix invertCompositeMatrix;
    if (!compositeMatrix.Invert(invertCompositeMatrix)) {
        LOGE("GEEdgeLightShaderFilter::MergeImage invert compositeMatrix failed.");
        return image;
    }

    auto compositeImageShader = Drawing::ShaderEffect::CreateImageShader(*compositeImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertCompositeMatrix);
    if (compositeImageShader == nullptr) {
        LOGE("GEEdgeLightShaderFilter::MergeImage create compositeImageShader failed.");
        return nullptr;
    }
    mergeBuilder->SetChild("imageBloom", compositeImageShader);

    // mergeBuilder set uniforms
    mergeBuilder->SetUniform("iResolution", canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    mergeBuilder->SetUniform("alphaProgress", std::clamp(alpha_, ALPHA_MIN, ALPHA_MAX));
    auto retImageInfo = Drawing::ImageInfo(imageWidth, imageHeight, imageInfo.GetColorType(),
        imageInfo.GetAlphaType(), Drawing::ColorSpace::CreateSRGBLinear());
#ifdef RS_ENABLE_GPU
    auto retImage = mergeBuilder->MakeImage(canvas.GetGPUContext().get(), &(mergeMatrix), retImageInfo, false);
#else
    auto retImage = mergeBuilder->MakeImage(nullptr, &(mergeMatrix), retImageInfo, false);
#endif
    LOGD("GEEdgeLightShaderFilter::MergeImage finished."); // if nullptr in ProcessImage;
    return retImage;
}

bool GEEdgeLightShaderFilter::IsInputImageValid(const std::shared_ptr<Drawing::Image> image)
{
    if (image == nullptr) {
        LOGE("GEEdgeLightShaderFilter::Input image is nullptr.");
        return false;
    }

    auto imageInfo = image->GetImageInfo();
    float height = imageInfo.GetHeight();
    float width = imageInfo.GetWidth();
    if (height < MIN_IMAGE_SIZE || width < MIN_IMAGE_SIZE) {
        LOGE("GEEdgeLightShaderFilter::Input image size is invalide."
            "H:[%{public}f] W:[%{public}f]", height, width);
        return false;
    }

    if (!imageInfo.GetColorSpace()) {
        LOGE("GEEdgeLightShaderFilter::Input image colorSpace is null.");
        return false;
    }

    return true;
}

GEEdgeLightShaderFilter::GEEdgeLightShaderFilter(const Drawing::GEEdgeLightShaderFilterParams& params)
{
    alpha_ = params.alpha;
    bloom_ = params.bloom;
    color_ = params.color;
    mask_ = params.mask;
    useRawColor_ = params.useRawColor;

    if (!InitConvertFragShaderEffect() || !InitDetectFragShaderEffect() || !InitGaussShaderEffect() ||
        !InitCompositeShaderEffect() || !InitMaskShaderEffect() || !InitMergeImageShaderEffect()) {
        LOGE("GEEdgeLightShaderFilter::GEEdgeLightShaderFilter failed when initializing Effect.");
        return;
    }
    g_color = params.color;
}

std::shared_ptr<Drawing::Image> GEEdgeLightShaderFilter::OnProcessImage(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect &src, const Drawing::Rect &dst)
{
    if (!IsInputImageValid(image)) {
        return nullptr;
    }
    if (!IsShaderEffectInitValid()) {
        return image;
    }

    LOGD("GEEdgeLightShaderFilter::OnProcessImage input imageID:%{public}d cache imageID:%{public}d.",
        image->GetUniqueID(), g_originalImageID);

    if ((g_color != color_) || (g_originalImageID != image->GetUniqueID())) {
        auto linearImage = ConvertColorSpace(canvas, image, Drawing::ColorSpace::CreateSRGBLinear());
        if (linearImage == nullptr) {
            LOGE("GEEdgeLightShaderFilter::OnProcessImage Linearize make image failed.");
            return image;
        }
        auto detectImage = DetectEdge(canvas, linearImage);
        if (detectImage == nullptr) {
            LOGE("GEEdgeLightShaderFilter::OnProcessImage DetectEdge make image failed.");
            return image;
        }
        auto blurImage = GaussianBlur(canvas, detectImage);
        if (blurImage == nullptr) {
            LOGE("GEEdgeLightShaderFilter::OnProcessImage GaussianBlur make image failed.");
            return image;
        }
        g_color = color_;
        g_originalImageID = image->GetUniqueID();
        g_afterCompositeImage = blurImage;
    } else {
        LOGD("GEEdgeLightShaderFilter::OnProcessImage Blooming use cache image.");
    }

    auto mergeImage = MergeImage(canvas, image, g_afterCompositeImage);
    if (mergeImage == nullptr) {
        LOGE("GEEdgeLightShaderFilter::OnProcessImage MergeImage make image failed.");
        return image;
    }

    auto colorSpace = image->GetImageInfo().GetColorSpace();
    if (colorSpace == nullptr) {
        LOGE("GEEdgeLightShaderFilter::OnProcessImage image GetColorSpace failed.");
        return image;
    }
    // Don't delete!! Stay consistent with skia: SkColorSpaceXformSteps.
    if (colorSpace->Equals(std::make_shared<Drawing::ColorSpace>(Drawing::ColorSpace::ColorSpaceType::NO_TYPE))) {
        colorSpace = Drawing::ColorSpace::CreateSRGB();
    }

    auto retImage = ConvertColorSpace(canvas, mergeImage, colorSpace);
    if (retImage == nullptr) {
        LOGE("GEEdgeLightShaderFilter::OnProcessImage ConvertColorSpace failed.");
        return image;
    } else {
        return retImage;
    }
}

void GEEdgeLightShaderFilter::Preprocess(Drawing::Canvas& canvas, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    // Do tone mapping when enable edr effect
    if (GEToneMappingHelper::NeedToneMapping(supportHeadroom_)) {
        color_ = GEToneMappingHelper::GetBrightnessMapping(supportHeadroom_, color_);
    }
}

} // namespace Rosen
} // namespace OHOS
