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

#include "ge_grid_warp_shader_filter.h"

#include "draw/surface.h"
#include "ge_log.h"

namespace OHOS {
namespace Rosen {

static constexpr float DEGREE_180 = 180.0f;

GEGridWarpShaderFilter::GEGridWarpShaderFilter(const Drawing::GEGridWarpShaderFilterParams& params)
{
    const int n = GRID_ROW_OR_COL_NUM;
    const int mainPointCount = (n + 1) * (n + 1);
    if (mainPointCount != GRID_WARP_MAIN_POINT_NUM) {
        LOGE("Main point count mismatch! Expected %d, got %zu", mainPointCount, GRID_WARP_MAIN_POINT_NUM);
        return;
    }

    for (size_t i = 0; i < GRID_WARP_MAIN_POINT_NUM; ++i) {
        gridPointsCoords_[i] = Drawing::Point{
            params.gridPoints[i].first,
            params.gridPoints[i].second
        };
        gridPointsAngles_[i] = Drawing::Point{
            params.rotationAngles[i].first / DEGREE_180 * PI,
            params.rotationAngles[i].second / DEGREE_180 * PI
        };
    }

    const float pointsDistance = 1.0f / (n * 3.0f);
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            const int patchIndex = row * n + col;
            bezierPatch_[patchIndex] = CalculateBezierPatch(row, col, n, pointsDistance);
        }
    }
}

Drawing::Point GEGridWarpShaderFilter::CalcPointCoord(const int index, const float pointsDistance,
    LocationType location)
{
    const auto& coord = gridPointsCoords_[index];
    const auto& angle = gridPointsAngles_[index];

    switch (location) {
        case LocationType::RIGHT:
            return {coord.GetX() + pointsDistance * cosf(angle.GetX()),
                    coord.GetY() + pointsDistance * sinf(angle.GetX())};
        case LocationType::LEFT:
            return {coord.GetX() - pointsDistance * cosf(angle.GetX()),
                    coord.GetY() - pointsDistance * sinf(angle.GetX())};
        case LocationType::BOTTOM:
            return {coord.GetX() + pointsDistance * sinf(angle.GetY()),
                    coord.GetY() + pointsDistance * cosf(angle.GetY())};
        case LocationType::TOP:
            return {coord.GetX() - pointsDistance * sinf(angle.GetY()),
                    coord.GetY() - pointsDistance * cosf(angle.GetY())};
        default: // LocationType::NONE：original self location
            return {coord.GetX(), coord.GetY()};
    }
}

GEGridWarpShaderFilter::BezierPatchArray GEGridWarpShaderFilter::CalculateBezierPatch(int row,
    int col, int n, float pointsDistance)
{
    const int idxTL = row * (n + 1) + col;               // Top left main control point index
    const int idxTR = row * (n + 1) + (col + 1);         // Top right main control point index
    const int idxBR = (row + 1) * (n + 1) + (col + 1);   // Bottom right main control point index
    const int idxBL = (row + 1) * (n + 1) + col;         // Bottom left main control point index

    return {
        CalcPointCoord(idxTL, pointsDistance, LocationType::NONE),   // 0:Top left point
        CalcPointCoord(idxTL, pointsDistance, LocationType::RIGHT),
        CalcPointCoord(idxTR, pointsDistance, LocationType::LEFT),
        CalcPointCoord(idxTR, pointsDistance, LocationType::NONE),
        CalcPointCoord(idxTR, pointsDistance, LocationType::BOTTOM),
        CalcPointCoord(idxBR, pointsDistance, LocationType::TOP),
        CalcPointCoord(idxBR, pointsDistance, LocationType::NONE),
        CalcPointCoord(idxBR, pointsDistance, LocationType::LEFT),
        CalcPointCoord(idxBL, pointsDistance, LocationType::RIGHT),
        CalcPointCoord(idxBL, pointsDistance, LocationType::NONE),
        CalcPointCoord(idxBL, pointsDistance, LocationType::TOP),
        CalcPointCoord(idxTL, pointsDistance, LocationType::BOTTOM)
    };
}

