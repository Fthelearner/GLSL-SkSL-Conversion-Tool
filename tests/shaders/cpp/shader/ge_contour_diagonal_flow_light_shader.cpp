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

#include "ge_contour_diagonal_flow_light_shader.h"

#include <algorithm>
#include <array>
#ifdef GE_OHOS
#include <charconv>
#include <cstdlib>
#endif
#include <iomanip>
#include <sstream>
#include <string>

#include "common/rs_vector2.h"
#include "draw/surface.h"
#include "ge_kawase_blur_shader_filter.h"
#include "ge_mesa_blur_shader_filter.h"
#include "ge_log.h"
#ifdef GE_OHOS
#include "ge_system_properties.h"
#endif
#include "ge_visual_effect_impl.h"

#ifdef USE_M133_SKIA
#include "src/core/SkChecksum.h"
#else
#include "src/core/SkOpts.h"
#endif

namespace OHOS {
namespace Rosen {

#ifdef GE_OHOS
int ConvertToInt(const char* originValue, int defaultValue)
{
    if (originValue == nullptr) {
        return defaultValue;
    }
    int value = defaultValue;
    auto result = std::from_chars(originValue, originValue + std::strlen(originValue), value);
    if (result.ec == std::errc()) {
        return value;
    } else {
        return defaultValue;
    }
}
#endif

bool GetSwitchMESABlurEnable()
{
#ifdef GE_OHOS
    static CachedHandle handle = CachedParameterCreate("persist.graphic.contourdiagnalmesa.enabled", "1");
    int32_t changed = 0;
    return ConvertToInt(CachedParameterGetChanged(handle, &changed), 1) != 0;
#else
    return true;
#endif
}

using CacheDataType = struct CacheData {
    std::shared_ptr<Drawing::Image> precalculationImg = nullptr;
    std::shared_ptr<Drawing::Image> blurredSdfMaskImg = nullptr;
    uint32_t hash = 0;
    float blurRadius = 0.0f;
};

using namespace Drawing;
namespace {
constexpr size_t NUM0 = 0;
constexpr size_t NUM1 = 1;
constexpr size_t NUM2 = 2;
constexpr size_t MIN_NUM = 6;
constexpr static uint8_t POSITION_CHANNEL = 2; // 2 floats per point
constexpr int MAX_CURVES_PER_GRID = 16;
constexpr int MIN_GRID_SIZE = 128;
constexpr int CURVE_CAPACITY = 128;
constexpr int XMIN_I = 0;
constexpr int XMAX_I = 1;
constexpr int YMIN_I = 2;
constexpr int YMAX_I = 3;
constexpr Drawing::ColorType RGBA_F16 = Drawing::ColorType::COLORTYPE_RGBA_F16;
constexpr bool NOT_BUDGETED = false;
// shader
static constexpr char FLOW_LIGHT_PROG[] = R"(
    uniform shader precalculationImage;
    uniform vec2 iResolution;
    uniform float line1Start;
    uniform float line1Length;
    uniform vec3 line1Color;
    uniform float line2Start;
    uniform float line2Length;
    uniform vec3 line2Color;
    uniform float lineThickness;
    // ===== Constant =====
    const float thickness = 0.04;
    const float glowBellSigma = 0.28;
    const float distMin = 1e-4;
    const float intensity = 1.75;
    const float glowRadius = 0.004;
    const float intensityFactor = 0.0045;

    // Gaussian bell-shaped profile centered at t = 0.5
    float BellShape(float t, float sigma)
    {
        float x = (t - 0.5) / sigma;
        return exp(-x * x);
    }

    // Compute glowing intensity based on SDF and local t on segment
    float GetSegmentGlowIntensity(float sdfData, float weight, float tLocal)
    {
        float w = BellShape(tLocal, glowBellSigma); // tLocal: [0,1]
        float th = thickness * w * lineThickness;
        return log(1.0 + weight * pow(glowRadius / max(sdfData - th, distMin), intensity) * intensityFactor);
    }

    void ComputeArcWindow(float tGlobal, float tStart, float windowLen, float sigma,
        inout float tLocal, inout float weight)
    {
        float shiftedT = mod(tGlobal - tStart + 1.0, 1.0);
        tLocal = shiftedT / windowLen;
        float inWindow = step(0.0, shiftedT) * step(shiftedT, windowLen);
        weight = inWindow * BellShape(tLocal, sigma);
    }

    vec3 SamplePrecalculationImage(vec2 coord)
    {
        return precalculationImage.eval(coord).rgb;
    }

    float DecodeFloat(vec2 rg)
    {
        return rg.x + rg.y / 255.0; // 255.0 = maximum value representable in 8-bit channel
    }

    vec4 main(vec2 fragCoord)
    {
        vec2 p = (2.0 * fragCoord - iResolution.xy) / iResolution.y; // 2.0: map uv to [-1, 1]
        vec3 precalculationData = SamplePrecalculationImage(fragCoord);
        float sdf = DecodeFloat(precalculationData.rg) * 3.0; // 3.0:decode the sdf
        float tGlobal = precalculationData.b;
        if (precalculationImage.eval(fragCoord).a < 0.5) { // 0.5: discard transparent pixels
            return vec4(0.0, 0.0, 0.0, 0.0);
        }

        // === Sliding window for glow arcs ===
        float weight1 = 0.0;
        float tLocal1 = 0.0;
        float weight2 = 0.0;
        float tLocal2 = 0.0;
        ComputeArcWindow(tGlobal, line1Start, line1Length, glowBellSigma, tLocal1, weight1);
        ComputeArcWindow(tGlobal, line2Start, line2Length, glowBellSigma, tLocal2, weight2);

        // === Glow color composition ===
        float line1Intensity = GetSegmentGlowIntensity(sdf, weight1, tLocal1);
        float line2Intensity = GetSegmentGlowIntensity(sdf, weight2, tLocal2);
        vec3 glowCol1 = line1Color * line1Intensity;
        vec3 glowCol2 = line2Color * line2Intensity;
        vec3 glow = glowCol1 + glowCol2;
        glow *= exp(-sdf * 50.0); // 50.0: the greater, the thinner core
        float alpha = max(max(glow.r, glow.g), glow.b);
        return vec4(glow, alpha);
    }
)";

static constexpr char PRECALCULATIONFORMORECURVES_PROG[] = R"(
    uniform shader loopImage;
    uniform vec2 iResolution;
    uniform float count;
    const int capacity = 48; // 48 control points for 16 Bezier curves
    uniform vec2 controlPoints[capacity];
    uniform float segmentIndex[16]; // 16: maximum 16 segments per grid
    // ===== Constant =====
    const float INF = 1e10; // infinity, i.e., 1.0 / 0.0
    const float SQRT3 = 1.7320508;

    // ===== Vector Math Utilities =====
    float Cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }
    float SaturateFloat(float a) { return clamp(a, 0.0, 1.0); }
    vec3 SaturateVec3(vec3 a) { return clamp(a, 0.0, 1.0); }
    float AbsMin(float a, float b) { return abs(a) < abs(b) ? a : b; }

    vec2 GetElement(vec2 arr[capacity], int index)
    {
        for (int i = 0; i < capacity; ++i) {
            if (i == index) return arr[i];
        }
        return vec2(0.0);
    }

