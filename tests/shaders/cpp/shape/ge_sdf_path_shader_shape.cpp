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

#include "ge_sdf_path_shader_shape.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stack>
#include <string>
#include <vector>

#include "common/rs_common_def.h"
#include "draw/surface.h"
#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

namespace {
static constexpr float MIN_SCALE = 200.0f;
static constexpr uint32_t LINE = 2;
static constexpr uint32_t QUADRATIC_BEZIER = 3;
static constexpr uint32_t CUBIC_BEZIER = 4;
constexpr uint32_t MAX_CURVES_PER_GRID = 4;
constexpr uint32_t MAX_CURVES_SUBMIT_PER_GRID = 20;
constexpr uint32_t MIN_GRID_SIZE = 64;
constexpr uint32_t CURVE_FLOAT_COUNT = 6;
constexpr uint32_t XMIN_I = 0;
constexpr uint32_t XMAX_I = 1;
constexpr uint32_t YMIN_I = 2;
constexpr uint32_t YMAX_I = 3;
constexpr Drawing::ColorType RGBA_F16 = Drawing::ColorType::COLORTYPE_RGBA_F16;
constexpr bool NOT_BUDGETED = false;
constexpr float DEFAULT_BASE_WIDTH = 150.0f;
constexpr float DEFAULT_BASE_HEIGHT = 250.0f;
constexpr float MAX_THICKNESS = 0.05f;
constexpr float POWER_BASE = 2.0f;
constexpr float MIN_SCALE_CLAMP = 0.001f;
constexpr float NDC_MULTIPLIER = 2.0f;      // multiplier for NDC coordinate conversion
constexpr float NDC_OFFSET = 1.0f;          // offset for NDC coordinate conversion
constexpr float ASPECT_DEFAULT = 1.0f;      // default aspect ratio when width/height is zero
constexpr float MIDPOINT_FACTOR = 0.5f;     // factor for calculating midpoint
constexpr float INVALID_CONTROL_POINT = -2.0f; // not included in the NDC space
constexpr float INVALID_SEGMENT_INDEX = -1.0f; // invalid negative curve index

bool IntersectBBox(const Box4f& a, const Box4f& b)
{
    return !(a[XMAX_I] < b[XMIN_I] || a[XMIN_I] > b[XMAX_I] || a[YMAX_I] < b[YMIN_I] || a[YMIN_I] > b[YMAX_I]);
}

std::vector<Vector2f> ConvertPixelToNDC(const std::vector<Vector2f>& pixelPoints, float baseWidth = DEFAULT_BASE_WIDTH,
    float baseHeight = DEFAULT_BASE_HEIGHT)
{
    if (baseWidth < 1.0f || baseHeight < 1.0f) { // 1.0f is minimum pixel size
        return {};
    }
    std::vector<Vector2f> ndcPoints;
    ndcPoints.reserve(pixelPoints.size());
    for (const auto& p : pixelPoints) {
        float normalizedX = p[0] / baseWidth;
        float normalizedY = p[1] / baseHeight; // 1 is index of y in point
        float ndcX = (normalizedX * NDC_MULTIPLIER - NDC_OFFSET) * (baseWidth / baseHeight);
        float ndcY = normalizedY * NDC_MULTIPLIER - NDC_OFFSET;
        ndcPoints.emplace_back(ndcX, ndcY);
    }
    return ndcPoints;
}

void ConvertPointsToFloats(const std::vector<Vector2f>& in, std::vector<float>& out)
{
    out.clear();
    for (auto& p : in) {
        out.push_back(p[0]);
        out.push_back(p[1]);
    }
}

using AddCurveSegment =
    std::function<void(std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers, Vector2f& currentPos)>;
static std::unordered_map<char, AddCurveSegment> addCurveSegmentMap_ = {
    { 'M', // M is the start of a path
        [](std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers, Vector2f& currentPos) {
            if (numbers.size() < 2) { // 2: the number size of one point
                LOGE("AddCurveSegment: M command requires 2 numbers");
                return;
            }
            currentPos = Vector2f(numbers[0], numbers[1]);
        } },
    { 'L', // L is liner segment
        [](std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers, Vector2f& currentPos) {
            if (numbers.size() < 2) { // 2: the number size of liner
                LOGE("AddCurveSegment: L command requires 2 numbers");
                return;
            }
            std::vector<Vector2f> segment;
            segment.push_back(currentPos);             // start point of curve (end point of the previous curve)
            Vector2f end = { numbers[0], numbers[1] }; // end point
            segment.push_back(end);
            currentPos = end;
            result.push_back(segment);
        } },
    { 'Q', // Q is quadratic bezier curve
        [](std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers, Vector2f& currentPos) {
            if (numbers.size() < 4) { // 4: the number size of quadratic bezier curve
                LOGE("AddCurveSegment: Q command requires 4 numbers");
                return;
            }
            std::vector<Vector2f> segment;
            segment.push_back(currentPos);                 // start point of curve (end point of the previous curve)
            segment.push_back({ numbers[0], numbers[1] }); // control point
            Vector2f end = { numbers[2], numbers[3] };     // end point
            segment.push_back(end);
            currentPos = end;
            result.push_back(segment);
        } },
    { 'C', // C is cubic bezier cuerve
        [](std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers, Vector2f& currentPos) {
            if (numbers.size() < 6) { // 6: the number size of cubic bezier cuerve
                LOGE("AddCurveSegment: C command requires 6 numbers");
                return;
            }
            std::vector<Vector2f> segment;
            segment.push_back(currentPos);                 // start point of curve (end point of the previous curve)
            segment.push_back({ numbers[0], numbers[1] }); // first control point
            segment.push_back({ numbers[2], numbers[3] }); // second control point
            Vector2f end = { numbers[4], numbers[5] };     // end point
            segment.push_back(end);
            currentPos = end;
            result.push_back(segment);
        } },
    { 'Z', // Z M is the end of a path
        [](std::vector<std::vector<Vector2f>>& result, std::vector<float>& numbers,
               Vector2f& currentPos) { LOGI("AddCurveSegment: Z a path ends"); } }
};
} // namespace

