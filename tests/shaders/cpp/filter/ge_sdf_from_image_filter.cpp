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

#include "ge_sdf_from_image_filter.h"

#include "ge_log.h"
#include "ge_system_properties.h"
#include "src/core/SkOpts.h"
#include "ge_mesa_blur_shader_filter.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace {
constexpr int MAX_SPREAD_FACTOR = 4096;
constexpr int TWO = 2;
} // namespace

static std::shared_ptr<Drawing::RuntimeEffect> g_JFAPrepareEffect;
static std::shared_ptr<Drawing::RuntimeEffect> g_jfaIterationEffect;
static std::shared_ptr<Drawing::RuntimeEffect> g_jfaProcessResultEffect;
static std::shared_ptr<Drawing::RuntimeEffect> g_sdfFillDerivEffect;

GESDFFromImageFilter::GESDFFromImageFilter(const Drawing::GESDFFromImageFilterParams& params)
    : spreadFactor_(params.spreadFactor), generateDerivs_(params.generateDerivs)
{
    if (!InitJFAPrepareEffect() || !InitJfaIterationEffect() || !InitJfaProcessResultEffect() ||
        !InitFillDerivEffect()) {
        GE_LOGE("GESDFFromImageFilter::GESDFFromImageFilter failed");
        return;
    }

    if (spreadFactor_ < 1) {
        GE_LOGD("GESDFFromImageFilter spreadFactor_(%{public}d) should be [1, 4096], changing to 0.",
            spreadFactor_);
        spreadFactor_ = 1;
    }

    if (spreadFactor_ > MAX_SPREAD_FACTOR) {
        GE_LOGD("GESDFFromImageFilter spreadFactor_(%{public}d) should be [1, 4096], changing to 4096.",
            spreadFactor_);
        spreadFactor_ = MAX_SPREAD_FACTOR;
    }
}

int GESDFFromImageFilter::GetSpreadFactor() const
{
    return spreadFactor_;
}

static thread_local std::shared_ptr<Drawing::RuntimeEffect> g_sampleShaderEffect = nullptr;
 
inline static const std::string BOX_BLUR_PROG = R"(
    uniform shader image;
    uniform float iScale;
    vec4 main(vec2 fragCoord)
    {
        float h = iScale * 0.5;
        float s1 = image.eval(fragCoord + vec2(h, h)).a;
        float s2 = image.eval(fragCoord - vec2(h, h)).a;
        float s3 = image.eval(fragCoord + vec2(h, -h)).a;
        float s4 = image.eval(fragCoord - vec2(h, -h)).a;
        return vec4(vec3(0.0), (s1 + s2 + s3 + s4) * 0.25);
    }
)";
 
std::shared_ptr<Drawing::Image> GESDFFromImageFilter::FakeBlur(Drawing::Canvas &canvas,
    const std::shared_ptr<Drawing::Image> edgeImage, const Drawing::SamplingOptions& samplingOptions)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::FakeBlur Run FakeBlur function.");
    if (!g_sampleShaderEffect) {
        g_sampleShaderEffect = Drawing::RuntimeEffect::CreateForShader(BOX_BLUR_PROG);
        if (g_sampleShaderEffect == nullptr) {
            GE_LOGE("GEEdgeLightShaderFilter::RuntimeShader g_gaussShaderEffect create failed.");
            return nullptr;
        }
    }
    constexpr float MIN_IMAGE_BLOOM_SIZE = 16.0; // < MIN_IMAGE_BLOOM_SIZE, not bloom, return image.
 
    // check image width and height in IsInputImageValid;
    auto imageInfo = edgeImage->GetImageInfo();
    imageInfo.SetColorType(Drawing::ColorType::COLORTYPE_RGBA_F16);
    float imageWidth = imageInfo.GetWidth();
    float imageHeight = imageInfo.GetHeight();
    if (imageWidth < MIN_IMAGE_BLOOM_SIZE || imageHeight < MIN_IMAGE_BLOOM_SIZE) {
        GE_LOGD("GEEdgeLightShaderFilter::FakeBlur image size is too small to bloom, return edgeImage. "
            "H:[%{public}f] W:[%{public}f]", imageHeight, imageWidth);
        return edgeImage;
    }
 
    auto sampleBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_sampleShaderEffect);
    auto srcImageShader = Drawing::ShaderEffect::CreateImageShader(*edgeImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, samplingOptions, Drawing::Matrix());
    if (srcImageShader == nullptr) {
        GE_LOGE("GESDFFromImageFilter::FakeBlur create srcImageShader failed.");
        return edgeImage;
    }

    sampleBuilder->SetChild("image", srcImageShader);
    sampleBuilder->SetUniform("iScale", 3.0f);
 
