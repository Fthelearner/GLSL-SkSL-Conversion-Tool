#include "binding_registry/feature_bindings.h"

#include <array>

namespace sksl_glsl_binding {
namespace {

bool HasSurfaceForm(std::span<const SurfaceForm> forms,
                    SurfaceLanguage language,
                    SurfaceFormKind kind,
                    std::string_view pattern) {
    for (const SurfaceForm& form : forms) {
        if (form.language == language && form.kind == kind && form.pattern == pattern) {
            return true;
        }
    }
    return false;
}

constexpr std::array<SurfaceForm, 1> kVersionDirectiveGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kDirective, "#version 450 core",
         "Representative GLSL version directive spelling."},
}};

constexpr std::array<SurfaceForm, 1> kExtensionDirectiveGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kDirective, "#extension GL_EXT_name : require",
         "Representative GLSL extension directive spelling."},
}};

constexpr std::array<SurfaceForm, 2> kLayoutQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kLayoutQualifier, "layout(location = N)",
         "Location-qualified declaration."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kLayoutQualifier,
         "layout(binding = N, set = M, std140|std430, constant_id = K)",
         "Resource/layout qualifier family relevant to reverse parsing."},
}};

constexpr std::array<SurfaceForm, 1> kPrecisionQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kDirective, "precision mediump float;",
         "Representative GLSL precision declaration."},
}};

constexpr std::array<SurfaceForm, 4> kStorageQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kStorageQualifier, "in",
         "Stage input qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kStorageQualifier, "out",
         "Stage output qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kStorageQualifier, "uniform",
         "Uniform storage qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kStorageQualifier, "buffer",
         "SSBO storage qualifier; may be lossy or unsupported in SkSL."},
}};

constexpr std::array<SurfaceForm, 3> kParameterQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kParameterQualifier, "in",
         "Function parameter input qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kParameterQualifier, "out",
         "Function parameter output qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kParameterQualifier, "inout",
         "Function parameter input/output qualifier."},
}};

constexpr std::array<SurfaceForm, 5> kInterpolationQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterpolationQualifier, "smooth",
         "Default interpolation qualifier when written explicitly."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterpolationQualifier, "flat",
         "Flat interpolation qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterpolationQualifier, "noperspective",
         "No-perspective interpolation qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterpolationQualifier, "centroid",
         "Centroid interpolation auxiliary qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterpolationQualifier, "sample",
         "Per-sample interpolation auxiliary qualifier."},
}};

constexpr std::array<SurfaceForm, 5> kMemoryQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kMemoryQualifier, "readonly",
         "Read-only image/buffer qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kMemoryQualifier, "writeonly",
         "Write-only image/buffer qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kMemoryQualifier, "coherent",
         "Memory coherence qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kMemoryQualifier, "volatile",
         "Volatile memory qualifier."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kMemoryQualifier, "restrict",
         "Aliasing restriction qualifier."},
}};

constexpr std::array<SurfaceForm, 3> kInterfaceBlockGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterfaceBlock,
         "uniform BlockName { ... } instance;",
         "Uniform interface block declaration."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterfaceBlock,
         "buffer BlockName { ... } instance;",
         "Storage-buffer interface block declaration."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInterfaceBlock,
         "in BlockName { ... } instance;",
         "Stage-interface block declaration."},
}};

constexpr std::array<SurfaceForm, 2> kInvariantPreciseQualifierGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kAuxiliaryQualifier, "invariant",
         "Invariant qualifier for outputs or block members."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kAuxiliaryQualifier, "precise",
         "Precision-control qualifier; may not have a direct SkSL analogue."},
}};

constexpr std::array<SurfaceForm, 1> kUniformShaderSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kResourceDeclaration, "uniform shader child;",
         "SkSL runtime-effect child declaration."},
}};
constexpr std::array<SurfaceForm, 2> kUniformShaderGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kResourceDeclaration, "uniform sampler2D child;",
         "Common forward lowering target."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kResourceDeclaration,
         "uniform sampler2D child; /* plus auxiliary metadata for child pipeline */",
         "When plain sampler state is insufficient, provenance must carry child metadata."},
}};

