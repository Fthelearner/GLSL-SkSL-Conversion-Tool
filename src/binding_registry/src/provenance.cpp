#include "binding_registry/provenance.h"
#include "binding_registry/builtin_bindings.h"
#include "binding_registry/intrinsic_bindings.h"
#include "binding_registry/feature_bindings.h"

#include <charconv>
#include <sstream>
#include <system_error>

namespace sksl_glsl_binding {

// ============================================================================
// ProvenanceConfig — building
// ============================================================================

void ProvenanceConfig::recordVariableType(std::string_view name,
                                          std::string_view sksl_type,
                                          std::string_view glsl_type,
                                          std::string_view helper_key) {
    ProvenanceEntry e;
    e.kind = ProvenanceEntry::Kind::kVariableType;
    e.helper_key = helper_key;
    e.variable_name = name;
    e.sksl_type = sksl_type;
    e.glsl_type = glsl_type;
    size_t idx = entries_.size();
    entries_.push_back(std::move(e));
    var_type_by_glsl_name_[std::string(name)] = idx;
    if (!helper_key.empty()) helper_keys_.insert(std::string(helper_key));
}

void ProvenanceConfig::recordFunctionCall(std::string_view callee,
                                          std::string_view sksl_method,
                                          std::string_view glsl_function,
                                          std::string_view helper_key,
                                          std::string_view child_type) {
    ProvenanceEntry e;
    e.kind = ProvenanceEntry::Kind::kFunctionCall;
    e.helper_key = helper_key;
    e.callee_name = callee;
    e.sksl_method = sksl_method;
    e.glsl_function = glsl_function;
    e.child_type = child_type;
    size_t idx = entries_.size();
    entries_.push_back(std::move(e));
    std::string key(callee);
    key += "::";
    key += glsl_function;
    func_call_by_callee_func_[key] = idx;
    if (!helper_key.empty()) helper_keys_.insert(std::string(helper_key));
}

void ProvenanceConfig::recordBuiltinVariable(std::string_view sksl_name,
                                             std::string_view glsl_name,
                                             std::string_view helper_key) {
    ProvenanceEntry e;
    e.kind = ProvenanceEntry::Kind::kBuiltinVariable;
    e.helper_key = helper_key;
    e.sksl_name = sksl_name;
    e.glsl_name = glsl_name;
    size_t idx = entries_.size();
    entries_.push_back(std::move(e));
    builtin_by_glsl_name_[std::string(glsl_name)] = idx;
    if (!helper_key.empty()) helper_keys_.insert(std::string(helper_key));
}

void ProvenanceConfig::recordCapabilityField(std::string_view sksl_field,
                                             std::string_view glsl_constant,
                                             std::string_view helper_key) {
    ProvenanceEntry e;
    e.kind = ProvenanceEntry::Kind::kCapabilityField;
    e.helper_key = helper_key;
    e.sksl_field = sksl_field;
    e.glsl_constant = glsl_constant;
    size_t idx = entries_.size();
    entries_.push_back(std::move(e));
    caps_by_glsl_constant_[std::string(glsl_constant)] = idx;
    if (!helper_key.empty()) helper_keys_.insert(std::string(helper_key));
}

// ============================================================================
// ProvenanceConfig — serialization (minimal JSON, no external dependencies)
// ============================================================================

namespace {

// Forward declarations for mutual recursion
void jsonSkipWhitespace(std::string_view json, size_t& pos);

void jsonEscape(std::string& out, std::string_view s) {
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
}

const char* entryKindString(ProvenanceEntry::Kind k) {
    switch (k) {
        case ProvenanceEntry::Kind::kVariableType:    return "variable_type";
        case ProvenanceEntry::Kind::kFunctionCall:     return "function_call";
        case ProvenanceEntry::Kind::kBuiltinVariable:  return "builtin_variable";
        case ProvenanceEntry::Kind::kCapabilityField:  return "capability_field";
    }
    return "unknown";
}

bool parseEntryKind(std::string_view s, ProvenanceEntry::Kind& out) {
    if (s == "variable_type")    { out = ProvenanceEntry::Kind::kVariableType;    return true; }
    if (s == "function_call")    { out = ProvenanceEntry::Kind::kFunctionCall;     return true; }
    if (s == "builtin_variable") { out = ProvenanceEntry::Kind::kBuiltinVariable;  return true; }
    if (s == "capability_field") { out = ProvenanceEntry::Kind::kCapabilityField;  return true; }
    return false;
}

// Rudimentary JSON string parser. Returns the unescaped string and advances pos.
// Skips leading whitespace before the opening quote.
std::string jsonParseString(std::string_view json, size_t& pos) {
    jsonSkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') return {};
    ++pos; // skip opening quote
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    if (pos < json.size() && json[pos] == '"') ++pos; // skip closing quote
    return result;
}

// Skip whitespace and return position of next non-whitespace.
void jsonSkipWhitespace(std::string_view json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
}

// Expect a specific character at pos, skip whitespace before it.
bool jsonExpect(std::string_view json, size_t& pos, char c) {
    jsonSkipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == c) {
        ++pos;
        return true;
    }
    return false;
}

} // namespace