    vec2 Bezier(vec2 pointA, vec2 pointB, vec2 pointC, float t)
    {
        return pointA + t * (-2.0 * pointA + 2.0 * pointB) + t * t * (pointA - 2.0 * pointB + pointC);
    }

    float SdfLine(vec2 p, vec2 a, vec2 b)
    {
        float h = SaturateFloat(dot(p - a, b - a) / dot(b - a, b - a));
        return length(p - a - h * (b - a));
    }

    float SdfLinePartition(vec2 p, vec2 a, vec2 b, inout vec2 closestPoint, inout float bestTLocal)
    {
        vec2 ba = b - a;
        vec2 pa = p - a;
        float h = SaturateFloat(dot(pa, ba) / dot(ba, ba));
        closestPoint = a + h * ba;
        bestTLocal = h;
        vec2 k = pa - h * ba;
        vec2 n = vec2(ba.y, -ba.x);
        return (dot(k, n) >= 0.0) ? length(k) : -length(k);
    }

    float SdfBezier(vec2 pos, vec2 pointA, vec2 pointB, vec2 pointC,
        inout vec2 closestPoint, inout float bestTLocal)
    {
        const float EPSILON = 1e-3;
        const float ONE_THIRD = 1.0 / 3.0;
        bool abEqual = all(lessThan(abs(pointA - pointB), vec2(EPSILON)));
        bool bcEqual = all(lessThan(abs(pointB - pointC), vec2(EPSILON)));
        bool acEqual = all(lessThan(abs(pointA - pointC), vec2(EPSILON)));
        if (abEqual && bcEqual) {
            closestPoint = pointA;
            return distance(pos, pointA);
        }
        if (abEqual || acEqual) return SdfLinePartition(pos, pointB, pointC, closestPoint, bestTLocal);
        if (bcEqual) return SdfLinePartition(pos, pointA, pointC, closestPoint, bestTLocal);
        if (abs(dot(normalize(pointB - pointA), normalize(pointC - pointB)) - 1.0) < EPSILON)
            return SdfLinePartition(pos, pointA, pointC, closestPoint, bestTLocal);
        vec2 a = pointB - pointA;
        vec2 b = pointA - 2.0 * pointB + pointC;
        vec2 c = a * 2.0;
        vec2 d = pointA - pos;
        float kk = 1.0 / dot(b, b);
        float kx = kk * dot(a, b);
        float ky = kk * (2.0 * dot(a, a) + dot(d, b)) * ONE_THIRD;
        float kz = kk * dot(d, a);
        float res = 0.0;
        float sgn = 0.0;
        float p = ky - kx * kx;
        float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
        float h = q * q + 4.0 * p * p * p;
        if (h >= 0.0) {
            h = sqrt(h);
            vec2 x = 0.5 * (vec2(h, -h) - q);
            vec2 uv = vec2((x.x > 0.0 ? 1.0 : -1.0) * pow(abs(x.x), ONE_THIRD),
                        (x.y > 0.0 ? 1.0 : -1.0) * pow(abs(x.y), ONE_THIRD));
            float t = SaturateFloat(uv.x + uv.y - kx) + EPSILON;
            closestPoint = Bezier(pointA, pointB, pointC, t);
            bestTLocal = t;
            vec2 qv = d + (c + b * t) * t;
            res = dot(qv, qv);
            sgn = Cross2(c + 2.0 * b * t, qv);
            return (sgn > 0.0 ? 1.0 : -1.0) * sqrt(res);
        }
        float z = sqrt(-p);
        float v = acos(q / (p * z * 2.0)) * ONE_THIRD;
        float m = cos(v);
        float n = sin(v) * SQRT3;
        vec3 t = SaturateVec3(vec3(m + m, -n - m, n - m) * z - kx) + EPSILON;
        vec2 qx = d + (c + b * t.x) * t.x;
        float dx = dot(qx, qx);
        float sx = Cross2(c + 2.0 * b * t.x, qx);
        vec2 qy = d + (c + b * t.y) * t.y;
        float dy = dot(qy, qy);
        float sy = Cross2(c + 2.0 * b * t.y, qy);
        res = (dx < dy) ? dx : dy;
        sgn = (dx < dy) ? sx : sy;
        float bestT = (dx < dy) ? t.x : t.y;
        closestPoint = Bezier(pointA, pointB, pointC, bestT);
        bestTLocal = bestT;
        return (sgn > 0.0 ? 1.0 : -1.0) * sqrt(res);
    }

    // ================= SDF Polygon =================
    float SdfControlSegment(vec2 p, vec2 pointA, vec2 pointB, vec2 pointC)
    {
        return AbsMin(SdfLine(p, pointA, pointB), SdfLine(p, pointB, pointC));
    }

    float SdfControlPolygon(vec2 p, vec2 controlPoly[capacity], int size,
        inout vec2 closest[3], inout float closestSegmentIndex)
    {
        float minDist = INF;
        int segmentCount = size / 2;

        // i = 0
        vec2 pointA = controlPoly[0];
        vec2 pointB = controlPoly[1];
        vec2 pointC = controlPoly[2];
        float d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[0];
        }