static const std::string SDF_PROPAGATION_SHADER = R"(
    uniform vec2 iResolution;
    uniform float u_step;
    uniform shader u_sdfTex;
    uniform shader u_maskTex;

    const float INF_1E4 = 1e4;
    const float EPSILON_1E_MINUS_3 = 0.001;

    float sampleSdf(vec2 baseUv, vec2 offset) {
        vec2 sampleUv = baseUv + offset / iResolution.xy;
        float sdf = u_sdfTex.eval(sampleUv * iResolution).r;
        if (sdf < INF_1E4 - EPSILON_1E_MINUS_3) {
            float ndcOffset = length(offset) * (2.0 / iResolution.y);
            return sdf + ndcOffset;
        }
        return INF_1E4;
    }

    vec4 main(vec2 fragCoord) {
        vec2 uv = fragCoord / iResolution.xy;

        float mask = u_maskTex.eval(fragCoord).r;
        float currentSdf = u_sdfTex.eval(fragCoord).r;
        if (mask < 1.0 || currentSdf < INF_1E4 - EPSILON_1E_MINUS_3) {
            return vec4(vec3(currentSdf), 1.0);
        }

        float step = u_step;
        float minSdf = currentSdf;
        minSdf = min(minSdf, sampleSdf(uv, vec2(-step, -step)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(0.0, -step)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(step, -step)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(-step, 0.0)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(step, 0.0)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(-step, step)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(0.0, step)));
        minSdf = min(minSdf, sampleSdf(uv, vec2(step, step)));

        return vec4(minSdf);
    }
)";

static const std::string PRECALCULATION_FOR_SDF_SHADER = R"(
    uniform vec2 iResolution;
    const int CAPACITY = 60;
    const int MAX_CURVES_IN_GRID = 20;
    uniform float u_curveCount;
    uniform vec2 controlPoints[CAPACITY];
    uniform float segmentIndex[MAX_CURVES_IN_GRID];
    uniform shader u_prevD;
    uniform float u_isFirstBatch;

    const float INF_1E20 = 1e20;
    const float SQRT3 = 1.7320508;

    float sdf_bezier_unsigned_sq(vec2 pos, vec2 A, vec2 B, vec2 C) {
        const float EPSILON_1E_MINUS_3 = 1e-8;
        vec2 a = B - A;
        vec2 b = A - 2.0 * B + C;

        if (dot(b, b) < EPSILON_1E_MINUS_3) {
            vec2 pa = pos - A;
            vec2 ba = C - A;
            float ba2 = dot(ba, ba);
            if (ba2 < EPSILON_1E_MINUS_3) return dot(pa, pa);
            float h = clamp(dot(pa, ba) / ba2, 0.0, 1.0);
            vec2 dvec = pa - h * ba;
            return dot(dvec, dvec);
        }

        const float ONE_THIRD = 1.0 / 3.0;
        vec2 c = a * 2.0;
        vec2 d = A - pos;
        float kk = 1.0 / dot(b, b);
        float kx = kk * dot(a, b);
        float ky = kk * (2.0 * dot(a, a) + dot(d, b)) * ONE_THIRD;
        float kz = kk * dot(d, a);
        float res = 0.0;
        float p = ky - kx * kx;
        float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
        float h = q * q + 4.0 * p * p * p;

        if (h >= 0.0) {
            h = sqrt(h);
            vec2 x = 0.5 * (vec2(h, -h) - q);
            vec2 uv = sign(x) * pow(abs(x), vec2(ONE_THIRD));
            float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
            vec2 qv = d + (c + b * t) * t;
            res = dot(qv, qv);
        } else {
            float z = sqrt(-p);
            float v = acos(q / (p * z * 2.0)) * ONE_THIRD;
            float m = cos(v);
            float n = sin(v) * SQRT3;

            float t1 = clamp((m + m) * z - kx, 0.0, 1.0);
            float t2 = clamp((-n - m) * z - kx, 0.0, 1.0);

            vec2 q1 = d + (c + b * t1) * t1;
            float d1 = dot(q1, q1);
            vec2 q2 = d + (c + b * t2) * t2;
            float d2 = dot(q2, q2);

            res = min(d1, d2);
        }
        return res;
    }

    float FindClosestBezier(vec2 p, inout vec2 outA, inout vec2 outB, inout vec2 outC) {
        float minDist = INF_1E20;
        int curveCount = int(u_curveCount);

        for (int i = 0; i < MAX_CURVES_IN_GRID; i++) {
            int global_curve_idx = int(segmentIndex[i]);
            if (global_curve_idx < 0) break;

            vec2 A = controlPoints[i * 3];
            vec2 B = controlPoints[i * 3 + 1];
            vec2 C = controlPoints[i * 3 + 2];

            float d = sdf_bezier_unsigned_sq(p, A, B, C);
            if (d < minDist) {
                minDist = d;
            }
        }
        return minDist;
    }

    float get_sdf_shape(vec2 p) {
        vec2 A = vec2(0.0);
        vec2 B = vec2(0.0);
        vec2 C = vec2(0.0);
        float min_dist = FindClosestBezier(p, A, B, C);
        return sqrt(min_dist);
    }

    vec4 main(vec2 fragCoord) {
        vec2 uv = fragCoord / iResolution;
        float ndcAspect = iResolution.x / iResolution.y;
        vec2 ndc = vec2(
            (uv.x * 2.0 - 1.0) * ndcAspect,
            uv.y * 2.0 - 1.0
        );
        float currentD = get_sdf_shape(ndc);

        if (u_isFirstBatch < 1e-3) {
            float prevD = u_prevD.eval(fragCoord).r;
            currentD = min(currentD, prevD);
        }

        return vec4(vec3(currentD), 1.0);
    }
)";