std::string ProvenanceConfig::toJson() const {
    std::string out;
    out += "{\n  \"version\": 1,\n";

    // --- metadata block ---
    if (!metadata_.empty()) {
        out += "  \"metadata\": {\n";
        out += "    \"source_program_kind\": \"";
        jsonEscape(out, metadata_.source_program_kind);
        out += "\",\n";
        out += "    \"shader_stage\": \"";
        jsonEscape(out, metadata_.shader_stage);
        out += "\",\n";
        out += "    \"glsl_dialect\": \"";
        jsonEscape(out, metadata_.glsl_dialect);
        out += "\",\n";
        out += "    \"glsl_version\": ";
        out += std::to_string(metadata_.glsl_version);
        out += ",\n";
        out += "    \"runtime_effect_mode\": ";
        out += metadata_.runtime_effect_mode ? "true" : "false";
        out += ",\n";
        out += "    \"use_rt_flip\": ";
        out += metadata_.use_rt_flip ? "true" : "false";
        out += ",\n";
        out += "    \"use_fragcoord_workaround\": ";
        out += metadata_.use_fragcoord_workaround ? "true" : "false";
        out += "\n  },\n";
    }

    out += "  \"entries\": [\n";

    for (size_t i = 0; i < entries_.size(); ++i) {
        const ProvenanceEntry& e = entries_[i];
        out += "    {\n";
        out += "      \"kind\": \"";
        out += entryKindString(e.kind);
        out += "\",\n";

        out += "      \"helper_key\": \"";
        jsonEscape(out, e.helper_key);
        out += "\"";
        if (!e.variable_name.empty()) {
            out += ",\n      \"variable_name\": \"";
            jsonEscape(out, e.variable_name);
            out += "\",\n      \"sksl_type\": \"";
            jsonEscape(out, e.sksl_type);
            out += "\",\n      \"glsl_type\": \"";
            jsonEscape(out, e.glsl_type);
            out += "\"";
        }
        if (!e.callee_name.empty()) {
            out += ",\n      \"callee_name\": \"";
            jsonEscape(out, e.callee_name);
            out += "\",\n      \"sksl_method\": \"";
            jsonEscape(out, e.sksl_method);
            out += "\",\n      \"glsl_function\": \"";
            jsonEscape(out, e.glsl_function);
            out += "\"";
            if (!e.child_type.empty()) {
                out += ",\n      \"child_type\": \"";
                jsonEscape(out, e.child_type);
                out += "\"";
            }
        }
        if (!e.sksl_name.empty()) {
            out += ",\n      \"sksl_name\": \"";
            jsonEscape(out, e.sksl_name);
            out += "\",\n      \"glsl_name\": \"";
            jsonEscape(out, e.glsl_name);
            out += "\"";
        }
        if (!e.sksl_field.empty()) {
            out += ",\n      \"sksl_field\": \"";
            jsonEscape(out, e.sksl_field);
            out += "\",\n      \"glsl_constant\": \"";
            jsonEscape(out, e.glsl_constant);
            out += "\"";
        }
        out += "\n    }";
        if (i + 1 < entries_.size()) out += ",";
        out += "\n";
    }

    out += "  ]\n}\n";
    return out;
}