        // i = 1
        pointA = controlPoly[3]; // iMidPrev = i - 1
        pointB = controlPoly[4]; // i
        pointC = controlPoly[5]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[1];
        }

        // i = 2
        pointA = controlPoly[6]; // iMidPrev = i - 1
        pointB = controlPoly[7]; // i
        pointC = controlPoly[8]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[2];
        }

        // i = 3
        if (size < 9) return minDist; // 9: next i beyond legal size
        pointA = controlPoly[9]; // iMidPrev = i - 1
        pointB = controlPoly[10]; // i
        pointC = controlPoly[11]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[3];
        }

        // i = 4
        if (size < 12) return minDist; // 12: next i beyond legal size
        pointA = controlPoly[12]; // iMidPrev = i - 1
        pointB = controlPoly[13]; // i
        pointC = controlPoly[14]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[4];
        }

        // i = 5
        if (size < 15) return minDist; // 15: next i beyond legal size
        pointA = controlPoly[15]; // iMidPrev = i - 1
        pointB = controlPoly[16]; // i
        pointC = controlPoly[17]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[5];
        }

        // i = 6
        if (size < 18) return minDist; // 18: next i beyond legal size
        pointA = controlPoly[18]; // iMidPrev = i - 1
        pointB = controlPoly[19]; // i
        pointC = controlPoly[20]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[6];
        }

        // i = 7
        if (size < 21) return minDist; // 21: next i beyond legal size
        pointA = controlPoly[21]; // iMidPrev = i - 1
        pointB = controlPoly[22]; // i
        pointC = controlPoly[23]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[7];
        }

        // i = 8
        if (size < 24) return minDist; // 24: next i beyond legal size
        pointA = controlPoly[24]; // iMidPrev = i - 1
        pointB = controlPoly[25]; // i
        pointC = controlPoly[26]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[8];
        }

        // i = 9
        if (size < 27) return minDist; // 27: next i beyond legal size
        pointA = controlPoly[27]; // iMidPrev = i - 1
        pointB = controlPoly[28]; // i
        pointC = controlPoly[29]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[9];
        }

        // i = 10
        if (size < 30) return minDist; // 30: next i beyond legal size
        pointA = controlPoly[30]; // iMidPrev = i - 1
        pointB = controlPoly[31]; // i
        pointC = controlPoly[32]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[10];
        }

        // i = 11
        if (size < 33) return minDist; // 33: next i beyond legal size
        pointA = controlPoly[33]; // iMidPrev = i - 1
        pointB = controlPoly[34]; // i
        pointC = controlPoly[35]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[11];
        }

        // i = 12
        if (size < 36) return minDist; // 36: next i beyond legal size
        pointA = controlPoly[36]; // iMidPrev = i - 1
        pointB = controlPoly[37]; // i
        pointC = controlPoly[38]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[12];
        }

        // i = 13
        if (size < 39) return minDist; // 39: next i beyond legal size
        pointA = controlPoly[39]; // iMidPrev = i - 1
        pointB = controlPoly[40]; // i
        pointC = controlPoly[41]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[13];
        }

        // i = 14
        if (size < 42) return minDist; // 42: next i beyond legal size
        pointA = controlPoly[42]; // iMidPrev = i - 1
        pointB = controlPoly[43]; // i
        pointC = controlPoly[44]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[14];
        }

        // i = 15
        if (size < 45) return minDist; // 45: next i beyond legal size
        pointA = controlPoly[45]; // iMidPrev = i - 1
        pointB = controlPoly[46]; // i
        pointC = controlPoly[47]; // iMid = i + 1
        d = SdfControlSegment(p, pointA, pointB, pointC);
        if (abs(d) < abs(minDist)) {
            minDist = d;
            closest[0] = pointA;
            closest[1] = pointB;
            closest[2] = pointC;
            closestSegmentIndex = segmentIndex[15];
        }
        return minDist;
    }

    float SdfBezierShape(vec2 p, vec2 controlPoly[capacity], int controlPolySize,
        inout vec2 closestPoint, inout float bestTLocal, inout float closestSegmentIndex)
    {
        vec2 closest[3];
        SdfControlPolygon(p, controlPoly, controlPolySize, closest, closestSegmentIndex);
        return SdfBezier(p, closest[0], closest[1], closest[2], closestPoint, bestTLocal);
    }

    vec4 SampleLoopImage(vec2 coord)
    {
        return loopImage.eval(coord);
    }

    vec2 EncodeFloat(float x)
    {
        x = clamp(x, 0.0, 1.0);
        float hi = floor(x * 255.0) / 255.0; // 255.0 = maximum value representable in 8-bit channel
        float lo = fract(x * 255.0); // 255.0 = maximum value representable in 8-bit channel
        return vec2(hi, lo);
    }

    float DecodeFloat(vec2 rg)
    {
        return rg.x + rg.y / 255.0; // 255.0 = maximum value representable in 8-bit channel
    }

    vec4 main(vec2 fragCoord)
    {
        int size = int(count);
        vec2 p = (2.0 * fragCoord - iResolution.xy) / iResolution.y; // 2.0: normalized screen coordinates

        vec4 precalculationData = SampleLoopImage(fragCoord);
        float formerSdf = DecodeFloat(precalculationData.rg) * 3.0; // 3.0: decode the former sdf
        float formerSegmentIndex = precalculationData.b;
        float formerLocalT = precalculationData.a;

        vec2 closestPoint = vec2(0.0);
        float tLocal = 0.0;
        float closestSegmentIndex = 0.0;
        float sdf = SdfBezierShape(p, controlPoints, size, closestPoint, tLocal, closestSegmentIndex);
        closestSegmentIndex = closestSegmentIndex * 0.005; // 0.005: encode the segmentIndex
        float absSdf = abs(sdf);
        if (formerSdf < absSdf) {
            absSdf = formerSdf;
            closestSegmentIndex = formerSegmentIndex;
            tLocal = formerLocalT;
        }
        vec2 encodedSdf = EncodeFloat(absSdf / 3.0); // 3.0: compress sdf to [0,1]
        return vec4(encodedSdf, closestSegmentIndex, tLocal);
    }
)";

static constexpr char BLEND_IMG_PROG[] = R"(
    uniform vec2 factorlight;
    uniform vec2 factorhalo;
    uniform vec2 factorprecalc;
    uniform shader precalculationImage;
    uniform shader image1;
    uniform shader image2;
    uniform float lightWeight;
    uniform float haloWeight;
    uniform float blurRadius;
    uniform float headRoom;

    vec4 main(vec2 fragCoord)
    {
        vec4 c1 = image1.eval(fragCoord * factorlight).rgba;
        vec4 c2 = image2.eval(fragCoord * factorhalo).rgba;
        float totalWeight = (lightWeight + haloWeight < 1e-5) ? 1.0 : lightWeight + haloWeight;
        float contourWeight = precalculationImage.eval(fragCoord * factorprecalc).r;
        contourWeight *= blurRadius / 50.0; // 50.0: default blur radius
        vec4 blendColor = c1 + c2 * haloWeight * haloWeight / (totalWeight * lightWeight) * contourWeight;
        vec4 lnValue = log(blendColor + vec4(1.0, 1.0, 1.0, 1.0));
        vec4 c_out = max(2.0, headRoom) * 2.0 * lnValue / (lnValue + 1);
        return vec4(c_out.rgb, clamp(c_out.a, 0.0, 1.0));
    }
)";

static constexpr char SDF_MASK_PROG[] = R"(
    uniform shader precalculationImage;

    float DecodeFloat(vec2 rg)
    {
        return rg.x + rg.y / 255.0; // 255.0 = maximum value representable in 8-bit channel
    }

    vec4 main(vec2 fragCoord)
    {
        float sdf = DecodeFloat(precalculationImage.eval(fragCoord).rg) * 3.0; // 3.0: decode the sdf
        float contourWeight = exp(-sdf * 20.0); // 20.0: the greater, the thinner core
        if (precalculationImage.eval(fragCoord).a < 0.5) { // 0.5: discard the transparent pixels
            contourWeight = 0.0;
        }
        return vec4(contourWeight, 0.0, 0.0, 1.0);
    }
)";

static constexpr char CONVERT_IMG_PROG[] = R"(
    uniform shader sdfImage;
    uniform shader progressImage;
    uniform float count;
    uniform float leftGridBoundary;
    uniform float topGridBoundary;
    uniform float curveWeightPrefix[128]; // 128: the max number of segments
    uniform float curveWeightCurrent[128]; // 128: the max number of segments

    vec4 SampleLocalImage(vec2 coord)
    {
        vec2 mapCoord = vec2(coord.x - leftGridBoundary, coord.y - topGridBoundary);
        return vec4(sdfImage.eval(mapCoord).rg, progressImage.eval(mapCoord).ba);
    }

    vec2 EncodeFloat(float x)
    {
        x = clamp(x, 0.0, 1.0);
        float hi = floor(x * 255.0) / 255.0; // 255.0 = maximum value representable in 8-bit channel
        float lo = fract(x * 255.0); // 255.0 = maximum value representable in 8-bit channel
        return vec2(hi, lo);
    }

    float GetElement(float arr[128], int index) // 128: the max number of segments
    {
        for (int i = 0; i < 128; ++i) { // 128: the max number of segments
            if (i == index) return arr[i];
        }
        return 0.0;
    }

    vec4 main(vec2 fragCoord)
    {
        float segmentCount = 0.5 * count;
        vec4 precalculationData = SampleLocalImage(fragCoord);
        vec2 encodedSdf = precalculationData.rg;
        float decodedIndex = floor(precalculationData.b * 200.0 + 0.5); // 200.0: decode the closestSegmentIndex
        int closestSegmentIndex = int(decodedIndex);
        float tLocal = precalculationData.a;

        float tGlobal = GetElement(curveWeightPrefix, closestSegmentIndex) +
            tLocal * GetElement(curveWeightCurrent, closestSegmentIndex);
        return vec4(encodedSdf, tGlobal, 1.0);
    }
)";