#ifdef RS_ENABLE_GPU
    auto blurImageH = sampleBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
#else
    auto blurImageH = sampleBuilder->MakeImage(nullptr, nullptr, imageInfo, false);
#endif
    if (blurImageH == nullptr) {
        GE_LOGE("GEEdgeLightShaderFilter::FakeBlur blurImageH make image failed.");
        return nullptr;
    }
    return blurImageH;
}

std::shared_ptr<Drawing::Image> GESDFFromImageFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::OnProcessImage Convert image to SDF once");
    if (!IsInputValid(canvas, image, src, dst)) {
        return image;
    }

    static const Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    static const Drawing::SamplingOptions nearest(Drawing::FilterMode::NEAREST, Drawing::MipmapMode::NONE);

    std::shared_ptr<Drawing::Image> initOutput = nullptr;
    initOutput = RunJFAPrepareEffect(canvas, image, linear, src, dst, Drawing::ColorType::COLORTYPE_RGBA_F16);
    if (!initOutput) {
        GE_LOGE("GESDFFromImageFilter::OnProcessImage Init make image error");
        return image;
    }

    std::shared_ptr<Drawing::Image> iterationsOutput = nullptr;
    iterationsOutput = RunJfaIterationsEffect(canvas, initOutput, nearest, Drawing::ColorType::COLORTYPE_RGBA_F16);
    if (!iterationsOutput) {
        GE_LOGE("GESDFFromImageFilter::OnProcessImage Iterations make image error");
        return image;
    }

    std::shared_ptr<Drawing::Image> sdfOutput = nullptr;
    sdfOutput = RunJfaProcessResultEffect(canvas, iterationsOutput, nearest, image->GetColorType());
    if (!sdfOutput) {
        GE_LOGE("GESDFFromImageFilter::OnProcessImage ProcessResult make image error");
        return image;
    }

    if (!generateDerivs_) {
        return sdfOutput;
    }

    // Downsample SDF
    auto blurredSdfImage = FakeBlur(canvas, sdfOutput, linear);
    if (!blurredSdfImage) {
        GE_LOGE("GESDFFromImageFilter::OnProcessImage FakeBlur make image error");
        return sdfOutput;
    }

    std::shared_ptr<Drawing::Image> derivOutput = nullptr;
    derivOutput = RunFillDerivEffect(canvas, sdfOutput, blurredSdfImage, nearest, image->GetColorType());
    if (!derivOutput) {
        GE_LOGE("GESDFFromImageFilter::OnProcessImage fillDeriv make image error");
        return sdfOutput;
    }
    return derivOutput;
}

bool GESDFFromImageFilter::InitJFAPrepareEffect()
{
    if (g_JFAPrepareEffect != nullptr) {
        return true;
    }

    static const std::string shaderString(R"(
        uniform shader imageInput;
        uniform float spreadFactor;
        const float SQRT_2 = 1.41421356;
        vec2 getGradient(vec2 fragCoord) {
            const vec2 h = vec2(0.5, 0.0);
            return vec2(imageInput.eval(fragCoord + h.xy).a - imageInput.eval(fragCoord - h.xy).a,
  			            imageInput.eval(fragCoord + h.yx).a - imageInput.eval(fragCoord - h.yx).a);
        }
        
        half4 main(vec2 fragCoord) {
            vec2 centerFragCoord = fragCoord + vec2(0.5);

            float imageSample = imageInput.eval(centerFragCoord).a;
          
          	if (imageSample <= 0.0) {
              return half4(1.0, 1.0, 0.5, 0.5);
            }
          	if (imageSample >= 1.0) {
              return half4(0.5, 0.5, 1.0, 1.0);
            }
          
          	vec2 grad = getGradient(centerFragCoord);
          	vec2 normGrad = normalize(grad);
          	vec2 edgeCoords = vec2(0.5);

          	float borderCoeff = (imageSample - 0.5) * SQRT_2; // determine how far the actual border is from sample value
          	edgeCoords -= normGrad * borderCoeff / (2.0 * spreadFactor);
          
          	if (borderCoeff < 0.0)
            {
              return half4(edgeCoords, 0.5, 0.5);
            }
            
            return half4(0.5, 0.5, edgeCoords);
        }
    )");

    g_JFAPrepareEffect = Drawing::RuntimeEffect::CreateForShader(shaderString);
    if (g_JFAPrepareEffect == nullptr) {
        GE_LOGE("GESDFFromImageFilter::InitJFAPrepareEffect failed");
        return false;
    }

    return true;
}

