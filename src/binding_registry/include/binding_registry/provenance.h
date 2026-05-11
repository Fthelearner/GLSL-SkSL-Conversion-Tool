#ifndef SKSL_GLSL_BINDING_REGISTRY_PROVENANCE_H
#define SKSL_GLSL_BINDING_REGISTRY_PROVENANCE_H

#include "binding_registry/binding_types.h"
#include "binding_registry/feature_bindings.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sksl_glsl_binding {

struct BuiltinBinding;
struct IntrinsicBinding;

// ============================================================================
// Sideband provenance config file
//
// Forward translation (SKSL→GLSL) records lowering decisions in a JSON
// sideband file (shader.glsl.provenance). The reverse translator reads
// this file to recover SKSL semantics that have no direct GLSL surface form.
//
// No provenance markers are embedded in the GLSL/SKSL output itself —
// the sideband file is the sole carrier of round-trip metadata.
// ============================================================================

// Metadata about the conversion context, written once per provenance file.
// The reverse translator uses this to reconstruct the correct BindingContext.
struct ProvenanceMetadata {
    std::string source_program_kind;   // e.g. "kRuntimeShader", "kFragment"
    std::string shader_stage;          // e.g. "kFragment", "kVertex", "kCompute"
    std::string glsl_dialect;          // e.g. "kOpenGLCore", "kGLES", "kVulkanGLSL"
    int glsl_version = 450;
    bool runtime_effect_mode = false;
    bool use_rt_flip = false;
    bool use_fragcoord_workaround = false;

    bool empty() const { return source_program_kind.empty(); }
};

// One recorded lowering decision.
struct ProvenanceEntry {
    enum class Kind : uint8_t {
        kVariableType,     // uniform shader → uniform sampler2D
        kFunctionCall,     // child.eval() → texture()
        kBuiltinVariable,  // sk_FragCoord → gl_FragCoord
        kCapabilityField,  // sk_Caps.xxx → constant
    };

    Kind kind = Kind::kBuiltinVariable;
    std::string helper_key;  // links to binding_registry entries

    // For kVariableType
    std::string variable_name;
    std::string sksl_type;
    std::string glsl_type;

    // For kFunctionCall
    std::string callee_name;
    std::string sksl_method;     // e.g. "eval"
    std::string glsl_function;   // e.g. "texture"
    std::string child_type;      // e.g. "kShader", "kColorFilter", "kBlender" — disambiguates child effect kind

    // For kBuiltinVariable
    std::string sksl_name;
    std::string glsl_name;

    // For kCapabilityField
    std::string sksl_field;      // e.g. "sk_Caps.floatIs32Bits"
    std::string glsl_constant;   // e.g. "__sksl_caps_floatIs32Bits"
};

// ============================================================================
// ProvenanceConfig — the in-memory representation of a sideband file
// ============================================================================

class ProvenanceConfig {
public:
    ProvenanceConfig() = default;

    // --- Metadata ---

    void setMetadata(const ProvenanceMetadata& meta) { metadata_ = meta; }
    const ProvenanceMetadata& metadata() const { return metadata_; }
    bool hasMetadata() const { return !metadata_.empty(); }

    // --- Building (forward translation) ---

    void recordVariableType(std::string_view name,
                            std::string_view sksl_type,
                            std::string_view glsl_type,
                            std::string_view helper_key);

    void recordFunctionCall(std::string_view callee,
                            std::string_view sksl_method,
                            std::string_view glsl_function,
                            std::string_view helper_key,
                            std::string_view child_type = {});

    void recordBuiltinVariable(std::string_view sksl_name,
                               std::string_view glsl_name,
                               std::string_view helper_key);

    void recordCapabilityField(std::string_view sksl_field,
                               std::string_view glsl_constant,
                               std::string_view helper_key);

    bool empty() const { return entries_.empty() && metadata_.empty(); }
    const std::vector<ProvenanceEntry>& entries() const { return entries_; }

    // --- Serialization ---

    // Serialize to JSON string.
    std::string toJson() const;

    // Parse from JSON string. Returns true on success.
    bool fromJson(std::string_view json);

    // --- Lookup (reverse translation) ---

    // Find variable-type entries by GLSL name.
    const ProvenanceEntry* findVariableType(std::string_view glsl_name) const;

    // Find function-call entries by callee name + GLSL function.
    const ProvenanceEntry* findFunctionCall(std::string_view callee,
                                            std::string_view glsl_function) const;

    // Find builtin-variable entries by GLSL name.
    const ProvenanceEntry* findBuiltinVariable(std::string_view glsl_name) const;

    // Find capability-field entries by GLSL constant name.
    const ProvenanceEntry* findCapabilityField(std::string_view glsl_constant) const;

    // Check whether a helper_key is present in any entry.
    bool hasHelperKey(std::string_view helper_key) const;

private:
    ProvenanceMetadata metadata_;
    std::vector<ProvenanceEntry> entries_;

    // Indexes for fast lookup
    std::unordered_map<std::string, size_t> var_type_by_glsl_name_;
    std::unordered_map<std::string, size_t> func_call_by_callee_func_;
    std::unordered_map<std::string, size_t> builtin_by_glsl_name_;
    std::unordered_map<std::string, size_t> caps_by_glsl_constant_;
    std::unordered_set<std::string> helper_keys_;  // O(1) lookup for hasHelperKey
};

// ============================================================================
// ReverseMapping — maps GLSL surface forms back to SKSL via the registry
// ============================================================================

class ReverseMapping {
public:
    explicit ReverseMapping(const ProvenanceConfig* config = nullptr)
        : config_(config) {}

    // Given a GLSL identifier, attempt to find the canonical SKSL form.
    std::string mapIdentifier(std::string_view glsl_identifier,
                              const BindingContext& context) const;

    // Map a GLSL built-in function call to its SKSL equivalent.
    std::string mapFunctionCall(std::string_view glsl_function_name,
                                const BindingContext& context) const;

    // Check if a texture() call should be recovered to child.eval().
    bool shouldRecoverChildEval(std::string_view callee_name) const;

    // Access the underlying config (for metadata queries, etc.)
    const ProvenanceConfig* config() const { return config_; }

private:
    const ProvenanceConfig* config_ = nullptr;
};

}  // namespace sksl_glsl_binding

#endif
