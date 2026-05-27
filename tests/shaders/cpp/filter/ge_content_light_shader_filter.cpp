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

#include "ge_content_light_shader_filter.h"
#include "ge_log.h"

namespace OHOS {
namespace Rosen {

constexpr size_t NUM_0 = 0;
constexpr size_t NUM_1 = 1;
constexpr size_t NUM_2 = 2;
constexpr size_t NUM_3 = 3;
constexpr size_t NUM_4 = 4;
constexpr float MIN_LIGHT_POSITION = -10.0f;
constexpr float MAX_LIGHT_POSITION = 10.0f;

std::shared_ptr<Drawing::RuntimeEffect> GEContentLightFilter::contentLightShaderEffect_ = nullptr;

GEContentLightFilter::GEContentLightFilter(const Drawing::GEContentLightFilterParams& params)
    : lightPosition_(params.position), lightColor_(params.color), lightIntensity_(params.intensity),
    rotationAngle_(params.rotationAngle)
{}

std::shared_ptr<Drawing::Image> GEContentLightFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEContentLightFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    Drawing::Matrix matrix;
    auto shader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    auto imageInfo = image->GetImageInfo();
    float height = imageInfo.GetHeight();
    float width = imageInfo.GetWidth();
    if (height < 1e-6 || width < 1e-6) {
        return nullptr;
    }
    ClampLightValue();
    GenerateContentLightEffect();

    float lightColor[NUM_4] = {lightColor_[NUM_0], lightColor_[NUM_1], lightColor_[NUM_2], lightColor_[NUM_3]};
    Drawing::RuntimeShaderBuilder builder(contentLightShaderEffect_);
    builder.SetChild("image", shader);
    builder.SetUniform("iResolution", width, height);
    builder.SetUniform("lightIntensity", lightIntensity_);
    builder.SetUniform("lightPosition", lightPosition_[NUM_0], lightPosition_[NUM_1], lightPosition_[NUM_2]);
    builder.SetUniform("lightColor", lightColor, NUM_4);
    builder.SetUniform("contentRotationAngle", rotationAngle_[NUM_0], rotationAngle_[NUM_1], rotationAngle_[NUM_2]);
#ifdef RS_ENABLE_GPU
    auto invertedImage = builder.MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
#else
    auto invertedImage = builder.MakeImage(nullptr, nullptr, imageInfo, false);
#endif
    if (invertedImage == nullptr) {
        LOGE("GEContentLightFilter::OnProcessImage make image failed");
        return nullptr;
    }
    return invertedImage;
}

float GEContentLightFilter::ClampValue(float x, float minValue, float maxValue)
{
    return std::min(std::max(x, minValue), maxValue);
}

void GEContentLightFilter::ClampLightValue()
{
    for (size_t idx = 0; idx < NUM_3; idx ++) {
        lightPosition_[idx] = ClampValue(lightPosition_[idx], MIN_LIGHT_POSITION, MAX_LIGHT_POSITION);
    }
    for (size_t idx = 0; idx < NUM_4; idx ++) {
        lightColor_[idx] = ClampValue(lightColor_[idx], 0.0f, 1.0f);
    }
    lightIntensity_ = ClampValue(lightIntensity_, 0.0f, 1.0f);
}

void GEContentLightFilter::GenerateContentLightEffect()
{
    const std::string shaderStringContentLight = R"(
        uniform shader image;
        uniform half2 iResolution;
        uniform half lightIntensity;
        uniform half3 lightPosition;
        uniform half4 lightColor;
        uniform half3 contentRotationAngle;

        half sdRoundedBox(half2 p, half2 b, half r)
        {
            half2 q = abs(p) - b + r;
            return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
        }

        half4 shinningEffect(in half3 fragPos, in half3 normal, in half3 lightPos, in half3 viewPos,
            in half4 specularColor, in half shinning)
        {
            half3 lightDir = normalize(lightPos - fragPos);
            half3 viewDir = normalize(viewPos - fragPos);
            half3 halfwayDir = normalize(lightDir + viewDir);

            // Blinn-Phong
            half4 specularC = specularColor * pow(max(dot(normal, halfwayDir), 0.), shinning);
            return specularC;
        }

        half4 createContentNormal(half2 pos, half maskAlpha) // pos in [-w, w] x [-1, 1]
        {
            if (maskAlpha < 0.01) {
                return half4(0.0, 0.0, -1.0, -1.0);
            }

            half R = max(iResolution.x, iResolution.y) * 4. / iResolution.y;
            half z = sqrt(R * R - pos.x * pos.x - pos.y * pos.y);
            half3 normal = normalize(half3(pos.x / z, pos.y / z, 0.5));

            return half4(normal, 1.);
        }

        mat3 GetRotationMatrix(half3 rotAngle)
        {
            rotAngle *= 3.1415926 / 180.0;
            mat3 Rx = mat3 (1.0, 0.0, 0.0,
                            0.0, cos(rotAngle.x), -sin(rotAngle.x),
                            0.0, sin(rotAngle.x), cos(rotAngle.x));
            mat3 Ry = mat3 (cos(rotAngle.y), 0.0, sin(rotAngle.y),
                            0.0, 1.0, 0.0,
                            -sin(rotAngle.y), 0.0, cos(rotAngle.y));
            mat3 Rz = mat3 (cos(rotAngle.z), -sin(rotAngle.z), 0.0,
                            sin(rotAngle.z), cos(rotAngle.z), 0.0,
                            0.0, 0.0, 1.0);
            return Rz * Ry * Rx;
        }

        half4 ContentShinning(
            half2 uv, half4 specularColor, half shinning, half3 lightPos, half3 viewPos, mat3 rotM, half maskAlpha)
        {
            half3 fragPos = half3(uv, 0.0);
            half4 normal = createContentNormal(uv, maskAlpha);
            if (normal.w < 0.0) {
                return half4(0.0);
            }
            half3 fragNormal = rotM * normal.xyz;

            half4 shinningColor = shinningEffect(fragPos, fragNormal, lightPos, viewPos, specularColor, shinning);
            return shinningColor;
        }

        half4 main(in vec2 fragCoord)
        {
            half4 inputImage = image.eval(fragCoord);
            if (inputImage.w > 0.0) {
                inputImage.rgb /= inputImage.w;
            } else {
                return inputImage;
            }
            half2 uv = fragCoord / iResolution.xy;
            uv = uv + uv - 1.0;
            half screenRatio = iResolution.x / iResolution.y;
            uv.x *= screenRatio;

            half4 shinningColor = half4(0.0);
            half3 lightPos = half3(lightPosition.x * screenRatio, lightPosition.y, lightPosition.z);

            mat3 rotM = GetRotationMatrix(contentRotationAngle);
            half4 specularColor = lightColor;
            half shinning = 36.0;
            half3 viewPos = lightPos;
            shinningColor =
                ContentShinning(uv, specularColor, shinning, lightPos, viewPos, rotM, inputImage.w);

            half intensity = clamp(lightIntensity, 0.0, 1.0) * lightColor.a;
            shinningColor.rgb *= intensity;
            shinningColor.rgb += inputImage.rgb - inputImage.rgb * shinningColor.rgb * 0.85;

            return half4(shinningColor.rgb * inputImage.w, inputImage.w);
        }
    )";
    if (contentLightShaderEffect_ == nullptr) {
        contentLightShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(shaderStringContentLight);
    }
}
} // namespace Rosen
} // namespace OHOS