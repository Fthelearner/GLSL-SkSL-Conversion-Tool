#include "binding_registry/builtin_bindings.h"

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

constexpr std::array<SurfaceForm, 1> kFragCoordSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_FragCoord",
         "Canonical SkSL fragment-coordinate builtin."},
}};
constexpr std::array<SurfaceForm, 2> kFragCoordGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_FragCoord",
         "Direct GLSL fragment-coordinate builtin."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kBuiltinSetup,
         "vec4(gl_FragCoord.x, rtFlip.x + rtFlip.y * gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w)",
         "Resolved fragment coordinate with RTFlip applied."},
}};

constexpr std::array<SurfaceForm, 1> kClockwiseSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_Clockwise",
         "Canonical SkSL front-facing builtin."},
}};
constexpr std::array<SurfaceForm, 2> kClockwiseGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_FrontFacing",
         "Direct GLSL front-facing builtin."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInjectedAlias,
         "bool sk_Clockwise = gl_FrontFacing; /* optional conditional inversion */",
         "Backend-generated alias used when coordinate-space adjustments are needed."},
}};

constexpr std::array<SurfaceForm, 1> kPositionForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_Position",
         "Canonical SkSL vertex-position builtin."},
}};
constexpr std::array<SurfaceForm, 1> kPositionGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_Position",
         "Direct GLSL vertex-position builtin."},
}};

constexpr std::array<SurfaceForm, 1> kPointSizeForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_PointSize",
         "Canonical SkSL point-size builtin."},
}};
constexpr std::array<SurfaceForm, 1> kPointSizeGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_PointSize",
         "Direct GLSL point-size builtin when supported by the target pipeline."},
}};

constexpr std::array<SurfaceForm, 1> kVertexIDSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_VertexID",
         "Canonical SkSL vertex ID builtin."},
}};
constexpr std::array<SurfaceForm, 2> kVertexIDGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_VertexID",
         "OpenGL / GLES builtin spelling."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_VertexIndex",
         "Vulkan GLSL builtin spelling."},
}};

constexpr std::array<SurfaceForm, 1> kInstanceIDSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_InstanceID",
         "Canonical SkSL instance ID builtin."},
}};
constexpr std::array<SurfaceForm, 2> kInstanceIDGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_InstanceID",
         "OpenGL / GLES builtin spelling."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_InstanceIndex",
         "Vulkan GLSL builtin spelling."},
}};

constexpr std::array<SurfaceForm, 1> kGlobalInvocationIDSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_GlobalInvocationID",
         "Canonical SkSL compute builtin."},
}};
constexpr std::array<SurfaceForm, 1> kGlobalInvocationIDGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_GlobalInvocationID",
         "Direct GLSL compute builtin."},
}};

constexpr std::array<SurfaceForm, 1> kLocalInvocationIDSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_LocalInvocationID",
         "Canonical SkSL compute builtin."},
}};
constexpr std::array<SurfaceForm, 1> kLocalInvocationIDGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_LocalInvocationID",
         "Direct GLSL compute builtin."},
}};

constexpr std::array<SurfaceForm, 1> kLocalInvocationIndexSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_LocalInvocationIndex",
         "Canonical SkSL compute builtin."},
}};
constexpr std::array<SurfaceForm, 1> kLocalInvocationIndexGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_LocalInvocationIndex",
         "Direct GLSL compute builtin."},
}};

constexpr std::array<SurfaceForm, 1> kNumWorkgroupsSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_NumWorkgroups",
         "Canonical SkSL compute builtin."},
}};
constexpr std::array<SurfaceForm, 1> kNumWorkgroupsGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_NumWorkGroups",
         "Direct GLSL compute builtin."},
}};

constexpr std::array<SurfaceForm, 1> kWorkgroupIDSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kIdentifier, "sk_WorkgroupID",
         "Canonical SkSL compute builtin."},
}};
constexpr std::array<SurfaceForm, 1> kWorkgroupIDGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kIdentifier, "gl_WorkGroupID",
         "Direct GLSL compute builtin."},
}};

constexpr std::array<SurfaceForm, 1> kCapsFloatIs32BitsSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kMemberAccess, "sk_Caps.floatIs32Bits",
         "SkSL capability field."},
}};
constexpr std::array<SurfaceForm, 1> kCapsFloatIs32BitsGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInjectedAlias, "__sksl_caps_floatIs32Bits",
         "Generated constant alias or folded literal; exact recovery generally needs provenance."},
}};

