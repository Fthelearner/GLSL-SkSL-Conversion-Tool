#ifndef SKSL_GLSL_BINDING_REGISTRY_FEATURE_BINDINGS_H
#define SKSL_GLSL_BINDING_REGISTRY_FEATURE_BINDINGS_H

#include "binding_registry/binding_types.h"

#include <span>
#include <string_view>

namespace sksl_glsl_binding {

enum class FeatureId : uint16_t {
    kVersionDirective = 0,
    kExtensionDirective,
    kLayoutQualifier,
    kPrecisionQualifier,
    kStorageQualifier,
    kParameterQualifier,
    kInterpolationQualifier,
    kMemoryQualifier,
    kInterfaceBlock,
    kInvariantPreciseQualifier,
    kUniformShader,
    kChildEval,
    kFragCoordResolvedAlias,
    kClockwiseAlias,
    kScalarToVectorCanonicalization,
    kSamplerImageResourceModel,
};

enum class FeatureCategory : uint8_t {
    kPreprocessor,
    kDeclarationSyntax,
    kQualifierSyntax,
    kInterfaceSyntax,
    kRuntimeEffect,
    kBuiltinSetup,
    kCanonicalization,
    kResourceModel,
};

enum class FeatureHandling : uint8_t {
    kDirect,
    kRewrite,
    kRewriteWithHelper,
    kSidebandOnly,
    kReject,
};

struct FeatureBinding {
    FeatureId id = FeatureId::kVersionDirective;
    std::string_view semantic_name;
    FeatureCategory category = FeatureCategory::kPreprocessor;
    FeatureHandling forward_handling = FeatureHandling::kDirect;
    FeatureHandling reverse_handling = FeatureHandling::kDirect;
    RoundTripKind round_trip = RoundTripKind::kExact;
    uint32_t stage_mask = kAnyStageMask;
    uint32_t glsl_dialect_mask = kAnyGlslDialectMask;
    std::span<const SurfaceForm> sksl_forms;
    std::span<const SurfaceForm> glsl_forms;
    std::string_view helper_key;
    std::string_view note;
};

std::span<const FeatureBinding> AllFeatureBindings();
const FeatureBinding* FindFeature(FeatureId id);
const FeatureBinding* MatchFeatureBySurfaceForm(SurfaceLanguage language,
                                                SurfaceFormKind kind,
                                                std::string_view pattern);
bool FeatureAppliesToContext(const FeatureBinding& binding, const BindingContext& context);

}  // namespace sksl_glsl_binding

#endif