static const std::string NORMAL_CALCULATION_SHADER = R"(
    uniform vec2 iResolution;
    uniform shader u_seeds;
    uniform shader pathShader;

    vec4 main(vec2 fragCoord) {
        float centerD = u_seeds.eval(fragCoord).r;

        float mask = pathShader.eval(fragCoord).r;
        float pixelDFactor = iResolution.y * 2.0;
        if (mask < 1e-5) {
            return vec4(0.0, 0.0, 0.0, centerD * pixelDFactor);
        }

        float L = u_seeds.eval(fragCoord + vec2(-1.0, 0.0)).r;
        float R = u_seeds.eval(fragCoord + vec2(1.0, 0.0)).r;
        float T = u_seeds.eval(fragCoord + vec2(0.0, -1.0)).r;
        float B = u_seeds.eval(fragCoord + vec2(0.0, 1.0)).r;

        vec2 normal = vec2(R - L, T - B) * 0.5;

        float len = length(normal);
        if (len > 0.00001) {
            normal /= len;
        } else {
            normal = vec2(0.0, 0.0);
        }

        centerD = (centerD + L + R + T + B) * 0.2 * pixelDFactor;

        float factor = 1.0 - smoothstep(10.0, 0.0, centerD);
        normal *= factor;
        float aaMargin = 3.0;

        return vec4(normal, 0.0, -centerD * mask * factor + aaMargin);
    }
)";

static const std::string CLEAR_INF_SHADER = R"(
        const float INF_1E4 = 1e4;
        vec4 main(vec2 fragCoord) {
            return vec4(vec3(INF_1E4), 1.0);
    }
)";

thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_jfaShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_distanceShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_pathSeedShaderEffect_ = nullptr;

thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_precalcShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_normalShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_sdfPropEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_clearInfEffect = nullptr;

static std::vector<float> parseNumbers(const std::string& s, size_t& i)
{
    std::vector<float> result;
    while (i < s.length()) {
        // Skip spaces
        while (i < s.length() && std::isspace(s[i])) {
            i++;
        }
        if (i >= s.length()) {
            break;
        }

        // check whether it is the next command (letter).
        char c = std::toupper(s[i]);
        if (std::isalpha(c) && !(i + 1 < s.length() && (s[i + 1] == '-' || std::isdigit(s[i + 1])))) {
            LOGD("next command: %{public}c", c);
            break;
        }

        // Parse a number
        size_t start = i;
        bool hasDigits = false;
        while (i < s.length() && (std::isdigit(s[i]) || s[i] == '-' || s[i] == '.')) {
            if (std::isdigit(s[i])) {
                hasDigits = true;
            }
            i++;
        }

        if (!hasDigits || i == start) {
            break;
        }

        // converting numbers
        double val = std::stod(s.substr(start, i - start));
        result.push_back(static_cast<float>(val));
    }
    return result;
}

