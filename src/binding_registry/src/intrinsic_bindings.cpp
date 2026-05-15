#include "binding_registry/intrinsic_bindings.h"

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

constexpr std::array<SurfaceForm, 1> kSinForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "sin(x)",
         "Homogeneous numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 1> kSinGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "sin(x)",
         "Direct GLSL builtin call."},
}};

constexpr std::array<SurfaceForm, 1> kCosForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "cos(x)",
         "Homogeneous numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 1> kCosGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "cos(x)",
         "Direct GLSL builtin call."},
}};

constexpr std::array<SurfaceForm, 1> kClampForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "clamp(x, minVal, maxVal)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kClampGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "clamp(x, minVal, maxVal)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall,
         "clamp(vecN_expr, vecN(scalarMin), vecN(scalarMax))",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kMinForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "min(x, y)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kMinGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "min(x, y)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall, "min(vecN_expr, vecN(scalar))",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kMaxForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "max(x, y)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kMaxGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "max(x, y)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall, "max(vecN_expr, vecN(scalar))",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kModForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "mod(x, y)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kModGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "mod(x, y)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall, "mod(vecN_expr, vecN(scalar))",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kStepForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "step(edge, x)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kStepGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "step(edge, x)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall,
         "step(vecN(edgeScalar), vecN_expr)",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kSmoothstepForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "smoothstep(edge0, edge1, x)",
         "Mixed scalar/vector numeric builtin call."},
}};
constexpr std::array<SurfaceForm, 2> kSmoothstepGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "smoothstep(edge0, edge1, x)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall,
         "smoothstep(vecN(edge0Scalar), vecN(edge1Scalar), vecN_expr)",
         "Canonicalized scalar-to-vector form emitted for stricter consumers."},
}};

constexpr std::array<SurfaceForm, 1> kMixForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "mix(x, y, a)",
         "Numeric mix call; selector may be scalar or vector."},
}};
constexpr std::array<SurfaceForm, 2> kMixGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "mix(x, y, a)",
         "Direct GLSL builtin call."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kConstructorCall,
         "mix(vecN_x, vecN_y, vecN(selectorScalar))",
         "Canonicalized scalar-to-vector selector form for numeric mix."},
}};

constexpr std::array<SurfaceForm, 1> kFloatBitsToIntForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "floatBitsToInt(x)",
         "Bit-cast intrinsic; parameter type differs from return type."},
}};
constexpr std::array<SurfaceForm, 1> kFloatBitsToIntGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "floatBitsToInt(x)",
         "Direct GLSL builtin call."},
}};

constexpr std::array<SurfaceForm, 1> kIntBitsToFloatForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "intBitsToFloat(x)",
         "Bit-cast intrinsic; parameter type differs from return type."},
}};
constexpr std::array<SurfaceForm, 1> kIntBitsToFloatGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "intBitsToFloat(x)",
         "Direct GLSL builtin call."},
}};

constexpr std::array<SurfaceForm, 1> kReflectForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "reflect(I, N)",
         "Geometric builtin call."},
}};
constexpr std::array<SurfaceForm, 1> kReflectGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "reflect(I, N)",
         "Direct GLSL builtin call."},
}};

constexpr std::array<SurfaceForm, 1> kRefractForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall, "refract(I, N, eta)",
         "Geometric builtin call; third argument is scalar."},
}};
constexpr std::array<SurfaceForm, 1> kRefractGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "refract(I, N, eta)",
         "Direct GLSL builtin call; eta should stay scalar."},
}};

