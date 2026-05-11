#ifndef SKSL_GLSL_BINDING_REGISTRY_BUILTIN_BINDINGS_H
#define SKSL_GLSL_BINDING_REGISTRY_BUILTIN_BINDINGS_H

#include "binding_registry/binding_types.h"

#include <span>
#include <string_view>

namespace sksl_glsl_binding {

enum class BuiltinId : uint16_t {
    kFragCoord = 0,
    kClockwise,
    kPosition,
    kPointSize,
    kVertexID,
    kInstanceID,
    kGlobalInvocationID,
    kLocalInvocationID,
    kLocalInvocationIndex,
    kNumWorkgroups,
    kWorkgroupID,
    kCapsFloatIs32Bits,
    kCapsIntegerSupport,
    kCapsBuiltinDeterminantSupport,
};

enum class BuiltinCategory : uint8_t {
    kStageInput,
    kStageOutput,
    kPipelinePseudoVar,
    kCapsField,
};

enum class BuiltinLoweringKind : uint8_t {
    kDirectName,
    kComputedAlias,
    kCompileTimeConstant,
    kInjectedHelper,
    kSidebandOnly,
    kUnsupported,
};

struct BuiltinBinding {
    BuiltinId id = BuiltinId::kFragCoord;
    std::string_view semantic_name;
    BuiltinCategory category = BuiltinCategory::kStageInput;
    BuiltinLoweringKind forward_lowering = BuiltinLoweringKind::kDirectName;
    RoundTripKind round_trip = RoundTripKind::kExact;
    uint32_t stage_mask = kAnyStageMask;
    uint32_t glsl_dialect_mask = kAnyGlslDialectMask;
    std::string_view sksl_spelling;
    std::string_view glsl_primary_spelling;
    std::span<const SurfaceForm> sksl_forms;
    std::span<const SurfaceForm> glsl_forms;
    std::string_view helper_key;
    std::string_view note;
};

std::span<const BuiltinBinding> AllBuiltinBindings();
const BuiltinBinding* FindBuiltin(BuiltinId id);
const BuiltinBinding* MatchBuiltinBySkslSpelling(std::string_view spelling);
const BuiltinBinding* MatchBuiltinByGlslPrimarySpelling(std::string_view spelling);
const BuiltinBinding* MatchBuiltinBySurfaceForm(SurfaceLanguage language,
                                                SurfaceFormKind kind,
                                                std::string_view pattern);
bool BuiltinAppliesToContext(const BuiltinBinding& binding, const BindingContext& context);

}  // namespace sksl_glsl_binding

#endif