std::vector<Vector2f> ConvertUVToNDC(const std::vector<Vector2f>& uvPoints, int width, int height)
{
    // canvas rect must width > 1 and height >1
    if (height < 1 || width < 1 || uvPoints.empty()) {
        return {};
    }

    std::vector<Vector2f> ndcPoints;
    ndcPoints.reserve(uvPoints.size());
    float aspect = static_cast<float>(width) / static_cast<float>(height);

    for (const auto& uv : uvPoints) {
        float ndcX = (uv[0] * 2.0f - 1.0f) * aspect;
        float ndcY = uv[1] * 2.0f - 1.0f;
        ndcPoints.emplace_back(ndcX, ndcY);
    }

    return ndcPoints;
}

void ConvertPointsTo(const std::vector<Vector2f>& in, std::vector<float>& out)
{
    out.clear();

    for (auto& p : in) {
        out.push_back(p[0]);
        out.push_back(p[1]);
    }
}

uint32_t CalHash(const std::vector<Vector2f>& in)
{
#ifdef USE_M133_SKIA
    const auto hashFunc = SkChecksum::Hash32;
#else
    const auto hashFunc = SkOpts::hash;
#endif
    uint32_t hashOut = 0;
    for (auto& p : in) {
        hashOut = hashFunc(&p, sizeof(p), hashOut);
    }
    return hashOut;
}

bool FEqual(float a, float b)
{
    return std::abs(a - b) <= std::numeric_limits<float>::epsilon();
}

bool IntersectBBox(const Box4f& a, const Box4f& b)
{
    return !(a[XMAX_I] < b[XMIN_I] || a[XMIN_I] > b[XMAX_I] || a[YMAX_I] < b[YMIN_I] || a[YMIN_I] > b[YMAX_I]);
}
} // anonymous namespace

GEContourDiagonalFlowLightShader::GEContourDiagonalFlowLightShader() {}
GEContourDiagonalFlowLightShader::GEContourDiagonalFlowLightShader(GEContentDiagonalFlowLightShaderParams& param)
{
    contourDiagonalFlowLightParams_ = param;
}