constexpr std::array<SurfaceForm, 1> kChildEvalSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "child.eval(coords)",
         "SkSL child effect evaluation."},
}};
constexpr std::array<SurfaceForm, 2> kChildEvalGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "texture(child, uv)",
         "Common forward lowering target."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kHelperCall, "skslEvalChild(child, coords)",
         "Helper-based lowering for pipelines that need extra metadata or color management."},
}};

constexpr std::array<SurfaceForm, 1> kFragCoordResolvedAliasGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kBuiltinSetup,
         "vec4 sk_FragCoord = vec4(gl_FragCoord.x, rtFlip.x + rtFlip.y * gl_FragCoord.y, ...)",
         "Normalized helper setup for resolved fragment coordinates."},
}};

constexpr std::array<SurfaceForm, 1> kClockwiseAliasGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInjectedAlias,
         "bool sk_Clockwise = gl_FrontFacing; /* optional inversion */",
         "Normalized helper setup for resolved front-facing state."},
}};

constexpr std::array<SurfaceForm, 1> kScalarToVectorCanonicalizationGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall, "vecN(scalar)",
         "Canonicalized scalar-to-vector splat emitted in specific builtin argument positions."},
}};

constexpr std::array<SurfaceForm, 2> kSamplerImageResourceModelGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kResourceDeclaration, "uniform sampler2D tex;",
         "Combined texture/sampler resource."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kResourceDeclaration,
         "layout(rgba32f) uniform coherent image2D image;",
         "Image resource declaration with qualifiers."},
}};