std::vector<std::vector<Vector2f>> GESDFPathShaderShape::GetCurveByPath(const Drawing::Path& path)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::GetCurveByPath");
    std::vector<std::vector<Vector2f>> result = {};
    if (!path.IsValid()) {
        LOGE("GESDFPathShaderShape::GetCurveByPath: path is invalid.");
        return result;
    }
    std::string svgPath = path.ConvertToSVGString();
    Vector2f currentPos; // Curren position is

    size_t i = 0;
    while (i < svgPath.length()) {
        // Skip spaces
        while (i < svgPath.length() && std::isspace(svgPath[i])) {
            i++;
        }
        if (i >= svgPath.length()) {
            break;
        }

        char cmd = std::toupper(svgPath[i]);
        i++;
        // Read the corresponding number of digits according to the letters
        std::vector<float> numbers = parseNumbers(svgPath, i);
        auto it = addCurveSegmentMap_.find(cmd);
        if (it != addCurveSegmentMap_.end()) {
            it->second(result, numbers, currentPos);
        } else {
            LOGE("The %{public}c is not supported", cmd);
            continue;
        }
    }
    return result;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFPathShaderShape::MakePrecalcShaderBuilder()
{
    if (g_precalcShaderEffect_ == nullptr) {
        g_precalcShaderEffect_ = RuntimeEffect::CreateForShader(PRECALCULATION_FOR_SDF_SHADER);
        if (g_precalcShaderEffect_ == nullptr) {
            LOGE("GESDFPathShaderShape::MakePrecalcShaderBuilder failed to create effect");
            return nullptr;
        }
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(g_precalcShaderEffect_);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFPathShaderShape::MakeNormalShaderBuilder()
{
    if (g_normalShaderEffect_ == nullptr) {
        g_normalShaderEffect_ = RuntimeEffect::CreateForShader(NORMAL_CALCULATION_SHADER);
        if (g_normalShaderEffect_ == nullptr) {
            LOGE("GESDFPathShaderShape::MakeNormalShaderBuilder failed to create effect");
            return nullptr;
        }
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(g_normalShaderEffect_);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFPathShaderShape::MakeSdfPropShaderBuilder()
{
    if (g_sdfPropEffect_ == nullptr) {
        g_sdfPropEffect_ = RuntimeEffect::CreateForShader(SDF_PROPAGATION_SHADER);
        if (g_sdfPropEffect_ == nullptr) {
            LOGE("GESDFPathShaderShape::MakeSdfPropShaderBuilder failed to create effect");
            return nullptr;
        }
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(g_sdfPropEffect_);
}

std::shared_ptr<Image> GESDFPathShaderShape::RunSDFPropagation(
    Canvas& canvas, std::shared_ptr<Image> sdfTex, std::shared_ptr<Image> maskTex, int width, int height)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::RunSDFPropagation");

    if (numPasses_ <= 0) {
        LOGE("GESDFPathShaderShape::RunSDFPropagation skiped numPasses_<=0");
        return sdfTex;
    }

    auto gpuContext = canvas.GetGPUContext();
    if (!gpuContext) {
        LOGE("GESDFPathShaderShape::RunSDFPropagation no GPU context");
        return sdfTex;
    }

    SamplingOptions nearest(FilterMode::NEAREST, MipmapMode::NONE);
    auto outputImageInfo = sdfTex->GetImageInfo();

    auto builder = MakeSdfPropShaderBuilder();
    if (!builder) {
        LOGE("GESDFPathShaderShape::RunSDFPropagation pass builder failed");
        return sdfTex;
    }

    auto maskShader = ShaderEffect::CreateImageShader(*maskTex, TileMode::CLAMP, TileMode::CLAMP, nearest, Matrix());
    builder->SetChild("u_maskTex", maskShader);
    builder->SetUniform("iResolution", static_cast<float>(width), static_cast<float>(height));

    std::shared_ptr<Image> input = sdfTex;
    std::shared_ptr<Image> output = nullptr;
    for (size_t i = 0; i < numPasses_; i++) {
        int pixelStep = static_cast<int>(pow(POWER_BASE, numPasses_ - i - 1));
        auto inputSdfShader =
            ShaderEffect::CreateImageShader(*input, TileMode::CLAMP, TileMode::CLAMP, nearest, Matrix());
        builder->SetChild("u_sdfTex", inputSdfShader);
        builder->SetUniform("u_step", static_cast<float>(pixelStep));
        output = builder->MakeImage(gpuContext.get(), nullptr, outputImageInfo, false);
        if (!output) {
            LOGE("GESDFPathShaderShape::RunSDFPropagation pass %zu MakeImage failed", i);
            return input;
        }

        std::swap(input, output);
    }
    return input;
}

std::shared_ptr<Image> GESDFPathShaderShape::ComputeDistanceField(
    Canvas& canvas, std::shared_ptr<Image> sdfTex, int width, int height, std::shared_ptr<Image> pathImage)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::ComputeDistanceField");
    auto gpuContext = canvas.GetGPUContext();
    if (!gpuContext) {
        LOGE("GESDFPathShaderShape::ComputeDistanceField no GPU context");
        return nullptr;
    }

    SamplingOptions nearest(FilterMode::LINEAR, MipmapMode::LINEAR);
    auto inputImageInfo = sdfTex->GetImageInfo();

    auto normalBuilder = MakeNormalShaderBuilder();
    if (!normalBuilder) {
        LOGE("GESDFPathShaderShape::ComputeDistanceField effect create failed");
        return nullptr;
    }

    auto pathShader = ShaderEffect::CreateImageShader(*pathImage, TileMode::CLAMP,
        TileMode::CLAMP, nearest, Matrix());
    if (!pathShader) {
        LOGE("GESDFPathShaderShape::GenerateSeedTextureNew failed to create pathImage Shader");
        return nullptr;
    }

    auto inputShader = ShaderEffect::CreateImageShader(*sdfTex, TileMode::CLAMP, TileMode::CLAMP, nearest, Matrix());
    normalBuilder->SetChild("u_seeds", inputShader);
    normalBuilder->SetChild("pathShader", pathShader);
    normalBuilder->SetUniform("iResolution", static_cast<float>(width), static_cast<float>(height));
#ifdef RS_ENABLE_GPU
    return normalBuilder->MakeImage(gpuContext.get(), nullptr, inputImageInfo, false);
#else
    return normalBuilder->MakeImage(nullptr, nullptr, inputImageInfo, false);
#endif
}

void GESDFPathShaderShape::CreateSurfaceAndCanvas(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::CreateSurfaceAndCanvas");
    if (!rect.IsValid()) {
        GE_LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas rect is nullptr.");
        return;
    }
    Drawing::ImageInfo imageInfo(rect.GetWidth(), rect.GetHeight(), RGBA_F16, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    offscreenSurface_ = Drawing::Surface::MakeRenderTarget(canvas.GetGPUContext().get(), NOT_BUDGETED, imageInfo);
    if (!offscreenSurface_) {
        LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas offscreenSurface is invalid");
        return;
    }
    offscreenCanvas_ = offscreenSurface_->GetCanvas();
    if (!offscreenCanvas_) {
        LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas offscreenCanvas is invalid");
        return;
    }

    auto FallbackClear = [this, &rect]() {
        Drawing::Brush brush;
        brush.SetColor(0x00000000);
        offscreenCanvas_->AttachBrush(brush);
        offscreenCanvas_->DrawRect(rect);
        offscreenCanvas_->DetachBrush();
    };

    if (!g_clearInfEffect) {
        g_clearInfEffect = RuntimeEffect::CreateForShader(CLEAR_INF_SHADER);
        if (!g_clearInfEffect) {
            LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas CreateForShader failed");
            FallbackClear();
            return;
        }
    }

    auto builder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_clearInfEffect);
    if (!builder) {
        LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas create builder failed");
        FallbackClear();
        return;
    }

    auto clearShader = builder->MakeShader(nullptr, false);
    if (!clearShader) {
        LOGE("GESDFPathShaderShape::CreateSurfaceAndCanvas clear Shader failed");
        FallbackClear();
        return;
    }

    Drawing::Brush clearBrush;
    clearBrush.SetShaderEffect(clearShader);
    offscreenCanvas_->AttachBrush(clearBrush);
    offscreenCanvas_->DrawRect(rect);
    offscreenCanvas_->DetachBrush();
}

void GESDFPathShaderShape::AutoGridPartition(float width, float height, float maxThickness)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::AutoGridPartition");
    if (width < MIN_SCALE_CLAMP || height < MIN_SCALE_CLAMP) {
        GE_LOGE("GESDFPathShaderShape::AutoGridPartition surface size not valid");
        return;
    }
    curvesInGrid_.clear();

    std::vector<Box4f> curveBBoxes;
    Box4f canvasBBox;
    ComputeAllCurveBoundingBoxes(width, height, maxThickness, canvasBBox, curveBBoxes);

    std::queue<Grid> workQueue;
    InitializeWorkQueue(canvasBBox, curveBBoxes, workQueue);

    while (!workQueue.empty()) {
        Grid current = workQueue.front();
        workQueue.pop();
        float w = current.bbox[XMAX_I] - current.bbox[XMIN_I];
        float h = current.bbox[YMAX_I] - current.bbox[YMIN_I];
        bool needsSplit = (current.curveIndices.size() > MAX_CURVES_PER_GRID) &&
                          (w > MIN_GRID_SIZE && h > MIN_GRID_SIZE);
        if (needsSplit) {
            SplitGrid(current, curveBBoxes, workQueue, MIN_GRID_SIZE);
        } else {
            ProcessFinalGrid(current, curveBBoxes);
        }
    }
}

void GESDFPathShaderShape::SplitGrid(
    const Grid& current, const std::vector<Box4f>& curveBBoxes, std::queue<Grid>& workQueue, float minGridSize)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::SplitGrid");
    float midX = (current.bbox[XMIN_I] + current.bbox[XMAX_I]) * MIDPOINT_FACTOR;
    float midY = (current.bbox[YMIN_I] + current.bbox[YMAX_I]) * MIDPOINT_FACTOR;

    // Divide into four quadrants
    Box4f quadrants[4] = {
        { current.bbox[XMIN_I], midX, current.bbox[YMIN_I], midY }, // Top-Left
        { midX, current.bbox[XMAX_I], current.bbox[YMIN_I], midY }, // Top-Right
        { current.bbox[XMIN_I], midX, midY, current.bbox[YMAX_I] }, // Bottom-Left
        { midX, current.bbox[XMAX_I], midY, current.bbox[YMAX_I] }  // Bottom-Right
    };

    for (const auto& quad : quadrants) {
        Grid child { quad, {} };
        for (int idx : current.curveIndices) {
            if (IntersectBBox(child.bbox, curveBBoxes[idx])) {
                child.curveIndices.push_back(idx);
            }
        }
        if (!child.curveIndices.empty()) {
            workQueue.push(child);
        }
    }
}

void GESDFPathShaderShape::ComputeAllCurveBoundingBoxes(
    float width, float height, float maxThickness, Box4f& canvasBBox, std::vector<Box4f>& curveBBoxes)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::ComputeAllCurveBoundingBoxes");
    curveBBoxes.clear();
    curveBBoxes.reserve(numCurves_);
    canvasBBox = { width, 0.0f, height, 0.0f };
    for (size_t i = 0; i < numCurves_; ++i) {
        Box4f bbox = ComputeCurveBoundingBox(i, maxThickness, width, height);
        curveBBoxes.push_back(bbox);
        canvasBBox[XMIN_I] = std::min(bbox[XMIN_I], canvasBBox[XMIN_I]);
        canvasBBox[XMAX_I] = std::max(bbox[XMAX_I], canvasBBox[XMAX_I]);
        canvasBBox[YMIN_I] = std::min(bbox[YMIN_I], canvasBBox[YMIN_I]);
        canvasBBox[YMAX_I] = std::max(bbox[YMAX_I], canvasBBox[YMAX_I]);
    }
}