inline static const std::string JFA_ITERATION_PROG = R"(
    uniform vec2 iResolution;
    uniform float jfaRadius;
    uniform float spreadFactor;
    uniform shader imageInput;

    vec4 SafeFetch(vec2 fragCoord) {
        if (fragCoord.x < 0 || fragCoord.x > iResolution.x || fragCoord.y < 0 || fragCoord.y > iResolution.y) {
            return vec4(1e6);
        }
        return imageInput.eval(fragCoord);
    }

    vec2 EncodeCoords(vec2 coordsToEncode, vec2 fragCoord) {
        return clamp(((coordsToEncode - fragCoord) / spreadFactor + 1) / 2.0, 0, 1);
    }

    vec2 DecodeCoords(vec2 coordsToDecode, vec2 fragCoord) {
        return (coordsToDecode * 2 - 1) * spreadFactor + fragCoord;
    }
    
    void SearchNeighbors(inout vec4 O, vec2 fragCoord, vec2 sampleCoord)
    {
        vec4 imgSample = SafeFetch(sampleCoord);
        vec4 a = vec4(DecodeCoords(imgSample.xy, sampleCoord), DecodeCoords(imgSample.zw, sampleCoord));
        if (imgSample.x < 1.0 && imgSample.y < 1.0 && imgSample.x > 0.0 && imgSample.y > 0.0) {
            vec2 diffA = fragCoord - a.xy;
            vec2 diffO = O.xy - fragCoord;
            O.xy = dot(diffA, diffA) < dot(diffO, diffO) ? a.xy : O.xy;
        }
        if (imgSample.z < 1.0 && imgSample.w < 1.0 && imgSample.z > 0.0 && imgSample.w > 0.0) {
            vec2 diffA = fragCoord - a.zw;
            vec2 diffO = O.zw - fragCoord;
            O.zw = dot(diffA, diffA) < dot(diffO, diffO) ? a.zw : O.zw;
        }
    }

    half4 main(vec2 fragCoord) {
        vec4 O = imageInput.eval(fragCoord);
        O = vec4(DecodeCoords(O.xy, fragCoord), DecodeCoords(O.zw, fragCoord));
        SearchNeighbors(O, fragCoord, fragCoord + jfaRadius * vec2(0.0, -1.0));
        SearchNeighbors(O, fragCoord, fragCoord + jfaRadius * vec2(0.0, 1.0));
        SearchNeighbors(O, fragCoord, fragCoord + jfaRadius * vec2(-1.0, 0.0));
        SearchNeighbors(O, fragCoord, fragCoord + jfaRadius * vec2(1.0, 0.0));
        O = vec4(EncodeCoords(O.xy, fragCoord), EncodeCoords(O.zw, fragCoord));
        return O;
    }
)";

bool GESDFFromImageFilter::InitJfaIterationEffect()
{
    if (g_jfaIterationEffect != nullptr) {
        return true;
    }

    g_jfaIterationEffect = Drawing::RuntimeEffect::CreateForShader(JFA_ITERATION_PROG);
    if (g_jfaIterationEffect == nullptr) {
        GE_LOGE("GESDFFromImageFilter::InitJfaIterationEffect failed");
        return false;
    }

    return true;
}

bool GESDFFromImageFilter::InitJfaProcessResultEffect()
{
    if (g_jfaProcessResultEffect != nullptr) {
        return true;
    }

    static const std::string shaderString(R"(
        uniform shader imageInput;

        half4 main(vec2 fragCoord) {
        	half4 O = half4(0.0);

            vec4 imageSample = imageInput.eval(fragCoord);
        	vec2 xy = imageSample.xy * 2.0 - 1.0;
        	vec2 zw = imageSample.zw * 2.0 - 1.0;

        	O.a = length(xy) - length(zw);
        	O.a = (O.a + 1.0) / 2.0;
            O.a = clamp(O.a, 0.0, 1.0);
        	return O;
        }
    )");

    g_jfaProcessResultEffect = Drawing::RuntimeEffect::CreateForShader(shaderString);
    if (g_jfaProcessResultEffect == nullptr) {
        GE_LOGE("GESDFFromImageFilter::InitJfaProcessResultEffect failed");
        return false;
    }

    return true;
}