// fwidth / fwidthFine / fwidthCoarse: GPU derivative functions unavailable in
// SkSL RuntimeEffect. Forward lowering emits an iResolution-based approximation:
//   fwidth(x) ≈ (abs(x) + 5.0) / min(iResolution.x, iResolution.y)
// The abs(x) term captures value-proportional per-pixel change; the +5.0 floor
// ensures anti-aliasing for small values. Empirically tuned against curve.frag.
// Provenance records kFunctionCall(helper_key="fwidth_approx", glsl_function="fwidth",
// sksl_method="fwidth_approx") for round-trip reversibility.
constexpr std::array<SurfaceForm, 1> kFwidthForms{{
        {SurfaceLanguage::kSkSL, SurfaceFormKind::kFunctionCall,
         "fwidth_approx(x)",
         "Resolution-based derivative approximation; requires iResolution uniform."},
}};
constexpr std::array<SurfaceForm, 2> kFwidthGlslForms{{
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "fwidth(p)",
         "Direct GLSL derivative builtin; replaced by approximate formula in SkSL."},
        {SurfaceLanguage::kGLSL, SurfaceFormKind::kFunctionCall, "fwidthFine(p)",
         "Fine derivative variant; same approximation as fwidth."},
}};

constexpr std::array<IntrinsicBinding, 16> kIntrinsicBindings{{
        {IntrinsicId::kSin, "Sin", IntrinsicFamily::kHomogeneousNumeric,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "sin", "sin", kSinForms, kSinGlslForms, "genType -> genType", {},
         "Homogeneous builtin; no scalar-to-vector canonicalization needed."},

        {IntrinsicId::kCos, "Cos", IntrinsicFamily::kHomogeneousNumeric,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "cos", "cos", kCosForms, kCosGlslForms, "genType -> genType", {},
         "Homogeneous builtin; no scalar-to-vector canonicalization needed."},

        {IntrinsicId::kClamp, "Clamp", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "clamp", "clamp", kClampForms, kClampGlslForms, "genType x genType x genType",
         "scalar_vector_splat",
         "When a target consumer rejects scalar/vector mixed overloads, forward emission may "
         "canonicalize scalar bounds to vecN(scalar)."},

        {IntrinsicId::kMin, "Min", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "min", "min", kMinForms, kMinGlslForms, "genType x genType", "scalar_vector_splat",
         "Mixed overload; canonicalization may be needed for stricter consumers."},

        {IntrinsicId::kMax, "Max", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "max", "max", kMaxForms, kMaxGlslForms, "genType x genType", "scalar_vector_splat",
         "Mixed overload; canonicalization may be needed for stricter consumers."},

        {IntrinsicId::kMod, "Mod", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "mod", "mod", kModForms, kModGlslForms, "genType x genType", "scalar_vector_splat",
         "Mixed overload; canonicalization may be needed for stricter consumers."},

        {IntrinsicId::kStep, "Step", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "step", "step", kStepForms, kStepGlslForms, "genType x genType", "scalar_vector_splat",
         "Mixed overload; canonicalization may be needed for stricter consumers."},

        {IntrinsicId::kSmoothstep, "Smoothstep", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "smoothstep", "smoothstep", kSmoothstepForms, kSmoothstepGlslForms,
         "genType x genType x genType", "scalar_vector_splat",
         "Mixed overload; canonicalization may be needed for stricter consumers."},

        {IntrinsicId::kMix, "Mix", IntrinsicFamily::kMixedScalarVector,
         IntrinsicLoweringKind::kCanonicalizedBuiltinCall, RoundTripKind::kNormalized,
         kAnyStageMask, kAnyGlslDialectMask,
         "mix", "mix", kMixForms, kMixGlslForms, "genType x genType x genType|bvec",
         "scalar_vector_splat",
         "Numeric mix may canonicalize a scalar selector to vecN(selector). Boolean-selector mix "
         "must stay separate in the caller."},

        {IntrinsicId::kFloatBitsToInt, "FloatBitsToInt", IntrinsicFamily::kBitCast,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "floatBitsToInt", "floatBitsToInt", kFloatBitsToIntForms, kFloatBitsToIntGlslForms,
         "float genType -> int genType", {},
         "Bit-cast intrinsic; never canonicalize arguments based on return type."},

        {IntrinsicId::kIntBitsToFloat, "IntBitsToFloat", IntrinsicFamily::kBitCast,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "intBitsToFloat", "intBitsToFloat", kIntBitsToFloatForms, kIntBitsToFloatGlslForms,
         "int genType -> float genType", {},
         "Bit-cast intrinsic; never canonicalize arguments based on return type."},

        {IntrinsicId::kReflect, "Reflect", IntrinsicFamily::kGeometric,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "reflect", "reflect", kReflectForms, kReflectGlslForms, "genType x genType -> genType",
         {}, "Geometric builtin; both arguments must remain genType."},

        {IntrinsicId::kRefract, "Refract", IntrinsicFamily::kGeometric,
         IntrinsicLoweringKind::kDirectBuiltinCall, RoundTripKind::kExact,
         kAnyStageMask, kAnyGlslDialectMask,
         "refract", "refract", kRefractForms, kRefractGlslForms,
         "genType x genType x scalar -> genType", {},
         "Geometric builtin; eta should stay scalar and must not be folded into vecN(eta)."},

        // fwidth: GPU derivative → resolution-based approximation
        {IntrinsicId::kFwidth, "Fwidth", IntrinsicFamily::kHomogeneousNumeric,
         IntrinsicLoweringKind::kHelperRewrite, RoundTripKind::kUnsupported,
         kAnyStageMask, kAnyGlslDialectMask,
         "", "fwidth", kFwidthForms, kFwidthGlslForms,
         "genType -> genType", "fwidth_approx",
         "Replaced with (abs(x)+5.0)/min(iResolution.xy) in SkSL; empirically tuned; provenance records mapping."},

        {IntrinsicId::kFwidthFine, "FwidthFine", IntrinsicFamily::kHomogeneousNumeric,
         IntrinsicLoweringKind::kHelperRewrite, RoundTripKind::kUnsupported,
         kAnyStageMask, kAnyGlslDialectMask,
         "", "fwidthFine", kFwidthForms, kFwidthGlslForms,
         "genType -> genType", "fwidth_approx",
         "Same approximation as fwidth."},

        {IntrinsicId::kFwidthCoarse, "FwidthCoarse", IntrinsicFamily::kHomogeneousNumeric,
         IntrinsicLoweringKind::kHelperRewrite, RoundTripKind::kUnsupported,
         kAnyStageMask, kAnyGlslDialectMask,
         "", "fwidthCoarse", kFwidthForms, kFwidthGlslForms,
         "genType -> genType", "fwidth_approx",
         "Same approximation as fwidth."},
}};

}  // namespace