Box4f GESDFPathShaderShape::ComputeCurveBoundingBox(size_t curveIndex, float maxThickness, float width, float height)
{
    size_t baseIdx = curveIndex * 6;

    float x0 = controlPoints_[baseIdx];     // startPoint x
    float y0 = controlPoints_[baseIdx + 1]; // startPoint y
    float cx = controlPoints_[baseIdx + 2]; // controlPoint x
    float cy = controlPoints_[baseIdx + 3]; // controlPoint y
    float x1 = controlPoints_[baseIdx + 4]; // endPoint x
    float y1 = controlPoints_[baseIdx + 5]; // endPoint y
    float minX = std::min({ x0, cx, x1 }) - maxThickness;
    float maxX = std::max({ x0, cx, x1 }) + maxThickness;
    float minY = std::min({ y0, cy, y1 }) - maxThickness;
    float maxY = std::max({ y0, cy, y1 }) + maxThickness;
    // map ndc to [0, 1]
    float aspect;
    if (height < 0.001f || width < 0.001f) { // 0.001f is the minimum value
        aspect = ASPECT_DEFAULT;
    } else {
        aspect = width / height;
    }

    minX = std::floor((minX / aspect + NDC_OFFSET) / NDC_MULTIPLIER * width);
    maxX = std::ceil((maxX / aspect + NDC_OFFSET) / NDC_MULTIPLIER * width);
    minY = std::floor((minY + NDC_OFFSET) / NDC_MULTIPLIER * height);
    maxY = std::ceil((maxY + NDC_OFFSET) / NDC_MULTIPLIER * height);

    minX = std::max(minX, 0.0f);
    maxX = std::min(maxX, width);
    minY = std::max(minY, 0.0f);
    maxY = std::min(maxY, height);
    return { minX, maxX, minY, maxY };
}