void GEContourDiagonalFlowLightShader::MakeDrawingShader(const Drawing::Rect& rect, float progress) {}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEContourDiagonalFlowLightShader::GetFlowLightPrecalBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> precalculationShaderForMoreCurves = nullptr;

    if (precalculationShaderForMoreCurves == nullptr) {
        precalculationShaderForMoreCurves = Drawing::RuntimeEffect::CreateForShader(PRECALCULATIONFORMORECURVES_PROG);
    }

    if (precalculationShaderForMoreCurves == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader precalculationShaderForMoreCurves is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(precalculationShaderForMoreCurves);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEContourDiagonalFlowLightShader::FlowLightConvertBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> convertShader = nullptr;

    if (convertShader == nullptr) {
        convertShader = Drawing::RuntimeEffect::CreateForShader(CONVERT_IMG_PROG);
    }

    if (convertShader == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader convertShader is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(convertShader);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEContourDiagonalFlowLightShader::SdfMaskBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> sdfMaskShader = nullptr;

    if (sdfMaskShader == nullptr) {
        sdfMaskShader = Drawing::RuntimeEffect::CreateForShader(SDF_MASK_PROG);
    }

    if (sdfMaskShader == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader convertShader is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(sdfMaskShader);
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::BlurImg(Drawing::Canvas& canvas,
    const Drawing::Rect& rect, std::shared_ptr<Drawing::Image> image, float blurRadius)
{
    if (GetSwitchMESABlurEnable()) {
        Drawing::GEMESABlurShaderFilterParams mesaParams{};
        mesaParams.radius = blurRadius;
        auto blurImgShader = std::make_shared<GEMESABlurShaderFilter>(mesaParams);
        return blurImgShader->OnProcessImageWithoutUpSampling(canvas, image, rect, rect);
    }
    Drawing::GEKawaseBlurShaderFilterParams blurImgParas{blurRadius};
    std::shared_ptr<GEKawaseBlurShaderFilter> blurImgShader = std::make_shared<GEKawaseBlurShaderFilter>(blurImgParas);
    blurImgShader->SetFactor(0.0f); // not need noise
    return blurImgShader->ProcessImage(canvas, image, rect, rect);
}

void GEContourDiagonalFlowLightShader::Preprocess(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    if (contourDiagonalFlowLightParams_.contour_.size() < MIN_NUM) {
        GE_LOGE("GEContourDiagonalFlowLightShader less point %{public}zu",
            contourDiagonalFlowLightParams_.contour_.size());
        cacheAnyPtr_ = nullptr;
        return;
    }
    if (!rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader canvas not valid");
        return;
    }

    pointCnt_ = contourDiagonalFlowLightParams_.contour_.size();
    auto inHash = CalHash(contourDiagonalFlowLightParams_.contour_);
    float inRadius = contourDiagonalFlowLightParams_.haloRadius_;
    if (cacheAnyPtr_ == nullptr ||  inHash != std::any_cast<CacheDataType>(*cacheAnyPtr_).hash ||
        !FEqual(inRadius, std::any_cast<CacheDataType>(*cacheAnyPtr_).blurRadius)) {
        auto ndcPoints = ConvertUVToNDC(contourDiagonalFlowLightParams_.contour_, rect.GetWidth(), rect.GetHeight());
        CreateSurfaceAndCanvas(canvas, rect);
        if (offscreenSurface_ == nullptr || offscreenCanvas_ == nullptr) {
            GE_LOGE("GEContourDiagonalFlowLightShader create surface or canvas failed");
            cacheAnyPtr_ = nullptr;
            return;
        }
        ConvertPointsTo(ndcPoints, controlPoints_);
        constexpr int minValidPointSize = 6; // 3 line - 6 point
        bool isCurveValid = pointCnt_ > minValidPointSize && pointCnt_ % 2 == 0; // valid curves have 2n point
        if (!isCurveValid) {
            GE_LOGE("GEContourDiagonalFlowLightShader curve is not enough");
            return;
        }
        numCurves_ = pointCnt_ / 2; // one segment need 2 point
        controlPoints_.resize(pointCnt_ * POSITION_CHANNEL);
        AutoPartitionCal(canvas, rect);

        CacheDataType cacheData;
        cacheData.hash = inHash;
        cacheData.blurRadius = inRadius;

        auto cacheImg = offscreenSurface_->GetImageSnapshot();
        if (cacheImg) {
            cacheData.precalculationImg = cacheImg;
            auto sdfMaskImg = CreateSdfMaskImg(canvas, cacheImg);
            if (sdfMaskImg) {
                float sdfMaskBlurRadius = 10.0; // 10.0: blur radius for sdf mask in BlendImg
                cacheData.blurredSdfMaskImg = BlurImg(canvas, rect, sdfMaskImg, sdfMaskBlurRadius);
            }
            cacheAnyPtr_ = std::make_shared<std::any>(std::make_any<CacheDataType>(cacheData));
        }
    }
}

void GEContourDiagonalFlowLightShader::AutoPartitionCal(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    if (offscreenCanvas_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::AutoPartitionCal offscreenCanvas canvas failed");
        return;
    }
    float blurRadiusBound = 2.0f * // convert pixel scale to ndc scale
        contourDiagonalFlowLightParams_.haloRadius_ / (FEqual(rect.GetHeight(), 0.0f) ? 1.0f : rect.GetHeight());
    float maxThickness = 0.05f + blurRadiusBound; // 0.05: max thickness of the curve
    AutoGridPartition(rect.GetWidth(), rect.GetHeight(), maxThickness);
    // gpu cal
    for (int i = 0; i < static_cast<int>(curvesInGrid_.size()); i++) {
        if (curvesInGrid_[i].first.size() > 0) {
            Box4f area = curvesInGrid_[i].second.bbox;
            const Drawing::Rect rectN = Drawing::Rect(area[0], area[2], area[1], area[3]);
            PreCalculateRegion(canvas, *offscreenCanvas_, i, rect, rectN);
        }
    }
}

void GEContourDiagonalFlowLightShader::AutoGridPartition(int width, int height, float maxThickness)
{
    if (width <= 1 || height <= 1) {
        GE_LOGE("GEContourDiagonalFlowLightShader surface size not valid");
        return;
    }
    curvesInGrid_.clear();
    // calculate the bounding box of all curves
    std::vector<Box4f> curveBBoxes;
    Box4f canvasBBox;
    ComputeAllCurveBoundingBoxes(width, height, maxThickness, canvasBBox, curveBBoxes);
    // init workQueue
    std::queue<Grid> workQueue;
    InitializeWorkQueue(canvasBBox, curveBBoxes, workQueue);
    // grid Partition
    while (!workQueue.empty()) {
        Grid current = workQueue.front();
        workQueue.pop();
        // cal grid size
        float w = current.bbox[1] - current.bbox[0];
        float h = current.bbox[3] - current.bbox[2];
        // check is need to split
        bool needsSplit = (static_cast<int>(current.curveIndices.size()) > MAX_CURVES_PER_GRID) &&
            (w > MIN_GRID_SIZE && h > MIN_GRID_SIZE);
        if (needsSplit) {
            SplitGrid(current, curveBBoxes, workQueue, MIN_GRID_SIZE);
        } else {
            ProcessFinalGrid(current, curveBBoxes, height);
        }
    }
}

void GEContourDiagonalFlowLightShader::SplitGrid(const Grid& current, const std::vector<Box4f>& curveBBoxes,
    std::queue<Grid>& workQueue, float minGridSize)
{
    // partition grid - quadtree
    float midX = (current.bbox[XMIN_I] + current.bbox[XMAX_I]) * 0.5f;
    float midY = (current.bbox[YMIN_I] + current.bbox[YMAX_I]) * 0.5f;
    Box4f quadrants[4] = {
        {current.bbox[XMIN_I], midX, current.bbox[YMIN_I], midY}, // Top-Left
        {midX, current.bbox[XMAX_I], current.bbox[YMIN_I], midY}, // Top-Right
        {current.bbox[XMIN_I], midX, midY, current.bbox[YMAX_I]}, // Bottom-Left
        {midX, current.bbox[XMAX_I], midY, current.bbox[YMAX_I]}  // Bottom-Right
    };
    for (const auto& quad : quadrants) {
        Grid child{quad, {}};
        // find intersecting curves
        for (int idx : current.curveIndices) {
            if (IntersectBBox(child.bbox, curveBBoxes[idx])) {
                child.curveIndices.push_back(idx);
            }
        }
        // add node to workqueue
        if (!child.curveIndices.empty()) {
            workQueue.push(child);
        }
    }
}

void GEContourDiagonalFlowLightShader::ComputeAllCurveBoundingBoxes(
    int width, int height, float maxThickness, Box4f& canvasBBox, std::vector<Box4f>& curveBBoxes)
{
    curveBBoxes.clear();
    curveBBoxes.reserve(numCurves_);
    // calculate the Minimum Bounding Box of the contour
    canvasBBox = {
        static_cast<float>(width),
        0.0f,
        static_cast<float>(height),
        0.0f,
    };

    curveWeightPrefix_.assign(numCurves_, 0.0f);
    curveWeightCurrent_.assign(numCurves_, 0.0f);
    std::vector<float> rawWeight(numCurves_, 0.0f);
    float approxLen = 0.0f;
    float sumWeight = 0.0f;

    for (size_t i = 0; i < numCurves_; ++i) {
        Box4f bbox = ComputeCurveBoundingBox(i, maxThickness, width, height, approxLen);
        curveBBoxes.push_back(bbox);
        canvasBBox[XMIN_I] = std::min(bbox[XMIN_I], canvasBBox[XMIN_I]);
        canvasBBox[XMAX_I] = std::max(bbox[XMAX_I], canvasBBox[XMAX_I]);
        canvasBBox[YMIN_I] = std::min(bbox[YMIN_I], canvasBBox[YMIN_I]);
        canvasBBox[YMAX_I] = std::max(bbox[YMAX_I], canvasBBox[YMAX_I]);

        rawWeight[i] = approxLen;
        sumWeight += rawWeight[i];
    }

    if (!FEqual(sumWeight, 0.0f)) {
        float acc = 0.0f;
        for (size_t i = 0; i < numCurves_; ++i) {
            float weight = rawWeight[i] / sumWeight;
            curveWeightPrefix_[i]  = acc;
            curveWeightCurrent_[i] = weight;
            acc += weight;
        }
    } else if (numCurves_ != 0) {
        const float uniformWeight = 1.0f / static_cast<float>(numCurves_);
        float acc = 0.0f;
        for (size_t i = 0; i < numCurves_; ++i) {
            curveWeightPrefix_[i] = acc;
            curveWeightCurrent_[i] = uniformWeight;
            acc += uniformWeight;
        }
    }
    curveWeightPrefix_.resize(CURVE_CAPACITY, 0.0f);
    curveWeightCurrent_.resize(CURVE_CAPACITY, 0.0f);
}

Box4f GEContourDiagonalFlowLightShader::ComputeCurveBoundingBox(
    size_t curveIndex, float maxThickness, int width, int height, float& approxLenPixels)
{
    float x0 = controlPoints_[4 * curveIndex];      // startPoint x
    float y0 = controlPoints_[4 * curveIndex + 1];  // startPoint y
    float cx = controlPoints_[4 * curveIndex + 2];  // controlPoint x
    float cy = controlPoints_[4 * curveIndex + 3];  // controlPoint y
    float x1 = controlPoints_[(4 * curveIndex + 4) % controlPoints_.size()];  // endPoint x
    float y1 = controlPoints_[(4 * curveIndex + 5) % controlPoints_.size()];  // endPoint y
    float minX = std::min({x0, cx, x1}) - maxThickness;
    float maxX = std::max({x0, cx, x1}) + maxThickness;
    float minY = std::min({y0, cy, y1}) - maxThickness;
    float maxY = std::max({y0, cy, y1}) + maxThickness;
    // map ndc to [0, 1]
    float aspect;
    if (height == 0 || width == 0) {
        aspect = 1.0f;
    } else {
        aspect = static_cast<float>(width) / static_cast<float>(height);
    }

    auto NdcToPix = [&aspect, &width, &height](float xNdc, float yNdc) -> std::pair<float, float> {
        float xPix = ((xNdc / aspect) + 1.0f) * 0.5f * static_cast<float>(width);
        float yPix = (yNdc + 1.0f) * 0.5f * static_cast<float>(height);
        return {xPix, yPix};
    };

    auto dist = [](float ax, float ay, float bx, float by) {
        return std::hypot(bx - ax, by - ay);
    };

    auto [x0p, y0p] = NdcToPix(x0, y0);
    auto [cxp, cyp] = NdcToPix(cx, cy);
    auto [x1p, y1p] = NdcToPix(x1, y1);
    approxLenPixels = dist(x0p, y0p, cxp, cyp) + dist(cxp, cyp, x1p, y1p);

    minX = std::floor((minX / aspect + 1.0f) / 2.0f * width);
    maxX = std::ceil((maxX / aspect + 1.0f) / 2.0f * width);
    minY = std::floor((minY + 1.0f) / 2.0f * height);
    maxY = std::ceil((maxY + 1.0f) / 2.0f * height);
    // check the curve is out of the screen, find the intersection
    minX = std::max(minX, 0.0f);
    maxX = std::min(maxX, static_cast<float>(width));
    minY = std::max(minY, 0.0f);
    maxY = std::min(maxY, static_cast<float>(height));
    return {minX, maxX, minY, maxY};
}

void GEContourDiagonalFlowLightShader::InitializeWorkQueue(
    const Box4f& canvasBBox, const std::vector<Box4f>& curveBBoxes, std::queue<Grid>& workQueue)
{
    // init root grid
    std::vector<int> initialCurves;
    for (size_t i = 0; i < numCurves_; ++i) {
        if (IntersectBBox(canvasBBox, curveBBoxes[i])) {
            initialCurves.push_back(i);
        }
    }
    workQueue.push({canvasBBox, initialCurves});
}

void GEContourDiagonalFlowLightShader::ProcessFinalGrid(
    Grid& current, const std::vector<Box4f>& curveBBoxes, int height)
{
    Grid processedGrid = current;
    std::vector<float> gridCurves;
    std::vector<float> inOrderSeg;
    constexpr int slidingWindowLen = 4; // curve 3 point(6 value), slidingWindowLen is 4
    // since only a partial image is captured, the following shifts need to be applied.
    float xShift = 2*processedGrid.bbox[0] / static_cast<float>(height);
    float yShift = 2*processedGrid.bbox[2] / static_cast<float>(height);
    for (int idx : processedGrid.curveIndices) {
        gridCurves.push_back(controlPoints_[slidingWindowLen * idx] - xShift);      // start point x
        gridCurves.push_back(controlPoints_[slidingWindowLen * idx + 1] - yShift);  // start point y
        gridCurves.push_back(controlPoints_[slidingWindowLen * idx + 2] - xShift);  // 2:control point x
        gridCurves.push_back(controlPoints_[slidingWindowLen * idx + 3] - yShift);  // 3:control point y
        gridCurves.push_back(controlPoints_[(slidingWindowLen * idx + 4) % // 4:end point x
            controlPoints_.size()] - xShift);
        gridCurves.push_back(controlPoints_[(slidingWindowLen * idx + 5) % // 5:end point y
            controlPoints_.size()] - yShift);
        
        inOrderSeg.push_back(static_cast<float>(idx));
    }
    curvesInGrid_.push_back(std::make_pair(gridCurves, processedGrid));
    segmentIndex_.push_back(inOrderSeg);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEContourDiagonalFlowLightShader::GetContourDiagonalFlowLightBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> contourDiagonalFlowLightShaderEffect_ = nullptr;
    if (contourDiagonalFlowLightShaderEffect_ == nullptr) {
        contourDiagonalFlowLightShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(FLOW_LIGHT_PROG);
    }

    if (contourDiagonalFlowLightShaderEffect_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader contourDiagonalFlowLightShaderEffect_ is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(contourDiagonalFlowLightShaderEffect_);
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::DrawRuntimeShader(Drawing::Canvas& canvas,
    const Drawing::Rect& rect)
{
    if (!rect.IsValid()) {
        return nullptr;
    }
    if (cacheAnyPtr_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader DrawRuntimeShader cache is nullptr.");
        return nullptr;
    }
    auto precalculationImage = std::any_cast<CacheDataType>(*cacheAnyPtr_).precalculationImg;
    if (precalculationImage == nullptr) {
        cacheAnyPtr_ = nullptr;
        GE_LOGE("GEContourDiagonalFlowLightShader DrawRuntimeShader cache img is nullptr.");
        return nullptr;
    }
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    builder_ = GetContourDiagonalFlowLightBuilder();
    Drawing::Matrix matrix;
    auto precalculationShader = Drawing::ShaderEffect::CreateImageShader(*precalculationImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    const float defaultLineLength = 0.3f;
    float line1Length = (contourDiagonalFlowLightParams_.line1Length_ < 0.0f ||
        FEqual(contourDiagonalFlowLightParams_.line1Length_, 0.0f)) ?
        defaultLineLength : contourDiagonalFlowLightParams_.line1Length_;
    float line2Length = (contourDiagonalFlowLightParams_.line2Length_ < 0.0f ||
        FEqual(contourDiagonalFlowLightParams_.line2Length_, 0.0f)) ?
        defaultLineLength : contourDiagonalFlowLightParams_.line2Length_;
    if (precalculationShader == nullptr || builder_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader precalculationShader cache img is nullptr.");
        return nullptr;
    }
    builder_->SetChild("precalculationImage", precalculationShader);
    builder_->SetUniform("iResolution", width, height);
    builder_->SetUniform("line1Start", contourDiagonalFlowLightParams_.line1Start_);
    builder_->SetUniform("line1Length", std::clamp(line1Length, 0.0f, 1.0f));
    builder_->SetUniform("line1Color", std::clamp(contourDiagonalFlowLightParams_.line1Color_[NUM0], 0.0f, 1.0f),
        std::clamp(contourDiagonalFlowLightParams_.line1Color_[NUM1], 0.0f, 1.0f),
        std::clamp(contourDiagonalFlowLightParams_.line1Color_[NUM2], 0.0f, 1.0f));
    builder_->SetUniform("line2Start", contourDiagonalFlowLightParams_.line2Start_);
    builder_->SetUniform("line2Length", std::clamp(line2Length, 0.0f, 1.0f));
    builder_->SetUniform("line2Color", std::clamp(contourDiagonalFlowLightParams_.line2Color_[NUM0], 0.0f, 1.0f),
        std::clamp(contourDiagonalFlowLightParams_.line2Color_[NUM1], 0.0f, 1.0f),
        std::clamp(contourDiagonalFlowLightParams_.line2Color_[NUM2], 0.0f, 1.0f));
    builder_->SetUniform("lineThickness", std::clamp(contourDiagonalFlowLightParams_.thickness_, 0.0f, 1.0f));
    Drawing::ImageInfo imageInfo(rect.GetWidth(), rect.GetHeight(), RGBA_F16, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    return DrawWithLightSurface(canvas, builder_, imageInfo);
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::DrawWithLightSurface(Drawing::Canvas& canvas,
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder, const Drawing::ImageInfo& imageInfo)
{
    if (lightSurface_ == nullptr) {
        lightSurface_ = Drawing::Surface::MakeRenderTarget(canvas.GetGPUContext().get(), NOT_BUDGETED, imageInfo);
    }
    if (builder == nullptr || lightSurface_ == nullptr) {
        return nullptr;
    }
    auto lightCanvas = lightSurface_->GetCanvas();
    auto shader = builder->MakeShader(nullptr, false);
    if (lightCanvas == nullptr || shader == nullptr) {
        return nullptr;
    }
    Drawing::Brush brush;
    brush.SetShaderEffect(shader);
    lightCanvas->AttachBrush(brush);
    lightCanvas->DrawRect(Drawing::Rect{0, 0, imageInfo.GetWidth(), imageInfo.GetHeight()});
    lightCanvas->DetachBrush();
    return lightSurface_->GetImageSnapshot();
}

void GEContourDiagonalFlowLightShader::ResizeCurvesData(int gridIndex, size_t subCurveCnt, size_t perSubCurveSize)
{
    curvesInGrid_[gridIndex].first.resize(subCurveCnt * perSubCurveSize, -5.0f); // -5.0:unused points
    segmentIndex_[gridIndex].resize(subCurveCnt * MAX_CURVES_PER_GRID, 0.0f);
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::LoopAllCurvesInBatches(Drawing::Canvas& mainCanvas,
    Drawing::Canvas& canvas, int gridIndex, const Drawing::Rect& wholeRect, const Drawing::Rect& rect)
{
    if (!wholeRect.IsValid() || !rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader::PreCalculateRegion drawing rect is nullptr.");
        return nullptr;
    }
    auto builder = GetFlowLightPrecalBuilder();
    if (builder == nullptr || offscreenSurface_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::builder or offscreenSurface is nullptr.");
        return nullptr;
    }
    auto sdfImg = CreateImg(mainCanvas, rect);
    constexpr size_t curveValueCount = 6; // one curve have 3 point - 6 float
    auto perSubCurveSize = static_cast<size_t>(MAX_CURVES_PER_GRID) * curveValueCount;
    auto subCurveCnt = (curvesInGrid_[gridIndex].first.size() + perSubCurveSize - 1) / perSubCurveSize;
    ResizeCurvesData(gridIndex, subCurveCnt, perSubCurveSize);
    Drawing::Matrix matrix;
    for (size_t i = 0; i < subCurveCnt; i++) {
        std::vector<float> curvesPoints(curvesInGrid_[gridIndex].first.begin() + i * perSubCurveSize,
            curvesInGrid_[gridIndex].first.begin() + (i + 1) * perSubCurveSize);
        std::vector<float> curvesIndex(segmentIndex_[gridIndex].begin() + i * MAX_CURVES_PER_GRID,
            segmentIndex_[gridIndex].begin() + (i + 1) * MAX_CURVES_PER_GRID);
        if (sdfImg == nullptr) {
            GE_LOGE("GEContourDiagonalFlowLightShader::sdfImg is nullptr.");
            return nullptr;
        }
        auto sdfImgShader = Drawing::ShaderEffect::CreateImageShader(*sdfImg, Drawing::TileMode::CLAMP,
            Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
        builder->SetChild("loopImage", sdfImgShader);
        builder->SetUniform("iResolution", wholeRect.GetWidth(), wholeRect.GetHeight());
        builder->SetUniform("count", static_cast<float>(pointCnt_));
        builder->SetUniform("controlPoints", curvesPoints.data(), curvesPoints.size());
        builder->SetUniform("segmentIndex", curvesIndex.data(), curvesIndex.size());
        auto contourDiagonalFlowLightShader = builder->MakeShader(nullptr, false);
        if (contourDiagonalFlowLightShader == nullptr) {
            GE_LOGE("GEContourDiagonalFlowLightShader::PreCalculateRegion contourDiagonalFlowLightShader is nullptr.");
            return nullptr;
        }
        Drawing::Brush brush;
        brush.SetShaderEffect(contourDiagonalFlowLightShader);
        sdfImg = CreateDrawImg(mainCanvas, rect, brush);
    }
    return sdfImg;
}

void GEContourDiagonalFlowLightShader::ConvertImage(Drawing::Canvas& canvas, const Drawing::Rect& rect,
    const std::shared_ptr<Drawing::Image>& sdfImg)
{
    if (sdfImg == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::sdfImg is nullptr in ConvertImage.");
        return;
    }
    auto convertBuilder = FlowLightConvertBuilder();
    if (convertBuilder == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::PreCalculateRegion convertBuilder is nullptr.");
        return;
    }
    Drawing::Matrix matrix;
    auto sdfImgShader = Drawing::ShaderEffect::CreateImageShader(*sdfImg,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR),
        matrix);
    auto globalTShader = Drawing::ShaderEffect::CreateImageShader(*sdfImg,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::NEAREST),
        matrix);
    convertBuilder->SetChild("sdfImage", sdfImgShader);
    convertBuilder->SetChild("progressImage", globalTShader);
    convertBuilder->SetUniform("count", static_cast<float>(pointCnt_));
    convertBuilder->SetUniform("leftGridBoundary", static_cast<float>(rect.GetLeft()));
    convertBuilder->SetUniform("topGridBoundary", static_cast<float>(rect.GetTop()));
    convertBuilder->SetUniform("curveWeightPrefix", curveWeightPrefix_.data(), curveWeightPrefix_.size());
    convertBuilder->SetUniform("curveWeightCurrent", curveWeightCurrent_.data(), curveWeightCurrent_.size());
    auto convertShader = convertBuilder->MakeShader(nullptr, false);
    if (convertShader == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::PreCalculateRegion convertShader is nullptr.");
        return;
    }
    Drawing::Brush brush;
    brush.SetShaderEffect(convertShader);
    canvas.AttachBrush(brush);
    canvas.DrawRect(rect);
    canvas.DetachBrush();
}

void GEContourDiagonalFlowLightShader::PreCalculateRegion(Drawing::Canvas& mainCanvas,
    Drawing::Canvas& canvas, int gridIndex, const Drawing::Rect& wholeRect, const Drawing::Rect& rect)
{
    auto sdfImg = LoopAllCurvesInBatches(mainCanvas, canvas, gridIndex, wholeRect, rect);
    ConvertImage(canvas, rect, sdfImg);
}

void GEContourDiagonalFlowLightShader::CreateSurfaceAndCanvas(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    if (!rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateSurfaceAndCanvas rect is nullptr.");
        return;
    }
    Drawing::ImageInfo imageInfo(rect.GetWidth(), rect.GetHeight(), RGBA_F16, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    offscreenSurface_ = Drawing::Surface::MakeRenderTarget(canvas.GetGPUContext().get(), NOT_BUDGETED, imageInfo);
    if (offscreenSurface_ == nullptr) {
        LOGE("GEContourDiagonalFlowLightShader::CreateSurfaceAndCanvas offscreenSurface is invalid");
        return;
    }
    offscreenCanvas_ = offscreenSurface_->GetCanvas();
    if (offscreenCanvas_ == nullptr) {
        LOGE("GEContourDiagonalFlowLightShader::CreateSurfaceAndCanvas offscreenCanvas is invalid");
        return;
    }
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::CreateImg(Drawing::Canvas& canvas,
    const Drawing::Rect& rect)
{
    if (!rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader::rect Is not Valid.");
        return nullptr;
    }
    Drawing::ImageInfo imageInfo(rect.GetWidth(), rect.GetHeight(), RGBA_F16, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    auto offscreenSurface = Drawing::Surface::MakeRenderTarget(canvas.GetGPUContext().get(), NOT_BUDGETED, imageInfo);
    if (offscreenSurface == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::offscreenSurface Is not Valid.");
        return nullptr;
    }
    auto offscreenCanvas = offscreenSurface->GetCanvas();
    if (offscreenCanvas == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::offscreenCanvas Is not Valid.");
        return nullptr;
    }
    Drawing::Brush brush;
    brush.SetColor(0xFFFFFFFF);
    offscreenCanvas->AttachBrush(brush);
    offscreenCanvas->DrawRect(Drawing::Rect{0, 0, rect.GetWidth(), rect.GetHeight()});
    offscreenCanvas->DetachBrush();
    return offscreenSurface->GetImageSnapshot();
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::CreateDrawImg(Drawing::Canvas& canvas,
    const Drawing::Rect& rect, const Drawing::Brush& brush)
{
    if (!rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateDrawImg rect Is not Valid.");
        return nullptr;
    }
    auto surface = canvas.GetSurface();
    if (surface == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateDrawImg surface Is not Valid.");
        return nullptr;
    }
    auto offscreenSurface = surface->MakeSurface(rect.GetWidth(), rect.GetHeight());
    if (offscreenSurface == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateDrawImg offscreenSurface Is not Valid.");
        return nullptr;
    }
    auto offscreenCanvas = offscreenSurface->GetCanvas();
    if (offscreenCanvas == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateDrawImg offscreenCanvas Is not Valid.");
        return nullptr;
    }
    offscreenCanvas->AttachBrush(brush);
    offscreenCanvas->DrawRect(Drawing::Rect{0, 0, rect.GetWidth(), rect.GetHeight()});
    offscreenCanvas->DetachBrush();
    return offscreenSurface->GetImageSnapshot();
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::CreateSdfMaskImg(Drawing::Canvas& canvas,
    std::shared_ptr<Drawing::Image> precalculationImg)
{
    if (precalculationImg == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateSdfMaskImg:invalid precalculation result.");
        return nullptr;
    }
    auto sdfMaskBuilder = SdfMaskBuilder();
    if (sdfMaskBuilder == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateSdfMaskImg sdfMaskBuilder is nullptr.");
        return nullptr;
    }
    Drawing::Matrix matrix;
    auto sdfImgShader = Drawing::ShaderEffect::CreateImageShader(*precalculationImg,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR),
        matrix);
    if (sdfImgShader == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::CreateSdfMaskImg sdfImgShader is nullptr.");
        return nullptr;
    }
    auto imageInfo = precalculationImg->GetImageInfo();
    sdfMaskBuilder->SetChild("precalculationImage", sdfImgShader);
    auto sdfMaskShader = sdfMaskBuilder->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    return sdfMaskShader;
}

std::shared_ptr<Drawing::Image> GEContourDiagonalFlowLightShader::BlendImg(Drawing::Canvas& canvas,
    std::shared_ptr<Drawing::Image> precalculationImg,
    std::shared_ptr<Drawing::Image> lightImg, std::shared_ptr<Drawing::Image> haloImg)
{
    if (precalculationImg == nullptr || lightImg == nullptr || haloImg == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader::BlendImg:invalid precalculation result.");
        return nullptr;
    }
    thread_local std::shared_ptr<Drawing::RuntimeEffect> blendShaderEffect_ = nullptr;
    if (blendShaderEffect_ == nullptr) {
        blendShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(BLEND_IMG_PROG);
    }
    if (blendShaderEffect_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader contourDiagonalFlowLightShaderEffect_ is nullptr.");
        return nullptr;
    }
    auto blendBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(blendShaderEffect_);
    Drawing::Matrix matrix;
    if (cacheAnyPtr_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader BlendImg cache is nullptr.");
        return nullptr;
    }
    auto precalculationImgShader = Drawing::ShaderEffect::CreateImageShader(*precalculationImg,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    auto lightShader = Drawing::ShaderEffect::CreateImageShader(*lightImg, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    auto haloShader = Drawing::ShaderEffect::CreateImageShader(*haloImg, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    auto imageInfo = imgInfo_;
    blendBuilder->SetUniform("factorlight",
        static_cast<float>(lightImg->GetWidth()) / static_cast<float>(imageInfo.GetWidth()),
        static_cast<float>(lightImg->GetHeight()) / static_cast<float>(imageInfo.GetHeight()));
    blendBuilder->SetUniform("factorhalo",
        static_cast<float>(haloImg->GetWidth()) / static_cast<float>(imageInfo.GetWidth()),
        static_cast<float>(haloImg->GetHeight()) / static_cast<float>(imageInfo.GetHeight()));
    blendBuilder->SetUniform("factorprecalc",
        static_cast<float>(precalculationImg->GetWidth()) / static_cast<float>(imageInfo.GetWidth()),
        static_cast<float>(precalculationImg->GetHeight()) / static_cast<float>(imageInfo.GetHeight()));
    blendBuilder->SetChild("precalculationImage", precalculationImgShader);
    blendBuilder->SetChild("image1", lightShader);
    blendBuilder->SetChild("image2", haloShader);
    blendBuilder->SetUniform("lightWeight", contourDiagonalFlowLightParams_.lightWeight_);
    blendBuilder->SetUniform("haloWeight", contourDiagonalFlowLightParams_.haloWeight_);
    blendBuilder->SetUniform("blurRadius", contourDiagonalFlowLightParams_.haloRadius_);
    blendBuilder->SetUniform("headRoom", std::max(supportHeadroom_, 1.0f));
    return DrawWithLightSurface(canvas, blendBuilder, imageInfo);
}

void GEContourDiagonalFlowLightShader::OnDrawShader(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    if (!rect.IsValid()) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader canvas is nullptr.");
        return;
    }
    Preprocess(canvas, rect); // to calculate your cache data
    if (cacheAnyPtr_ == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader cache is nullptr.");
        return;
    }
    auto precalculationImage = std::any_cast<CacheDataType>(*cacheAnyPtr_).blurredSdfMaskImg;
    if (precalculationImage == nullptr) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader cache img is nullptr.");
        return;
    }
    auto lightImg = DrawRuntimeShader(canvas, rect);
    if (!lightImg) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader light img is nullptr.");
        return;
    }
    imgInfo_ = lightImg->GetImageInfo();
    float lightCoreBlurRadius = 3.0; // 3.0: blur radius for light core for anti-aliasing
    auto blurredLightImg = BlurImg(canvas, rect, lightImg, lightCoreBlurRadius);
    if (!blurredLightImg) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader blurredLightImg is nullptr.");
        return;
    }
    auto blurImg = BlurImg(canvas, rect, lightImg, contourDiagonalFlowLightParams_.haloRadius_);
    if (!blurImg) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader halo img is nullptr.");
        return;
    }
    auto resImg = BlendImg(canvas, precalculationImage, blurredLightImg, blurImg);
    if (!resImg) {
        GE_LOGE("GEContourDiagonalFlowLightShader OnDrawShader blendimg is nullptr.");
        return;
    }
    Drawing::Brush brush;
    canvas.AttachBrush(brush);
    canvas.DrawImageRect(*resImg, rect, rect, Drawing::SamplingOptions());
    canvas.DetachBrush();
}
} // namespace Rosen
} // namespace OHOS