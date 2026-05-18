//
// Copyright (C) 2024
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef _GLSKSLOUTPUT_INCLUDED_
#define _GLSKSLOUTPUT_INCLUDED_

#include "../Include/intermediate.h"
#include "../Include/InfoSink.h"
#include "../Public/ShaderLang.h"

namespace sksl_glsl_binding { class ProvenanceConfig; }

namespace glslang {

// GLSL to SkSL code generation
// Converts a glslang AST to SkSL output.
// If inConfig is provided, it is used to guide reverse translation of
// SKSL-specific semantics that have no direct GLSL surface form.
// If outConfig is provided, it receives provenance entries recorded during
// this GLSL→SKSL translation, for bidirectional round-trip symmetry.
bool OutputSkSL(TIntermNode* root, EShLanguage stage, TInfoSink& infoSink,
                const sksl_glsl_binding::ProvenanceConfig* inConfig = nullptr,
                sksl_glsl_binding::ProvenanceConfig* outConfig = nullptr);

} // end namespace glslang

#endif // _GLSKSLOUTPUT_INCLUDED_