void GESDFPathShaderShape::InitializeWorkQueue(
    const Box4f& canvasBBox, const std::vector<Box4f>& curveBBoxes, std::queue<Grid>& workQueue)
{
    std::vector<uint32_t> initialCurves;
    for (size_t i = 0; i < numCurves_; ++i) {
        if (IntersectBBox(canvasBBox, curveBBoxes[i])) {
            initialCurves.push_back(i);
        }
    }
    workQueue.push({ canvasBBox, initialCurves });
}

void GESDFPathShaderShape::ProcessFinalGrid(Grid& current, const std::vector<Box4f>& curveBBoxes)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::ProcessFinalGrid");
    Grid processedGrid = current;
    std::vector<float> gridCurves;
    std::vector<float> inOrderSeg;
    for (size_t idx : processedGrid.curveIndices) {
        size_t baseIdx = idx * CURVE_FLOAT_COUNT;
 
        gridCurves.push_back(controlPoints_[baseIdx]);     // startPoint.x
        gridCurves.push_back(controlPoints_[baseIdx + 1]); // 1: startPoint.y
        gridCurves.push_back(controlPoints_[baseIdx + 2]); // 2: controlPoint.x
        gridCurves.push_back(controlPoints_[baseIdx + 3]); // 3: controlPoint.y
        gridCurves.push_back(controlPoints_[baseIdx + 4]); // 4: endPoint.x
        gridCurves.push_back(controlPoints_[baseIdx + 5]); // 5: endPoint.y
        inOrderSeg.push_back(static_cast<float>(idx));
    }
    curvesInGrid_.push_back(std::make_pair(gridCurves, processedGrid));
    segmentIndex_.push_back(inOrderSeg);
}

Drawing::Path GESDFPathShaderShape::PreparePathForRendering(const Drawing::Rect& rect, float& width, float& height)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::PreparePathForRendering");
    UpdateScale(params_.scale, rect);
    width = rect.GetWidth() * params_.scale.x_;
    height = rect.GetHeight() * params_.scale.y_;

    Drawing::Matrix matrix;
    matrix.SetScale(params_.scale.x_, params_.scale.y_);
    Drawing::Path path = params_.path;
    path.Offset(params_.offset.x_, params_.offset.y_);
    path.Transform(matrix);
    return path;
}

std::vector<Vector2f> GESDFPathShaderShape::ProcessCurveSegments(const std::vector<std::vector<Vector2f>>& paramsCoef)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::ProcessCurveSegments");
    std::vector<Vector2f> pixelControlPoints;
    numCurves_ = 0;

    for (auto& segment : paramsCoef) {
        if (segment.size() == LINE) {
            pixelControlPoints.push_back(segment[0]);
            pixelControlPoints.push_back(segment[0]);
            pixelControlPoints.push_back(segment[1]); // 1 is index
            numCurves_++;
        } else if (segment.size() == QUADRATIC_BEZIER) {
            pixelControlPoints.push_back(segment[0]);
            pixelControlPoints.push_back(segment[1]); // 1 is index
            pixelControlPoints.push_back(segment[2]); // 2 is index of QUADRATIC BEZIER
            numCurves_++;
        } else if (segment.size() == CUBIC_BEZIER) {
            Vector2f controlPoint = segment[1]; // 1 is index
            // 0, 2, 3 is index of CUBIC BEZIER
            cubicToQuadraticSingle(segment[0], controlPoint, segment[2], segment[3]);
            pixelControlPoints.push_back(segment[0]);
            pixelControlPoints.push_back(controlPoint);
            pixelControlPoints.push_back(segment[3]); // 3 is index of CUBIC BEZIER
            numCurves_++;
        }
    }
    return pixelControlPoints;
}