bool GESDFFromImageFilter::InitFillDerivEffect()
{
    if (g_sdfFillDerivEffect != nullptr || !generateDerivs_) {
        return true;
    }

    static const std::string shaderString(R"(
        uniform shader imageInput;
        uniform shader blurredSDFInput;
        const float width = 64.0;

        vec4 main(vec2 fragCoord) {
            float alpha = imageInput.eval(fragCoord).a;
            float sdfRaw = blurredSDFInput.eval(fragCoord).a;
            
            float d0 = sdfRaw * 2.0 - 1.0;
            float dist = abs(d0) * width;

            float h = 2.0 + clamp(dist * 0.2, 0.0, 8.0);
            
            float vR = blurredSDFInput.eval(fragCoord + vec2(h, 0)).a;
            float vL = blurredSDFInput.eval(fragCoord - vec2(h, 0)).a;
            float vB = blurredSDFInput.eval(fragCoord + vec2(0, h)).a;
            float vT = blurredSDFInput.eval(fragCoord - vec2(0, h)).a;

            vec2 grad = vec2(vR - vL, vB - vT);

            bool isInvalid = (sdfRaw > 0.95 || sdfRaw < 0.05) && (dot(grad, grad) < 0.0001);
            
            float spineSmooth = smoothstep(width, width * 0.2, dist);
            
            vec2 dir = isInvalid ? vec2(0.0) : (grad + 0.0001) * spineSmooth;

            vec4 O = vec4(0.0);
            O.x = clamp((dir.x + 1.0) / 2.0, 0.0, 1.0);
            O.y = clamp((dir.y + 1.0) / 2.0, 0.0, 1.0);
            O.w = clamp(alpha, 0.0, 1.0);
            return O;
        }
    )");

    g_sdfFillDerivEffect = Drawing::RuntimeEffect::CreateForShader(shaderString);
    if (g_sdfFillDerivEffect == nullptr) {
        GE_LOGE("GESDFFromImageFilter::InitFillDerivEffect failed");
        return false;
    }

    return true;
}

std::shared_ptr<Drawing::Image> GESDFFromImageFilter::RunJFAPrepareEffect(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::SamplingOptions& samplingOptions,
    const Drawing::Rect& src, const Drawing::Rect& dst, const Drawing::ColorType& outputColorType)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::RunJFAPrepareEffect Run JFA Prepare Function.");
    auto imageInfo = image->GetImageInfo();
    auto width = image->GetWidth();
    auto height = image->GetHeight();

    auto outputWidth = std::max(static_cast<int>(std::ceil(dst.GetWidth())), width);
    auto outputHeight = std::max(static_cast<int>(std::ceil(dst.GetHeight())), height);
    Drawing::Matrix stretchMatrix = BuildStretchMatrix(src, dst, width, height);
    auto outputImageInfo = Drawing::ImageInfo(
        outputWidth, outputHeight, outputColorType, imageInfo.GetAlphaType(), imageInfo.GetColorSpace());

    auto imageShader = Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, stretchMatrix);
    Drawing::RuntimeShaderBuilder JFAPrepareBuilder(g_JFAPrepareEffect);
    JFAPrepareBuilder.SetChild("imageInput", imageShader);
    JFAPrepareBuilder.SetUniform("spreadFactor", static_cast<float>(spreadFactor_));
#ifdef RS_ENABLE_GPU
    return JFAPrepareBuilder.MakeImage(canvas.GetGPUContext().get(), nullptr, outputImageInfo, false);
#else
    return JFAPrepareBuilder.MakeImage(nullptr, nullptr, outputImageInfo, false);
#endif
}

std::shared_ptr<Drawing::Image> GESDFFromImageFilter::RunJfaIterationsEffect(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::SamplingOptions& samplingOptions,
    const Drawing::ColorType& outputColorType)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::RunJfaIterationsEffect Run JFA Iteration Function.");
    Drawing::Matrix identityMatrix;

    auto outputImageInfo = image->GetImageInfo();
    outputImageInfo.SetColorType(outputColorType);

    std::shared_ptr<Drawing::Image> input = image;
    std::shared_ptr<Drawing::Image> output = nullptr;

    int jfaIterationCount = static_cast<int>(std::log2(spreadFactor_)) + 1;
    int jfaRadius = spreadFactor_;
    for (int jfaIteration = 0; jfaIteration < jfaIterationCount; ++jfaIteration) {
        Drawing::RuntimeShaderBuilder jfaIterationBuilder(g_jfaIterationEffect);
        auto imageInputShader = Drawing::ShaderEffect::CreateImageShader(
            *input, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, identityMatrix);
        jfaIterationBuilder.SetChild("imageInput", imageInputShader);
        jfaIterationBuilder.SetUniform("iResolution", input->GetWidth(), input->GetHeight());
        jfaIterationBuilder.SetUniform("jfaRadius", static_cast<float>(jfaRadius));
        jfaIterationBuilder.SetUniform("spreadFactor", static_cast<float>(spreadFactor_));
#ifdef RS_ENABLE_GPU
        output = jfaIterationBuilder.MakeImage(canvas.GetGPUContext().get(), nullptr, outputImageInfo, false);
#else
        output = jfaIterationBuilder.MakeImage(nullptr, nullptr, outputImageInfo, false);
#endif
        if (!output) {
            GE_LOGE("GESDFFromImageFilter::OnProcessImage Iteration make image error");
            return image;
        }
        jfaRadius = std::max(jfaRadius / TWO, 1);
        std::swap(input, output);
    }
    return input; // last output is swapped here at the end of cycle
}