namespace {

// Parse a JSON boolean value (true/false) at pos. Returns the boolean value.
bool jsonParseBool(std::string_view json, size_t& pos) {
    jsonSkipWhitespace(json, pos);
    if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") {
        pos += 4;
        return true;
    }
    if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") {
        pos += 5;
        return false;
    }
    return false;
}

// Parse a JSON integer value at pos. Returns the integer.
int jsonParseInt(std::string_view json, size_t& pos) {
    jsonSkipWhitespace(json, pos);
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        ++pos;
    }
    return val;
}

// Skip a JSON value (string, number, bool, object, array) at pos.
void jsonSkipValue(std::string_view json, size_t& pos) {
    jsonSkipWhitespace(json, pos);
    if (pos >= json.size()) return;
    if (json[pos] == '"') {
        jsonParseString(json, pos);
    } else if (json[pos] == '{') {
        int depth = 1; ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '{') ++depth;
            else if (json[pos] == '}') --depth;
            ++pos;
        }
    } else if (json[pos] == '[') {
        int depth = 1; ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '[') ++depth;
            else if (json[pos] == ']') --depth;
            ++pos;
        }
    } else {
        while (pos < json.size() && json[pos] != ',' &&
               json[pos] != '}' && json[pos] != ']') ++pos;
    }
}

} // namespace

