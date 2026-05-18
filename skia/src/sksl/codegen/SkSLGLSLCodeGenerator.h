/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_GLSLCODEGENERATOR
#define SKSL_GLSLCODEGENERATOR

namespace sksl_glsl_binding { class ProvenanceConfig; }

namespace SkSL {

struct NativeShader;
enum class PrettyPrint : bool;
class OutputStream;
struct Program;
struct ShaderCaps;

/** Converts a Program into GLSL code. */
bool ToGLSL(Program& program, const ShaderCaps* caps, OutputStream& out, PrettyPrint);
bool ToGLSL(Program& program, const ShaderCaps* caps, OutputStream& out);
bool ToGLSL(Program& program, const ShaderCaps* caps, NativeShader* out);

/**
 * Converts a Program into GLSL code, guided by an optional input provenance config.
 * The config carries metadata (shader stage, GLSL dialect, version, etc.) from a
 * prior GLSL→SkSL conversion, enabling round-trip fidelity.
 */
bool ToGLSL(Program& program, const ShaderCaps* caps, OutputStream& out, PrettyPrint,
            const sksl_glsl_binding::ProvenanceConfig* inConfig);

/** After ToGLSL, returns the provenance config recorded during lowering.
    Empty config means no SKSL-specific constructs were lowered. */
const sksl_glsl_binding::ProvenanceConfig& GetLastProvenanceConfig();

}  // namespace SkSL

#endif