void GESDFPathShaderShape::ProcessSingleBatch(Drawing::RuntimeShaderBuilder& builder, size_t gridIndex, size_t batch,
    std::shared_ptr<Drawing::Image>& prevSdf, std::shared_ptr<Drawing::ShaderEffect>& prevShader)
{
    const auto& grid = curvesInGrid_[gridIndex];
    const auto& allCurves = grid.first;
    const auto& allSegments = segmentIndex_[gridIndex];
    const Box4f area = grid.second.bbox;
    const Drawing::Rect rectN(area[XMIN_I], area[YMIN_I], area[XMAX_I], area[YMAX_I]);
    
    const size_t curvesPerBatch = MAX_CURVES_SUBMIT_PER_GRID;
    const size_t totalCurves = allCurves.size() / CURVE_FLOAT_COUNT;
    const size_t start = batch * curvesPerBatch;
    const size_t end = std::min(start + curvesPerBatch, totalCurves);

    std::vector<float> batchCurves;
    std::vector<float> batchSegments;
    batchCurves.reserve(curvesPerBatch * CURVE_FLOAT_COUNT);
    batchSegments.reserve(curvesPerBatch);
    for (size_t j = start; j < end; j++) {
        size_t base = j * CURVE_FLOAT_COUNT;
        batchCurves.insert(batchCurves.end(), allCurves.begin() + base, allCurves.begin() + base + CURVE_FLOAT_COUNT);
        batchSegments.push_back(allSegments[j]);
    }
    batchCurves.resize(curvesPerBatch * CURVE_FLOAT_COUNT, INVALID_CONTROL_POINT);
    batchSegments.resize(curvesPerBatch, INVALID_SEGMENT_INDEX);

    builder.SetUniform("controlPoints", batchCurves.data(), batchCurves.size());
    builder.SetUniform("segmentIndex", batchSegments.data(), batchSegments.size());
    builder.SetUniform("u_isFirstBatch", (batch == 0) ? 1.0f : 0.0f);
    // Note: u_prevD is not sampled when u_isFirstBatch == 1.0f, no need to set for batch 0

    SamplingOptions nearest(FilterMode::LINEAR, MipmapMode::LINEAR);
    if (batch > 0) {
        prevSdf = offscreenSurface_->GetImageSnapshot();
        if (!prevSdf) {
            LOGE("GESDFPathShaderShape::ProcessSingleBatch: failed to get image snapshot from offscreen surface");
            return;
        }
        prevShader = ShaderEffect::CreateImageShader(*prevSdf,
            TileMode::CLAMP, TileMode::CLAMP, nearest, Matrix());
        if (!prevShader) {
            LOGE("GESDFPathShaderShape::ProcessSingleBatch failed: failed to create image shader");
            return;
        }
        builder.SetChild("u_prevD", prevShader);
    }

    auto shader = builder.MakeShader(nullptr, false);
    if (!shader) {
        LOGE("GESDFPathShaderShape::ProcessSingleBatch shader create failed");
        return;
    }

    Drawing::Brush brush;
    brush.SetShaderEffect(shader);
    offscreenCanvas_->AttachBrush(brush);
    offscreenCanvas_->DrawRect(rectN);
    offscreenCanvas_->DetachBrush();
}

void GESDFPathShaderShape::RenderGridsToSurface(const Drawing::Rect& targetRect)
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::RenderGridsToSurface");
    auto builder = MakePrecalcShaderBuilder();
    if (!builder) {
        LOGE("GESDFPathShaderShape::RenderGridsToSurface builder create failed");
        return;
    }

    builder->SetUniform("iResolution", targetRect.GetWidth(), targetRect.GetHeight());
    builder->SetUniform("u_curveCount", static_cast<float>(numCurves_));
    SamplingOptions nearest(FilterMode::LINEAR, MipmapMode::LINEAR);
    auto prevSdf = offscreenSurface_->GetImageSnapshot();
    if (!prevSdf) {
            LOGE("GESDFPathShaderShape::RenderGridsToSurface: failed to get image snapshot from offscreen surface");
            return;
        }
    auto prevShader = ShaderEffect::CreateImageShader(*prevSdf,
        TileMode::CLAMP, TileMode::CLAMP, nearest, Matrix());
    for (size_t i = 0; i < curvesInGrid_.size(); i++) {
        if (curvesInGrid_[i].first.size() > 0) {
            const size_t curvesPerBatch = MAX_CURVES_SUBMIT_PER_GRID;
            const size_t totalCurves = curvesInGrid_[i].first.size() / CURVE_FLOAT_COUNT;
            const size_t numBatches = (totalCurves + curvesPerBatch - 1) / curvesPerBatch;

            for (size_t batch = 0; batch < numBatches; batch++) {
                ProcessSingleBatch(*builder, i, batch, prevSdf, prevShader);
            }
        }
    }
}

std::shared_ptr<Image> GESDFPathShaderShape::DrawPathToImage(
    Drawing::Canvas& canvas, int width, int height, const Drawing::Path& path)
{
    if (width <= 0 || height <= 0) {
        LOGE("GESDFPathShaderShape::DrawPathToImage error, width or height is invalid");
        return nullptr;
    }

    GE_TRACE_NAME_FMT("GESDFPathShaderShape::DrawPathToImage");
    Drawing::ImageInfo imageInfo = Drawing::ImageInfo { width, height, Drawing::ColorType::COLORTYPE_RGBA_F16,
        Drawing::AlphaType::ALPHATYPE_UNKNOWN };
    std::shared_ptr<Drawing::Surface> surface =
        Drawing::Surface::MakeRenderTarget(canvas.GetGPUContext().get(), false, imageInfo);
    if (!surface) {
        LOGE("Null surface");
        return nullptr;
    }
    auto canvas1 = surface->GetCanvas();
    if (canvas1 == nullptr) {
        return nullptr;
    }
    Drawing::Brush brush;
    brush.SetColor(Color::COLOR_RED);
    brush.SetAntiAlias(true);
    canvas1->AttachBrush(brush);
    canvas1->DrawPath(path);
    canvas1->DetachBrush();
    return surface->GetImageSnapshot();
}