bool ProvenanceConfig::fromJson(std::string_view json) {
    entries_.clear();
    metadata_ = ProvenanceMetadata{};
    var_type_by_glsl_name_.clear();
    func_call_by_callee_func_.clear();
    builtin_by_glsl_name_.clear();
    caps_by_glsl_constant_.clear();
    helper_keys_.clear();

    size_t pos = 0;
    if (!jsonExpect(json, pos, '{')) return false;

    // Parse object keys
    while (pos < json.size()) {
        jsonSkipWhitespace(json, pos);
        if (pos >= json.size()) break;
        if (json[pos] == '}') { ++pos; break; }
        if (json[pos] == ',') { ++pos; continue; }

        std::string key = jsonParseString(json, pos);
        if (!jsonExpect(json, pos, ':')) return false;

        if (key == "version") {
            // skip number
            jsonSkipWhitespace(json, pos);
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') ++pos;
        } else if (key == "metadata") {
            if (!jsonExpect(json, pos, '{')) return false;
            while (pos < json.size()) {
                jsonSkipWhitespace(json, pos);
                if (pos >= json.size()) break;
                if (json[pos] == '}') { ++pos; break; }
                if (json[pos] == ',') { ++pos; continue; }

                std::string mkey = jsonParseString(json, pos);
                if (!jsonExpect(json, pos, ':')) return false;

                if (mkey == "source_program_kind") {
                    metadata_.source_program_kind = jsonParseString(json, pos);
                } else if (mkey == "shader_stage") {
                    metadata_.shader_stage = jsonParseString(json, pos);
                } else if (mkey == "glsl_dialect") {
                    metadata_.glsl_dialect = jsonParseString(json, pos);
                } else if (mkey == "glsl_version") {
                    metadata_.glsl_version = jsonParseInt(json, pos);
                } else if (mkey == "runtime_effect_mode") {
                    metadata_.runtime_effect_mode = jsonParseBool(json, pos);
                } else if (mkey == "use_rt_flip") {
                    metadata_.use_rt_flip = jsonParseBool(json, pos);
                } else if (mkey == "use_fragcoord_workaround") {
                    metadata_.use_fragcoord_workaround = jsonParseBool(json, pos);
                } else {
                    jsonSkipValue(json, pos);
                }
            }
        } else if (key == "entries") {
            if (!jsonExpect(json, pos, '[')) return false;
            while (pos < json.size()) {
                jsonSkipWhitespace(json, pos);
                if (pos >= json.size()) break;
                if (json[pos] == ']') { ++pos; break; }
                if (json[pos] == ',') { ++pos; continue; }

                if (!jsonExpect(json, pos, '{')) return false;
                ProvenanceEntry e;

                while (pos < json.size()) {
                    jsonSkipWhitespace(json, pos);
                    if (pos >= json.size()) break;
                    if (json[pos] == '}') { ++pos; break; }
                    if (json[pos] == ',') { ++pos; continue; }

                    std::string field = jsonParseString(json, pos);
                    if (!jsonExpect(json, pos, ':')) return false;

                    if (field == "kind") {
                        std::string k = jsonParseString(json, pos);
                        parseEntryKind(k, e.kind);
                    } else if (field == "helper_key") {
                        e.helper_key = jsonParseString(json, pos);
                    } else if (field == "variable_name") {
                        e.variable_name = jsonParseString(json, pos);
                    } else if (field == "sksl_type") {
                        e.sksl_type = jsonParseString(json, pos);
                    } else if (field == "glsl_type") {
                        e.glsl_type = jsonParseString(json, pos);
                    } else if (field == "callee_name") {
                        e.callee_name = jsonParseString(json, pos);
                    } else if (field == "sksl_method") {
                        e.sksl_method = jsonParseString(json, pos);
                    } else if (field == "glsl_function") {
                        e.glsl_function = jsonParseString(json, pos);
                    } else if (field == "child_type") {
                        e.child_type = jsonParseString(json, pos);
                    } else if (field == "sksl_name") {
                        e.sksl_name = jsonParseString(json, pos);
                    } else if (field == "glsl_name") {
                        e.glsl_name = jsonParseString(json, pos);
                    } else if (field == "sksl_field") {
                        e.sksl_field = jsonParseString(json, pos);
                    } else if (field == "glsl_constant") {
                        e.glsl_constant = jsonParseString(json, pos);
                    } else {
                        jsonSkipValue(json, pos);
                    }
                }

                // Index the entry
                size_t idx = entries_.size();
                entries_.push_back(std::move(e));
                const ProvenanceEntry& ref = entries_.back();

                if (!ref.helper_key.empty()) {
                    helper_keys_.insert(ref.helper_key);
                }

                switch (ref.kind) {
                    case ProvenanceEntry::Kind::kVariableType:
                        if (!ref.variable_name.empty())
                            var_type_by_glsl_name_[ref.variable_name] = idx;
                        break;
                    case ProvenanceEntry::Kind::kFunctionCall:
                        if (!ref.callee_name.empty() && !ref.glsl_function.empty()) {
                            std::string fkey(ref.callee_name);
                            fkey += "::";
                            fkey += ref.glsl_function;
                            func_call_by_callee_func_[fkey] = idx;
                        }
                        break;
                    case ProvenanceEntry::Kind::kBuiltinVariable:
                        if (!ref.glsl_name.empty())
                            builtin_by_glsl_name_[ref.glsl_name] = idx;
                        break;
                    case ProvenanceEntry::Kind::kCapabilityField:
                        if (!ref.glsl_constant.empty())
                            caps_by_glsl_constant_[ref.glsl_constant] = idx;
                        break;
                }
            }
        } else {
            jsonSkipValue(json, pos);
        }
    }

    return true;
}

// ============================================================================
// ProvenanceConfig — lookup
// ============================================================================

