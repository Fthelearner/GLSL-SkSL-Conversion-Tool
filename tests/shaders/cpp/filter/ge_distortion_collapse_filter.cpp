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

#include "ge_distortion_collapse_filter.h"

#include "ge_log.h"

namespace OHOS::Rosen {
namespace {

static constexpr float EPSILON = 1e-6f;

constexpr char SHADER[] = R"(
    uniform shader imageShader;

    uniform vec2 imageSize;
    uniform vec2 invImageSize;

    uniform vec2 lu;
    uniform vec2 e;
    uniform vec2 f;
    uniform vec2 g;

    uniform float k2;
    uniform float ik2;
    uniform float k1Base;
    uniform float k0Base;

    uniform vec4 barrelDistortion;
    uniform float distortionEnabled;

    float Cross2d(vec2 a, vec2 b)
    {
        return a.x * b.y - a.y * b.x;
    }

    vec2 InvBilinear(vec2 p)
    {
        vec2 res = vec2(-1.0);

        vec2 h = p - lu;

        float localK1 = k1Base + Cross2d(p, g);
        float localK0 = k0Base + Cross2d(p, e);

        if (abs(k2) < 0.001) {
            res = vec2((h.x * localK1 + f.x * localK0) / (e.x * localK1 - g.x * localK0), -localK0 / localK1);
        } else {
            float w = localK1 * localK1 - 4.0 * localK0 * k2;
            if (w < 0.0) {
                return vec2(-1.0);
            }

            w = sqrt(w);

            float v = (-localK1 - w) * ik2;
            float u = (h.x - f.x * v) / (e.x + g.x * v);

            if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
                v = (-localK1 + w) * ik2;
                u = (h.x - f.x * v) / (e.x + g.x * v);
            }

            res = vec2(u, v);
        }

        return res;
    }

    half4 main(vec2 fragCoord)
    {
        vec2 uv = fragCoord * invImageSize;
        vec2 newUV = InvBilinear(uv);

        if (distortionEnabled > 0.0) {
            vec2 lerpDistortion = vec2(mix(barrelDistortion.x, barrelDistortion.y, newUV.x),
                                       mix(barrelDistortion.z, barrelDistortion.w, newUV.y));
            vec2 centerNewUV = newUV - vec2(0.5);
            vec2 normFactor = 1.0 / (1.0 + lerpDistortion * 0.5); // keeps the corners aligned with the input parameters
            float l2 = dot(centerNewUV, centerNewUV);
            centerNewUV *= 1.0 + lerpDistortion * l2;
            centerNewUV *= normFactor;
            newUV = centerNewUV + vec2(0.5);
        }

        // imageShader is created with TileMode::DECAL, so out-of-range sampling
        // will naturally return transparent and no extra bounds check is required.
        return imageShader.eval(newUV * imageSize);
    }
)";
} // namespace

GEDistortionCollapseFilter::GEDistortionCollapseFilter(const Drawing::GEDistortionCollapseFilterParams& params)
    : LUCorner_(params.LUCorner_),
      RUCorner_(params.RUCorner_),
      RBCorner_(params.RBCorner_),
      LBCorner_(params.LBCorner_),
      barrelDistortion_(params.barrelDistortion_)
{}