// 4 coordinates of image texture
// +----+----+
// | 1  | 2  |
// +----+----+
// | 3  | 4  |
// +----+----+
GEGridWarpShaderFilter::GridTextureCoords GEGridWarpShaderFilter::CalcTexCoords(int imageWidth, int imageHeight)
{
    std::array<std::array<Drawing::Point, GRID_TEXTURE_COORDS_NUM>, GRID_NUM> texCoords;
    for (size_t i = 0; i < GRID_NUM; i++) {
        float offsetX = 0.5f * (i % 2); // 0.5 & 2:corresponding to 2*2 grid
        float offsetY = 0.5f * (i / 2); // 0.5 & 2:corresponding to 2*2 grid
        texCoords[i][0].Set(offsetX * imageWidth, offsetY * imageHeight);
        texCoords[i][1].Set((offsetX + 0.5f) * imageWidth, offsetY * imageHeight);
        texCoords[i][2].Set((offsetX + 0.5f) * imageWidth, (offsetY + 0.5f) * imageHeight); // 2: loop index
        texCoords[i][3].Set(offsetX * imageWidth, (offsetY + 0.5f) * imageHeight); // 3: loop index
    }
    return texCoords;
}

std::shared_ptr<Drawing::Image> GEGridWarpShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    int imageHeight = image->GetHeight();
    int imageWidth = image->GetWidth();
    if (imageHeight <= 0 || imageWidth <= 0) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage imageinfo is invalid");
        return nullptr;
    }
    auto brush = GetBrush(image);

    std::array<std::array<Drawing::Point, GRID_TEXTURE_COORDS_NUM>, GRID_NUM> texCoords =
        CalcTexCoords(imageWidth, imageHeight);

    std::array<std::array<Drawing::Point, BEZIER_WARP_POINT_NUM>, GRID_NUM> realBezierPatch;
    for (size_t i = 0; i < GRID_NUM; i++) {
        for (size_t j = 0; j < BEZIER_WARP_POINT_NUM; j++) {
            realBezierPatch[i][j].Set(bezierPatch_[i][j].GetX() * imageWidth, bezierPatch_[i][j].GetY() * imageHeight);
        }
    }

    auto surface = canvas.GetSurface();
    if (surface == nullptr) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage surface is invalid");
        return nullptr;
    }
    auto offscreenRect = dst;
    std::shared_ptr<Drawing::Surface> offscreenSurface = surface->MakeSurface(offscreenRect.GetWidth(),
        offscreenRect.GetHeight());
    if (offscreenSurface == nullptr) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage offscreenSurface is invalid");
        return nullptr;
    }
    std::shared_ptr<Drawing::Canvas> offscreenCanvas = offscreenSurface->GetCanvas();
    if (offscreenCanvas == nullptr) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage offscreenCanvas is invalid");
        return nullptr;
    }

    offscreenCanvas->AttachBrush(brush);
    for (size_t i = 0; i < GRID_NUM; i++) {
        offscreenCanvas->DrawPatch(realBezierPatch[i].data(), nullptr, texCoords[i].data(),
            Drawing::BlendMode::SRC_OVER);
    }

    // Update the cache state with the filtered snapshot.
    auto filteredSnapshot = offscreenSurface->GetImageSnapshot();
    if (filteredSnapshot == nullptr) {
        LOGE("GEGridWarpShaderFilter::OnProcessImage filteredSnapshot is invalid");
    }
    offscreenCanvas->DetachBrush();
    return filteredSnapshot;
}

Drawing::Brush GEGridWarpShaderFilter::GetBrush(const std::shared_ptr<Drawing::Image>& image) const
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

} // namespace Rosen
} // namespace OHOS