const ProvenanceEntry* ProvenanceConfig::findVariableType(std::string_view glsl_name) const {
    auto it = var_type_by_glsl_name_.find(std::string(glsl_name));
    if (it != var_type_by_glsl_name_.end() && it->second < entries_.size())
        return &entries_[it->second];
    return nullptr;
}

const ProvenanceEntry* ProvenanceConfig::findFunctionCall(std::string_view callee,
                                                          std::string_view glsl_function) const {
    std::string key(callee);
    key += "::";
    key += glsl_function;
    auto it = func_call_by_callee_func_.find(key);
    if (it != func_call_by_callee_func_.end() && it->second < entries_.size())
        return &entries_[it->second];
    return nullptr;
}

const ProvenanceEntry* ProvenanceConfig::findBuiltinVariable(std::string_view glsl_name) const {
    auto it = builtin_by_glsl_name_.find(std::string(glsl_name));
    if (it != builtin_by_glsl_name_.end() && it->second < entries_.size())
        return &entries_[it->second];
    return nullptr;
}

const ProvenanceEntry* ProvenanceConfig::findCapabilityField(std::string_view glsl_constant) const {
    auto it = caps_by_glsl_constant_.find(std::string(glsl_constant));
    if (it != caps_by_glsl_constant_.end() && it->second < entries_.size())
        return &entries_[it->second];
    return nullptr;
}

bool ProvenanceConfig::hasHelperKey(std::string_view helper_key) const {
    return helper_keys_.find(std::string(helper_key)) != helper_keys_.end();
}

// ============================================================================
// ReverseMapping
// ============================================================================

std::string ReverseMapping::mapIdentifier(std::string_view glsl_identifier,
                                          const BindingContext& context) const {
    // Check sideband config first
    if (config_) {
        if (const ProvenanceEntry* e = config_->findBuiltinVariable(glsl_identifier)) {
            if (!e->sksl_name.empty()) return e->sksl_name;
        }
    }

    // Fall back to binding registry
    const BuiltinBinding* builtin = MatchBuiltinByGlslPrimarySpelling(glsl_identifier);
    if (builtin && BuiltinAppliesToContext(*builtin, context)) {
        if (!builtin->sksl_spelling.empty()) return std::string(builtin->sksl_spelling);
    }

    builtin = MatchBuiltinBySurfaceForm(SurfaceLanguage::kGLSL,
                                        SurfaceFormKind::kIdentifier,
                                        glsl_identifier);
    if (builtin && BuiltinAppliesToContext(*builtin, context)) {
        if (!builtin->sksl_spelling.empty()) return std::string(builtin->sksl_spelling);
    }

    return {};
}

std::string ReverseMapping::mapFunctionCall(std::string_view glsl_function_name,
                                            const BindingContext& context) const {
    const IntrinsicBinding* intrinsic = MatchIntrinsicByGlslPrimarySpelling(glsl_function_name);
    if (intrinsic && IntrinsicAppliesToContext(*intrinsic, context)) {
        if (!intrinsic->sksl_spelling.empty()) return std::string(intrinsic->sksl_spelling);
    }

    intrinsic = MatchIntrinsicBySurfaceForm(SurfaceLanguage::kGLSL,
                                            SurfaceFormKind::kFunctionCall,
                                            glsl_function_name);
    if (intrinsic && IntrinsicAppliesToContext(*intrinsic, context)) {
        if (!intrinsic->sksl_spelling.empty()) return std::string(intrinsic->sksl_spelling);
    }

    return {};
}

bool ReverseMapping::shouldRecoverChildEval(std::string_view callee_name) const {
    if (!config_) return false;
    const ProvenanceEntry* e = config_->findFunctionCall(callee_name, "texture");
    return e != nullptr && e->helper_key == "runtime_effect_child_eval";
}

}  // namespace sksl_glsl_binding
