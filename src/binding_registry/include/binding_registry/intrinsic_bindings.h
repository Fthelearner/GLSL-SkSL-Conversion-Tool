#ifndef SKSL_GLSL_BINDING_REGISTRY_INTRINSIC_BINDINGS_H
#define SKSL_GLSL_BINDING_REGISTRY_INTRINSIC_BINDINGS_H

#include "binding_registry/binding_types.h"

#include <span>
#include <string_view>

namespace sksl_glsl_binding {

enum class IntrinsicId : uint16_t {
    kSin = 0,
    kCos,
    kClamp,
    kMin,
    kMax,
    kMod,
    kStep,
    kSmoothstep,
    kMix,
    kFloatBitsToInt,
    kIntBitsToFloat,
    kReflect,
    kRefract,
};

enum class IntrinsicFamily : uint8_t {
    kHomogeneousNumeric,
    kMixedScalarVector,
    kBooleanSelector,
    kBitCast,
    kGeometric,
};

enum class IntrinsicLoweringKind : uint8_t {
    kDirectBuiltinCall,
    kCanonicalizedBuiltinCall,
    kOperatorBuiltin,
    kHelperRewrite,
    kUnsupported,
};

struct IntrinsicBinding {
    IntrinsicId id = IntrinsicId::kSin;
    std::string_view semantic_name;
    IntrinsicFamily family = IntrinsicFamily::kHomogeneousNumeric;
    IntrinsicLoweringKind forward_lowering = IntrinsicLoweringKind::kDirectBuiltinCall;
    RoundTripKind round_trip = RoundTripKind::kExact;
    uint32_t stage_mask = kAnyStageMask;
    uint32_t glsl_dialect_mask = kAnyGlslDialectMask;
    std::string_view sksl_spelling;
    std::string_view glsl_primary_spelling;
    std::span<const SurfaceForm> sksl_forms;
    std::span<const SurfaceForm> glsl_forms;
    std::string_view signature_family;
    std::string_view helper_key;
    std::string_view note;
};

std::span<const IntrinsicBinding> AllIntrinsicBindings();
const IntrinsicBinding* FindIntrinsic(IntrinsicId id);
const IntrinsicBinding* MatchIntrinsicBySkslSpelling(std::string_view spelling);
const IntrinsicBinding* MatchIntrinsicByGlslPrimarySpelling(std::string_view spelling);
const IntrinsicBinding* MatchIntrinsicBySurfaceForm(SurfaceLanguage language,
                                                    SurfaceFormKind kind,
                                                    std::string_view pattern);
bool IntrinsicAppliesToContext(const IntrinsicBinding& binding, const BindingContext& context);

}  // namespace sksl_glsl_binding

#endif
