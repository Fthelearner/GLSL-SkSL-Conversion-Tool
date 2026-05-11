#ifndef SKSL_GLSL_BINDING_REGISTRY_BINDING_TYPES_H
#define SKSL_GLSL_BINDING_REGISTRY_BINDING_TYPES_H

#include <cstdint>
#include <string_view>

namespace sksl_glsl_binding {

enum class SurfaceLanguage : uint8_t {
    kSkSL = 0,
    kGLSL = 1,
};

enum class ShaderStage : uint8_t {
    kVertex = 0,
    kFragment = 1,
    kCompute = 2,
};

enum class GlslDialect : uint8_t {
    kOpenGLCore = 0,
    kGLES = 1,
    kVulkanGLSL = 2,
};

enum class SemanticDomain : uint8_t {
    kBuiltin = 0,
    kIntrinsic = 1,
    kFeature = 2,
};

enum class SurfaceFormKind : uint8_t {
    kIdentifier,
    kMemberAccess,
    kFunctionCall,
    kConstructorCall,
    kLayoutQualifier,
    kStorageQualifier,
    kParameterQualifier,
    kInterpolationQualifier,
    kMemoryQualifier,
    kAuxiliaryQualifier,
    kInterfaceBlock,
    kDirective,
    kResourceDeclaration,
    kInjectedAlias,
    kHelperCall,
    kBuiltinSetup,
    kUnsupportedPattern,
};

enum class RoundTripKind : uint8_t {
    kExact,
    kNormalized,
    kViaProvenance,
    kLossy,
    kUnsupported,
};

struct BindingContext {
    ShaderStage stage = ShaderStage::kFragment;
    GlslDialect glsl_dialect = GlslDialect::kOpenGLCore;
    int glsl_version = 450;
    bool runtime_effect_mode = false;
    bool use_rt_flip = false;
    bool use_fragcoord_workaround = false;
    bool normalize_scalar_vector_builtins = true;
};

struct SurfaceForm {
    SurfaceLanguage language = SurfaceLanguage::kSkSL;
    SurfaceFormKind kind = SurfaceFormKind::kIdentifier;
    std::string_view pattern;
    std::string_view note;
};

struct BindingKey {
    SemanticDomain domain = SemanticDomain::kBuiltin;
    uint32_t id = 0;
};

struct ProvenanceRecord {
    BindingKey key;
    RoundTripKind round_trip = RoundTripKind::kUnsupported;
    std::string_view emitted_form_key;
    std::string_view helper_key;
    int output_begin = -1;
    int output_end = -1;
    std::string_view note;
};

constexpr uint32_t StageBit(ShaderStage stage) {
    return 1u << static_cast<uint8_t>(stage);
}

constexpr uint32_t GlslDialectBit(GlslDialect dialect) {
    return 1u << static_cast<uint8_t>(dialect);
}

constexpr uint32_t kAnyStageMask =
        StageBit(ShaderStage::kVertex) |
        StageBit(ShaderStage::kFragment) |
        StageBit(ShaderStage::kCompute);

constexpr uint32_t kAnyGlslDialectMask =
        GlslDialectBit(GlslDialect::kOpenGLCore) |
        GlslDialectBit(GlslDialect::kGLES) |
        GlslDialectBit(GlslDialect::kVulkanGLSL);

inline bool StageMaskMatches(uint32_t mask, ShaderStage stage) {
    return (mask & StageBit(stage)) != 0;
}

inline bool GlslDialectMaskMatches(uint32_t mask, GlslDialect dialect) {
    return (mask & GlslDialectBit(dialect)) != 0;
}

}  // namespace sksl_glsl_binding

#endif