void GESDFPathShaderShape::cubicToQuadraticSingle(
    const Vector2f& p0, Vector2f& p1, const Vector2f& p2, const Vector2f& p3)
{
    // Keep endpoint unchanged
    Vector2f q0 = p0;
    Vector2f q2 = p3;

    // Bezier curve coefficients for cubic to quadratic conversion
    const double c0 = 0.125;
    const double c1 = 0.375;
    const double c2 = 0.375;
    const double c3 = 0.125;
    Vector2f midCubic = { c0 * p0.x_ + c1 * p1.x_ + c2 * p2.x_ + c3 * p3.x_,
        c0 * p0.y_ + c1 * p1.y_ + c2 * p2.y_ + c3 * p3.y_ };

    const float weightDouble = 2.0f;
    const float weightHalf = 0.5f;
    Vector2f q1 = { weightDouble * midCubic.x_ - weightHalf * q0.x_ - weightHalf * q2.x_,
        weightDouble * midCubic.y_ - weightHalf * q0.y_ - weightHalf * q2.y_ };
    p1 = q1;
}

void GESDFPathShaderShape::UpdateScale(Vector2f& scale, const Drawing::Rect& rect)
{
    float scaleX = std::clamp(scale.x_, MIN_SCALE_CLAMP, 1.0f); // 1.0f: maximum scale
    float scaleY = std::clamp(scale.y_, MIN_SCALE_CLAMP, 1.0f); // 1.0f: maximum scale
    float width = rect.GetWidth() * scaleX;
    float height = rect.GetHeight() * scaleY;
    if (width < MIN_SCALE && width > 0.0f) {
        scaleX = MIN_SCALE / rect.GetWidth();
    }
    
    if (height < MIN_SCALE && height > 0.0f) {
        scaleY = MIN_SCALE / rect.GetHeight();
    }
    scale = Vector2f(scaleX, scaleY);
}

void GESDFPathShaderShape::Preprocess(Canvas& canvas, const Rect& rect, bool hasNormal)
{
    GE_TRACE_FUNC();
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::Preprocess");
    if (!params_.path.IsValid()) {
        LOGE("GESDFPathShaderShape::Preprocess: the paths is invalid.");
        return;
    }
    float width = 0.0f;
    float height = 0.0f;
    Drawing::Path path = PreparePathForRendering(rect, width, height);

    std::shared_ptr<Image> pathImage = DrawPathToImage(canvas, width, height, path);
    if (pathImage == nullptr) {
        LOGE("GESDFPathShaderShape::Preprocess: Failed to draw the paths");
        return;
    }

    std::vector<std::vector<Vector2f>> paramsCoef = GetCurveByPath(path);
    std::vector<Vector2f> pixelControlPoints = ProcessCurveSegments(paramsCoef);
    if (pixelControlPoints.empty()) {
        LOGE("GESDFPathShaderShape::Preprocess: no valid curves.");
        return;
    }
    // Convert Pixel To NDC
    std::vector<Vector2f> ndcControlPoints = ConvertPixelToNDC(pixelControlPoints, width, height);
    ConvertPointsToFloats(ndcControlPoints, controlPoints_);
    pointCnt_ = controlPoints_.size();

    AutoGridPartition(width, height, MAX_THICKNESS);
    if (curvesInGrid_.empty()) {
        LOGE("GESDFPathShaderShape::Preprocess: AutoGridPartition failed.");
        return;
    }

    Drawing::Rect targetRect(0, 0, width, height);
    CreateSurfaceAndCanvas(canvas, targetRect);
    if (!offscreenSurface_ || !offscreenCanvas_) {
        LOGE("GESDFPathShaderShape::PrePass create surface failed");
        return;
    }

    RenderGridsToSurface(targetRect);

    std::shared_ptr<Image> propagatedSdf = RunSDFPropagation(canvas, offscreenSurface_->GetImageSnapshot(),
        pathImage, static_cast<int>(width), static_cast<int>(height));

    disResult_ =
        ComputeDistanceField(canvas, propagatedSdf, static_cast<int>(width), static_cast<int>(height), pathImage);
    if (!disResult_) {
        LOGE("GESDFPathShaderShape::Preprocess ComputeDistanceField failed");
        return;
    }
}

std::shared_ptr<ShaderEffect> GESDFPathShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFPathShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    if (!disResult_) {
        LOGE("GESDFPathShaderShape::GenerateDrawingShader no sdfResult_, call PrePass first");
        return nullptr;
    }
    auto inputImageInfo = disResult_->GetImageInfo();
    float inputHeight = inputImageInfo.GetHeight();
    float inputWidth = inputImageInfo.GetWidth();
    // upScale to target resolution
    // check for division by zero
    if (inputWidth < 1.0f || inputHeight < 1.0f) { // 1.0f is minimum pixel size
        LOGE("GESDFPathShaderShape::GenerateDrawingShader invalid input size");
        return nullptr;
    }
    float scaleX = width / inputWidth;
    float scaleY = height / inputHeight;
    Drawing::Matrix upscaleMatrix;
    upscaleMatrix.SetScale(scaleX, scaleY);
    SamplingOptions sampling(FilterMode::LINEAR, MipmapMode::LINEAR);
    return ShaderEffect::CreateImageShader(*disResult_, TileMode::CLAMP, TileMode::CLAMP, sampling, upscaleMatrix);
}

std::shared_ptr<ShaderEffect> GESDFPathShaderShape::GenerateDrawingShaderHasNormal(float width, float height) const
{
    return GenerateDrawingShader(width, height);
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS
