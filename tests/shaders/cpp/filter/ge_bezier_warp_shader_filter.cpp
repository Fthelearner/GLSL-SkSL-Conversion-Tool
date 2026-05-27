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
#include "ge_log.h"
#include "draw/surface.h"
#include "ge_bezier_warp_shader_filter.h"

namespace OHOS {
namespace Rosen {
// 0 to 11 indicate the index of the bezier patch control point, starting at the top-left corner, in clockwise order.
constexpr size_t TOP_LEFT_INDEX      = 0;
constexpr size_t TOP_ONE_THIRD       = 1;
constexpr size_t TOP_TWO_THIRDS      = 2;
constexpr size_t TOP_RIGHT_INDEX     = 3;
constexpr size_t RIGHT_ONE_THIRD     = 4;
constexpr size_t RIGHT_TWO_THIRDS    = 5;
constexpr size_t BOTTOM_RIGHT_INDEX  = 6;
constexpr size_t BOTTOM_TWO_THIRDS   = 7;
constexpr size_t BOTTOM_ONE_THIRD    = 8;
constexpr size_t BOTTOM_LEFT_INDEX   = 9;
constexpr size_t LEFT_TWO_THIRDS     = 10;
constexpr size_t LEFT_ONE_THIRD      = 11;

GEBezierWarpShaderFilter::GEBezierWarpShaderFilter(const Drawing::GEBezierWarpShaderFilterParams& params)
    : destinationPatch_(params.destinationPatch)
{}

std::shared_ptr<Drawing::Image> GEBezierWarpShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    int imageHeight = image->GetHeight();
    int imageWidth = image->GetWidth();
    if (imageHeight <= 0 || imageWidth <= 0) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage imageinfo is invalid");
        return nullptr;
    }

    std::array<Drawing::Point, BEZIER_WARP_POINT_NUM> bezierPatch = destinationPatch_;
    auto brush = GetBrush(image);

    // 4 coordinates of image texture
    const std::array<Drawing::Point, 4> texCoords = {
        Drawing::Point{ 0.f, 0.f }, Drawing::Point{ imageWidth, 0.f },
        Drawing::Point{ imageWidth, imageHeight }, Drawing::Point{ 0.f, imageHeight }};

    for (size_t i = 0; i < BEZIER_WARP_POINT_NUM; ++i) {
        bezierPatch[i].Set(bezierPatch[i].GetX() * imageWidth, bezierPatch[i].GetY() * imageHeight);
    }

    auto surface = canvas.GetSurface();
    if (surface == nullptr) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage surface is invalid");
        return nullptr;
    }
    auto offscreenRect = dst;
    std::shared_ptr<Drawing::Surface> offscreenSurface = surface->MakeSurface(offscreenRect.GetWidth(),
        offscreenRect.GetHeight());
    if (offscreenSurface == nullptr) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage offscreenSurface is invalid");
        return nullptr;
    }
    std::shared_ptr<Drawing::Canvas> offscreenCanvas = offscreenSurface->GetCanvas();
    if (offscreenCanvas == nullptr) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage offscreenCanvas is invalid");
        return nullptr;
    }

    offscreenCanvas->AttachBrush(brush);
    offscreenCanvas->DrawPatch(bezierPatch.data(), nullptr, texCoords.data(), Drawing::BlendMode::SRC_OVER);

    // Update the cache state with the filtered snapshot.
    auto filteredSnapshot = offscreenSurface->GetImageSnapshot();
    if (filteredSnapshot == nullptr) {
        LOGE("GEBezierWarpShaderFilter::OnProcessImage filteredSnapshot is invalid");
    }
    offscreenCanvas->DetachBrush();
    return filteredSnapshot;
}

void GEBezierWarpShaderFilter::InitCtrlPoints(std::array<Drawing::Point, BEZIER_WARP_POINT_NUM>& bezierPatch) const
{
    // Find the third division point
    float third = 1.0f / 3.0f;
    float twoThirds = 2.0f / 3.0f;
    float widthOneThird = third; // Anchor point is located at one-third of the width.
    float widthTwoThirds = twoThirds; // Anchor point is located at two-thirds of the width.
    float heightOneThird = third; // Anchor point is located at one-third of the height.
    float heightTwoThirds = twoThirds; // Anchor point is located at two-thirds of the height.

    // top edge control points of upper part
    bezierPatch[TOP_LEFT_INDEX].Set(0.f, 0.f);
    bezierPatch[TOP_ONE_THIRD].Set(widthOneThird, 0.f);
    bezierPatch[TOP_TWO_THIRDS].Set(widthTwoThirds, 0.f);
    bezierPatch[TOP_RIGHT_INDEX].Set(1.f, 0.f);
    // right edge control points of upper part
    bezierPatch[RIGHT_ONE_THIRD].Set(1.f, heightOneThird);
    bezierPatch[RIGHT_TWO_THIRDS].Set(1.f, heightTwoThirds);
    // bottom edge control points of upper part
    bezierPatch[BOTTOM_RIGHT_INDEX].Set(1.f, 1.f);
    bezierPatch[BOTTOM_TWO_THIRDS].Set(widthTwoThirds, 1.f);
    bezierPatch[BOTTOM_ONE_THIRD].Set(widthOneThird, 1.f);
    bezierPatch[BOTTOM_LEFT_INDEX].Set(0.f, 1.f);
    // left edge control points of upper part
    bezierPatch[LEFT_TWO_THIRDS].Set(0.f, heightTwoThirds);
    bezierPatch[LEFT_ONE_THIRD].Set(0.f, heightOneThird);
}

Drawing::Brush GEBezierWarpShaderFilter::GetBrush(const std::shared_ptr<Drawing::Image>& image) const
{
    Drawing::Brush brush;
    if (image == nullptr) {
        return brush;
    }
    brush.SetBlendMode(Drawing::BlendMode::SRC_OVER);
    Drawing::SamplingOptions samplingOptions;
    Drawing::Matrix scaleMat;
    brush.SetShaderEffect(Drawing::ShaderEffect::CreateImageShader(
        *image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, samplingOptions, scaleMat));
    return brush;
}

void GEBezierWarpShaderFilter::SetPathTo(Drawing::Path &path,
    std::array<Drawing::Point, BEZIER_WARP_POINT_NUM>& bezierPatch) const
{
    // The 0th point is the starting point of drawing.
    path.MoveTo(bezierPatch[0].GetX(), bezierPatch[0].GetY());
    // The 1th, 2th and 3th control points are connected to represent the top edge.
    path.CubicTo(bezierPatch[1], bezierPatch[2], bezierPatch[3]);
    // The 4th, 5th and 6th control points are connected to represent the right edge.
    path.CubicTo(bezierPatch[4], bezierPatch[5], bezierPatch[6]);
    // The 7th, 8th and 9th control points are connected to represent the bottom edge.
    path.CubicTo(bezierPatch[7], bezierPatch[8], bezierPatch[9]);
    // The 10th, 11th and 0th control points are connected to represent the left edge.
    path.CubicTo(bezierPatch[10], bezierPatch[11], bezierPatch[0]);
}

} // namespace Rosen
} // namespace OHOS