constexpr std::array<SurfaceForm, 1> kCapsIntegerSupportSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kMemberAccess, "sk_Caps.integerSupport",
         "SkSL capability field."},
}};
constexpr std::array<SurfaceForm, 1> kCapsIntegerSupportGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInjectedAlias, "__sksl_caps_integerSupport",
         "Generated constant alias or folded literal; exact recovery generally needs provenance."},
}};

constexpr std::array<SurfaceForm, 1> kCapsBuiltinDeterminantSupportSkslForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kMemberAccess,
         "sk_Caps.builtinDeterminantSupport", "SkSL capability field."},
}};
constexpr std::array<SurfaceForm, 1> kCapsBuiltinDeterminantSupportGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kInjectedAlias,
         "__sksl_caps_builtinDeterminantSupport",
         "Generated constant alias or folded literal; exact recovery generally needs provenance."},
}};

constexpr std::array<BuiltinBinding, 14> kBuiltinBindings{{
        {BuiltinId::kFragCoord, "FragCoord", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kComputedAlias, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         "sk_FragCoord", "gl_FragCoord", kFragCoordSkslForms, kFragCoordGlslForms,
         "fragcoord_setup",
         "Forward emission may need RTFlip/workaround setup; reverse matching should recognize "
         "both direct and resolved forms."},

        {BuiltinId::kClockwise, "Clockwise", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kComputedAlias, RoundTripKind::kViaProvenance,
         StageBit(ShaderStage::kFragment), kAnyGlslDialectMask,
         "sk_Clockwise", "gl_FrontFacing", kClockwiseSkslForms, kClockwiseGlslForms,
         "clockwise_setup",
         "Forward emission may need a backend-generated alias from gl_FrontFacing."},

        {BuiltinId::kPosition, "Position", BuiltinCategory::kStageOutput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kExact,
         StageBit(ShaderStage::kVertex), kAnyGlslDialectMask,
         "sk_Position", "gl_Position", kPositionForms, kPositionGlslForms, {},
         "Direct builtin output mapping."},

        {BuiltinId::kPointSize, "PointSize", BuiltinCategory::kStageOutput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kNormalized,
         StageBit(ShaderStage::kVertex), kAnyGlslDialectMask,
         "sk_PointSize", "gl_PointSize", kPointSizeForms, kPointSizeGlslForms, {},
         "Supported directly in GLSL but may require target-specific validation."},

        {BuiltinId::kVertexID, "VertexID", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kNormalized,
         StageBit(ShaderStage::kVertex), kAnyGlslDialectMask,
         "sk_VertexID", "gl_VertexID", kVertexIDSkslForms, kVertexIDGlslForms, {},
         "Dialect-dependent GLSL spelling; reverse should normalize both spellings to one semantic "
         "ID."},

        {BuiltinId::kInstanceID, "InstanceID", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kNormalized,
         StageBit(ShaderStage::kVertex), kAnyGlslDialectMask,
         "sk_InstanceID", "gl_InstanceID", kInstanceIDSkslForms, kInstanceIDGlslForms, {},
         "Dialect-dependent GLSL spelling; reverse should normalize both spellings to one semantic "
         "ID."},

        {BuiltinId::kGlobalInvocationID, "GlobalInvocationID", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kExact,
         StageBit(ShaderStage::kCompute), kAnyGlslDialectMask,
         "sk_GlobalInvocationID", "gl_GlobalInvocationID",
         kGlobalInvocationIDSkslForms, kGlobalInvocationIDGlslForms, {},
         "Direct compute builtin mapping."},

        {BuiltinId::kLocalInvocationID, "LocalInvocationID", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kExact,
         StageBit(ShaderStage::kCompute), kAnyGlslDialectMask,
         "sk_LocalInvocationID", "gl_LocalInvocationID",
         kLocalInvocationIDSkslForms, kLocalInvocationIDGlslForms, {},
         "Direct compute builtin mapping."},

        {BuiltinId::kLocalInvocationIndex, "LocalInvocationIndex",
         BuiltinCategory::kStageInput, BuiltinLoweringKind::kDirectName,
         RoundTripKind::kExact, StageBit(ShaderStage::kCompute), kAnyGlslDialectMask,
         "sk_LocalInvocationIndex", "gl_LocalInvocationIndex",
         kLocalInvocationIndexSkslForms, kLocalInvocationIndexGlslForms, {},
         "Direct compute builtin mapping."},

        {BuiltinId::kNumWorkgroups, "NumWorkgroups", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kExact,
         StageBit(ShaderStage::kCompute), kAnyGlslDialectMask,
         "sk_NumWorkgroups", "gl_NumWorkGroups", kNumWorkgroupsSkslForms,
         kNumWorkgroupsGlslForms, {}, "Direct compute builtin mapping."},

        {BuiltinId::kWorkgroupID, "WorkgroupID", BuiltinCategory::kStageInput,
         BuiltinLoweringKind::kDirectName, RoundTripKind::kExact,
         StageBit(ShaderStage::kCompute), kAnyGlslDialectMask,
         "sk_WorkgroupID", "gl_WorkGroupID", kWorkgroupIDSkslForms, kWorkgroupIDGlslForms, {},
         "Direct compute builtin mapping."},

        {BuiltinId::kCapsFloatIs32Bits, "CapsFloatIs32Bits", BuiltinCategory::kCapsField,
         BuiltinLoweringKind::kCompileTimeConstant, RoundTripKind::kViaProvenance,
         kAnyStageMask, kAnyGlslDialectMask,
         "sk_Caps.floatIs32Bits", "__sksl_caps_floatIs32Bits",
         kCapsFloatIs32BitsSkslForms, kCapsFloatIs32BitsGlslForms, "caps_constant",
         "Capability fields are best treated as semantic constants rather than ordinary variables."},

        {BuiltinId::kCapsIntegerSupport, "CapsIntegerSupport", BuiltinCategory::kCapsField,
         BuiltinLoweringKind::kCompileTimeConstant, RoundTripKind::kViaProvenance,
         kAnyStageMask, kAnyGlslDialectMask,
         "sk_Caps.integerSupport", "__sksl_caps_integerSupport",
         kCapsIntegerSupportSkslForms, kCapsIntegerSupportGlslForms, "caps_constant",
         "Capability fields are best treated as semantic constants rather than ordinary variables."},

        {BuiltinId::kCapsBuiltinDeterminantSupport, "CapsBuiltinDeterminantSupport",
         BuiltinCategory::kCapsField, BuiltinLoweringKind::kCompileTimeConstant,
         RoundTripKind::kViaProvenance, kAnyStageMask, kAnyGlslDialectMask,
         "sk_Caps.builtinDeterminantSupport", "__sksl_caps_builtinDeterminantSupport",
         kCapsBuiltinDeterminantSupportSkslForms, kCapsBuiltinDeterminantSupportGlslForms,
         "caps_constant",
         "Capability fields are best treated as semantic constants rather than ordinary variables."},
}};

}  // namespace