std::shared_ptr<Drawing::Image> GESDFFromImageFilter::RunJfaProcessResultEffect(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::SamplingOptions& samplingOptions,
    const Drawing::ColorType& outputColorType)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::RunJfaProcessResultEffect Run JFA ProcessResult Function.");
    auto outputImageInfo = image->GetImageInfo();
    outputImageInfo.SetColorType(outputColorType);

    Drawing::Matrix identityMatrix;
    auto imageShader = Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, identityMatrix);
    Drawing::RuntimeShaderBuilder jfaProcessResultBuilder(g_jfaProcessResultEffect);
    jfaProcessResultBuilder.SetChild("imageInput", imageShader);
#ifdef RS_ENABLE_GPU
    return jfaProcessResultBuilder.MakeImage(canvas.GetGPUContext().get(), nullptr, outputImageInfo, false);
#else
    return jfaProcessResultBuilder.MakeImage(nullptr, nullptr, outputImageInfo, false);
#endif
}

std::shared_ptr<Drawing::Image> GESDFFromImageFilter::RunFillDerivEffect(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const std::shared_ptr<Drawing::Image> blurredSDFImage,
    const Drawing::SamplingOptions& samplingOptions, const Drawing::ColorType& outputColorType)
{
    GE_TRACE_NAME_FMT("GESDFFromImageFilter::RunFillDerivEffect Run Derivitive function.");
    auto outputImageInfo = image->GetImageInfo();
    outputImageInfo.SetColorType(outputColorType);

    Drawing::Matrix identityMatrix;
    auto imageInputShader = Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, identityMatrix);
    auto blurredSDFInputShader = Drawing::ShaderEffect::CreateImageShader(
        *blurredSDFImage, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, identityMatrix);
    Drawing::RuntimeShaderBuilder fillDerivBuilder(g_sdfFillDerivEffect);
    fillDerivBuilder.SetChild("imageInput", imageInputShader);
    fillDerivBuilder.SetChild("blurredSDFInput", blurredSDFInputShader);
#ifdef RS_ENABLE_GPU
    return fillDerivBuilder.MakeImage(canvas.GetGPUContext().get(), nullptr, outputImageInfo, false);
#else
    return fillDerivBuilder.MakeImage(nullptr, nullptr, outputImageInfo, false);
#endif
}

bool GESDFFromImageFilter::IsInputValid(Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image>& image,
    const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (!image || !g_JFAPrepareEffect || !g_jfaIterationEffect || !g_jfaProcessResultEffect ||
        (!g_sdfFillDerivEffect && generateDerivs_)) {
        GE_LOGE("GESDFFromImageFilter::IsInputValid invalid shader or image");
        return false;
    }
    return true;
}

Drawing::Matrix GESDFFromImageFilter::BuildStretchMatrix(
    const Drawing::Rect& src, const Drawing::Rect& dst, int imageWidth, int imageHeight) const
{
    Drawing::Matrix matrix;
    float scaleW = static_cast<float>((dst.GetWidth())) / (imageWidth > 0 ? imageWidth : 1);
    float scaleH = static_cast<float>((dst.GetHeight())) / (imageHeight > 0 ? imageHeight : 1);
    matrix.Translate(-src.GetLeft(), -src.GetTop());
    matrix.PostScale(scaleW, scaleH);

    Drawing::Matrix translateMatrix;
    translateMatrix.Translate(dst.GetLeft(), dst.GetTop());
    matrix.PostConcat(translateMatrix);

    return matrix;
}
} // namespace Rosen
} // namespace OHOS