std::shared_ptr<Drawing::Image> GEDistortionCollapseFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (!image) {
        LOGE("GEDistortionCollapseFilter::OnProcessImage image is null");
        return image;
    }

    auto builder = MakeEffectShader(image, src, dst);
    if (!builder) {
        LOGE("GEDistortionCollapseFilter::OnProcessImage builder is null");
        return image;
    }

    auto outputImageInfo = image->GetImageInfo();

    Drawing::Matrix matrix;
    matrix.PreScale(src.GetWidth() / dst.GetWidth(), src.GetHeight() / dst.GetHeight());
    matrix.PreTranslate(src.GetLeft() - dst.GetLeft(), src.GetTop() - dst.GetTop());

    outputImageInfo.SetWidth(dst.GetWidth());
    outputImageInfo.SetHeight(dst.GetHeight());

    auto resultImage = builder->MakeImage(canvas.GetGPUContext().get(), &(matrix), image->GetImageInfo(), false);
    if (!resultImage) {
        LOGE("GEDistortionCollapseFilter::OnProcessImage resultImage is null");
        return image;
    }

    return resultImage;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEDistortionCollapseFilter::MakeEffectShader(
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    static auto effectShader = Drawing::RuntimeEffect::CreateForShader(SHADER);
    if (!effectShader) {
        LOGE("MakeEffectShader::RuntimeShader effect error\n");
        return nullptr;
    }

    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(effectShader);

    Drawing::Matrix matrix;
    const Drawing::SamplingOptions sampling(Drawing::FilterMode::LINEAR);
    builder->SetChild("imageShader", Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::DECAL, Drawing::TileMode::DECAL, sampling, matrix));

    const float imageWidth = src.GetWidth();
    const float imageHeight = src.GetHeight();
    builder->SetUniform("imageSize", imageWidth, imageHeight);
    builder->SetUniform("invImageSize", 1.0f / std::max(imageWidth, EPSILON), 1.0f / std::max(imageHeight, EPSILON));

    ComputeAndSetUniforms(builder);

    return builder;
}

void GEDistortionCollapseFilter::ComputeAndSetUniforms(const std::shared_ptr<Drawing::RuntimeShaderBuilder>& builder)
{
    const float luX = LUCorner_.GetX();
    const float luY = LUCorner_.GetY();
    const float ruX = RUCorner_.GetX();
    const float ruY = RUCorner_.GetY();
    const float rbX = RBCorner_.GetX();
    const float rbY = RBCorner_.GetY();
    const float lbX = LBCorner_.GetX();
    const float lbY = LBCorner_.GetY();

    auto Cross2d = [](float ax, float ay, float bx, float by) -> float {
        return ax * by - ay * bx;
    };

    // Precompute constants used by inverse bilinear mapping.
    const float ex = ruX - luX;
    const float ey = ruY - luY;
    const float fx = lbX - luX;
    const float fy = lbY - luY;
    const float gx = luX - ruX + rbX - lbX;
    const float gy = luY - ruY + rbY - lbY;

    const float k2 = Cross2d(gx, gy, fx, fy);
    const float safeK2 = (std::abs(k2) > EPSILON) ? k2 : ((k2 >= 0.0f) ? EPSILON : -EPSILON);
    const float ik2 = 0.5f / safeK2; // Keep the original formula v = (-k1 ± w) * 0.5 / k2
    const float k1Base = Cross2d(ex, ey, fx, fy) - Cross2d(luX, luY, gx, gy);
    const float k0Base = -Cross2d(luX, luY, ex, ey);

    builder->SetUniform("lu", luX, luY);
    builder->SetUniform("e", ex, ey);
    builder->SetUniform("f", fx, fy);
    builder->SetUniform("g", gx, gy);
    builder->SetUniform("k2", k2);
    builder->SetUniform("ik2", ik2);
    builder->SetUniform("k1Base", k1Base);
    builder->SetUniform("k0Base", k0Base);

    const int barrelDistortionSize = 4;
    float barrelDistortion[] = {barrelDistortion_[0], barrelDistortion_[1], barrelDistortion_[2], barrelDistortion_[3]};
    builder->SetUniform("barrelDistortion", barrelDistortion, barrelDistortionSize);

    const float distortionEnabled = (std::abs(barrelDistortion_[0]) > EPSILON ||
        std::abs(barrelDistortion_[1]) > EPSILON || std::abs(barrelDistortion_[2]) > EPSILON ||
        std::abs(barrelDistortion_[3]) > EPSILON) ? 1.0f : 0.0f;
    builder->SetUniform("distortionEnabled", distortionEnabled);
}
} // namespace OHOS::Rosen