std::span<const BuiltinBinding> AllBuiltinBindings() {
    return kBuiltinBindings;
}

const BuiltinBinding* FindBuiltin(BuiltinId id) {
    for (const BuiltinBinding& binding : kBuiltinBindings) {
        if (binding.id == id) {
            return &binding;
        }
    }
    return nullptr;
}

const BuiltinBinding* MatchBuiltinBySkslSpelling(std::string_view spelling) {
    for (const BuiltinBinding& binding : kBuiltinBindings) {
        if (binding.sksl_spelling == spelling) {
            return &binding;
        }
    }
    return nullptr;
}

const BuiltinBinding* MatchBuiltinByGlslPrimarySpelling(std::string_view spelling) {
    for (const BuiltinBinding& binding : kBuiltinBindings) {
        if (binding.glsl_primary_spelling == spelling) {
            return &binding;
        }
    }
    return nullptr;
}

const BuiltinBinding* MatchBuiltinBySurfaceForm(SurfaceLanguage language,
                                                SurfaceFormKind kind,
                                                std::string_view pattern) {
    for (const BuiltinBinding& binding : kBuiltinBindings) {
        if (HasSurfaceForm(binding.sksl_forms, language, kind, pattern) ||
            HasSurfaceForm(binding.glsl_forms, language, kind, pattern)) {
            return &binding;
        }
    }
    return nullptr;
}

bool BuiltinAppliesToContext(const BuiltinBinding& binding, const BindingContext& context) {
    return StageMaskMatches(binding.stage_mask, context.stage) &&
           GlslDialectMaskMatches(binding.glsl_dialect_mask, context.glsl_dialect);
}

}  // namespace sksl_glsl_binding