constexpr std::array<FeatureBinding, 16> kFeatureBindings{{
        {FeatureId::kVersionDirective, "VersionDirective", FeatureCategory::kPreprocessor,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kVersionDirectiveGlslForms, {}, "Track GLSL version/profile as a first-class surface feature."},

        {FeatureId::kExtensionDirective, "ExtensionDirective", FeatureCategory::kPreprocessor,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kExtensionDirectiveGlslForms, {}, "Extension directives survive as source-level features rather than AST nodes."},

        {FeatureId::kLayoutQualifier, "LayoutQualifier", FeatureCategory::kQualifierSyntax,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kLayoutQualifierGlslForms, {}, "Layout syntax carries binding/resource metadata that should be centralized."},

        {FeatureId::kPrecisionQualifier, "PrecisionQualifier", FeatureCategory::kQualifierSyntax,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kNormalized,
         kAnyStageMask, GlslDialectBit(GlslDialect::kGLES),
         {}, kPrecisionQualifierGlslForms, {}, "Precision syntax is primarily relevant on GLES-style targets."},

        {FeatureId::kStorageQualifier, "StorageQualifier", FeatureCategory::kQualifierSyntax,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kStorageQualifierGlslForms, {},
         "Storage qualifiers drive declaration semantics and need centralized reverse handling."},

        {FeatureId::kParameterQualifier, "ParameterQualifier", FeatureCategory::kQualifierSyntax,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kParameterQualifierGlslForms, {},
         "Function parameter direction qualifiers should not be treated as ad-hoc parser detail."},

        {FeatureId::kInterpolationQualifier, "InterpolationQualifier",
         FeatureCategory::kQualifierSyntax, FeatureHandling::kDirect,
         FeatureHandling::kDirect, RoundTripKind::kNormalized,
         StageBit(ShaderStage::kVertex) | StageBit(ShaderStage::kFragment),
         kAnyGlslDialectMask, {}, kInterpolationQualifierGlslForms, {},
         "Interpolation qualifiers affect pipeline IO semantics and must survive round-trip normalization."},

        {FeatureId::kMemoryQualifier, "MemoryQualifier", FeatureCategory::kQualifierSyntax,
         FeatureHandling::kDirect, FeatureHandling::kDirect, RoundTripKind::kLossy,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kMemoryQualifierGlslForms, {},
         "Image/buffer memory qualifiers often exceed SkSL surface syntax and may only round-trip approximately."},

        {FeatureId::kInterfaceBlock, "InterfaceBlock", FeatureCategory::kInterfaceSyntax,
         FeatureHandling::kDirect, FeatureHandling::kRewrite, RoundTripKind::kLossy,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kInterfaceBlockGlslForms, {},
         "GLSL interface blocks should be classified explicitly so reverse conversion can decide whether to lower, flatten, or reject them."},

        {FeatureId::kInvariantPreciseQualifier, "InvariantPreciseQualifier",
         FeatureCategory::kQualifierSyntax, FeatureHandling::kDirect,
         FeatureHandling::kDirect, RoundTripKind::kLossy,
         kAnyStageMask, kAnyGlslDialectMask,
         {}, kInvariantPreciseQualifierGlslForms, {},
         "GLSL invariant/precise qualifiers are source-visible features even when target SkSL support is partial."},

        {FeatureId::kUniformShader, "UniformShader", FeatureCategory::kRuntimeEffect,
         FeatureHandling::kRewrite, FeatureHandling::kSidebandOnly, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         kUniformShaderSkslForms, kUniformShaderGlslForms, "runtime_effect_child",
         "SkSL runtime-effect child types do not have a direct GLSL surface equivalent."},

        {FeatureId::kChildEval, "ChildEval", FeatureCategory::kRuntimeEffect,
         FeatureHandling::kRewrite, FeatureHandling::kSidebandOnly, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         kChildEvalSkslForms, kChildEvalGlslForms, "runtime_effect_child_eval",
         "texture(...) alone is not enough to prove a GLSL call came from child.eval(...)." },

        {FeatureId::kFragCoordResolvedAlias, "FragCoordResolvedAlias", FeatureCategory::kBuiltinSetup,
         FeatureHandling::kRewriteWithHelper, FeatureHandling::kRewrite, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         {}, kFragCoordResolvedAliasGlslForms, "fragcoord_setup",
         "Recognize backend-generated fragment-coordinate aliases as a separate surface feature."},

        {FeatureId::kClockwiseAlias, "ClockwiseAlias", FeatureCategory::kBuiltinSetup,
         FeatureHandling::kRewriteWithHelper, FeatureHandling::kRewrite, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         {}, kClockwiseAliasGlslForms, "clockwise_setup",
         "Recognize backend-generated front-facing aliases as a separate surface feature."},

        {FeatureId::kScalarToVectorCanonicalization, "ScalarToVectorCanonicalization",
         FeatureCategory::kCanonicalization, FeatureHandling::kRewrite, FeatureHandling::kRewrite,
         RoundTripKind::kNormalized, kAnyStageMask, kAnyGlslDialectMask,
         {}, kScalarToVectorCanonicalizationGlslForms, "scalar_vector_splat",
         "Used by mixed overload intrinsics when a target consumer rejects scalar/vector forms."},

        {FeatureId::kSamplerImageResourceModel, "SamplerImageResourceModel",
         FeatureCategory::kResourceModel, FeatureHandling::kDirect, FeatureHandling::kDirect,
         RoundTripKind::kNormalized, kAnyStageMask, kAnyGlslDialectMask,
         {}, kSamplerImageResourceModelGlslForms, {},
         "Resource declarations and qualifiers should be treated as shared surface syntax, not ad-hoc special cases."},
}};

}  // namespace

std::span<const FeatureBinding> AllFeatureBindings() {
    return kFeatureBindings;
}

const FeatureBinding* FindFeature(FeatureId id) {
    for (const FeatureBinding& binding : kFeatureBindings) {
        if (binding.id == id) {
            return &binding;
        }
    }
    return nullptr;
}

const FeatureBinding* MatchFeatureBySurfaceForm(SurfaceLanguage language,
                                                SurfaceFormKind kind,
                                                std::string_view pattern) {
    for (const FeatureBinding& binding : kFeatureBindings) {
        if (HasSurfaceForm(binding.sksl_forms, language, kind, pattern) ||
            HasSurfaceForm(binding.glsl_forms, language, kind, pattern)) {
            return &binding;
        }
    }
    return nullptr;
}

bool FeatureAppliesToContext(const FeatureBinding& binding, const BindingContext& context) {
    return StageMaskMatches(binding.stage_mask, context.stage) &&
           GlslDialectMaskMatches(binding.glsl_dialect_mask, context.glsl_dialect);
}

}  // namespace sksl_glsl_binding