std::span<const IntrinsicBinding> AllIntrinsicBindings() {
    return kIntrinsicBindings;
}

const IntrinsicBinding* FindIntrinsic(IntrinsicId id) {
    for (const IntrinsicBinding& binding : kIntrinsicBindings) {
        if (binding.id == id) {
            return &binding;
        }
    }
    return nullptr;
}

const IntrinsicBinding* MatchIntrinsicBySkslSpelling(std::string_view spelling) {
    for (const IntrinsicBinding& binding : kIntrinsicBindings) {
        if (binding.sksl_spelling == spelling) {
            return &binding;
        }
    }
    return nullptr;
}

const IntrinsicBinding* MatchIntrinsicByGlslPrimarySpelling(std::string_view spelling) {
    for (const IntrinsicBinding& binding : kIntrinsicBindings) {
        if (binding.glsl_primary_spelling == spelling) {
            return &binding;
        }
    }
    return nullptr;
}

const IntrinsicBinding* MatchIntrinsicBySurfaceForm(SurfaceLanguage language,
                                                    SurfaceFormKind kind,
                                                    std::string_view pattern) {
    for (const IntrinsicBinding& binding : kIntrinsicBindings) {
        if (HasSurfaceForm(binding.sksl_forms, language, kind, pattern) ||
            HasSurfaceForm(binding.glsl_forms, language, kind, pattern)) {
            return &binding;
        }
    }
    return nullptr;
}

bool IntrinsicAppliesToContext(const IntrinsicBinding& binding, const BindingContext& context) {
    return StageMaskMatches(binding.stage_mask, context.stage) &&
           GlslDialectMaskMatches(binding.glsl_dialect_mask, context.glsl_dialect);
}

}  // namespace sksl_glsl_binding
