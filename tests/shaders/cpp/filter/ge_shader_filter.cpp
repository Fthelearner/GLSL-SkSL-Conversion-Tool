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
#include "ge_shader_filter.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {

std::shared_ptr<Drawing::Image> GEShaderFilter::ProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    GE_TRACE_NAME_FMT("GEShaderFilter::ProcessImage, Type: %s, dst Rect Width: %g, Height: %g",
        TypeName().data(), dst.GetWidth(), dst.GetHeight());
    return OnProcessImage(canvas, image, src, dst);
}

bool GEShaderFilter::DrawImage(Drawing::Canvas& canvas, const std::shared_ptr<Drawing::Image> image,
    const Drawing::Rect& src, const Drawing::Rect& dst, Drawing::Brush& brush)
{
    GE_TRACE_NAME_FMT("GEShaderFilter::DrawImage, Type: %s, dst Rect Width: %g, Height: %g",
        TypeName().data(), dst.GetWidth(), dst.GetHeight());
    return OnDrawImage(canvas, image, src, dst, brush);
}

} // namespace Rosen
} // namespace OHOS
