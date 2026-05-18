//
// Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
// Copyright (C) 2024
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

//
// GLSL to SkSL Code Generator
// Converts glslang AST to SkSL (Skia Shading Language) output
//

#include "../Include/Common.h"
#include "../Include/intermediate.h"
#include "../Include/InfoSink.h"
#include "../MachineIndependent/localintermediate.h"

#include "binding_registry/builtin_bindings.h"
#include "binding_registry/intrinsic_bindings.h"
#include "binding_registry/feature_bindings.h"
#include "binding_registry/provenance.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdio>

namespace glslang {

namespace binding = sksl_glsl_binding;

static std::string formatFloat(double val) {
    if (std::isinf(val)) return val > 0 ? "(1.0/0.0)" : "(-1.0/0.0)";
    if (std::isnan(val)) return "(0.0/0.0)";
    char buf[64];
    snprintf(buf, sizeof(buf), "%.9g", val);
    std::string s(buf);
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
        s += ".0";
    }
    return s;
}

static int getOperatorPrecedence(TOperator op) {
    switch (op) {
        case EOpAssign: case EOpAddAssign: case EOpSubAssign:
        case EOpMulAssign: case EOpDivAssign: case EOpModAssign:
        case EOpAndAssign: case EOpInclusiveOrAssign: case EOpExclusiveOrAssign:
        case EOpLeftShiftAssign: case EOpRightShiftAssign:
        case EOpVectorTimesMatrixAssign: case EOpVectorTimesScalarAssign:
        case EOpMatrixTimesScalarAssign: case EOpMatrixTimesMatrixAssign:
            return 1;
        case EOpComma: return 0;
        case EOpLogicalOr: return 2;
        case EOpLogicalXor: return 3;
        case EOpLogicalAnd: return 4;
        case EOpInclusiveOr: return 5;
        case EOpExclusiveOr: return 6;
        case EOpAnd: return 7;
        case EOpEqual: case EOpNotEqual:
        case EOpVectorEqual: case EOpVectorNotEqual:
            return 8;
        case EOpLessThan: case EOpGreaterThan:
        case EOpLessThanEqual: case EOpGreaterThanEqual:
            return 9;
        case EOpLeftShift: case EOpRightShift: return 10;
        case EOpAdd: case EOpSub: return 11;
        case EOpMul: case EOpDiv: case EOpMod:
        case EOpVectorTimesScalar: case EOpVectorTimesMatrix:
        case EOpMatrixTimesVector: case EOpMatrixTimesScalar:
        case EOpMatrixTimesMatrix:
            return 12;
        default: return 13;
    }
}

//
// GLSL to SkSL Code Generator class
// Traverses the glslang AST and outputs SkSL code
//
class GLSLSkSLCodeGenerator : public TIntermTraverser {
public:
    GLSLSkSLCodeGenerator(TInfoSink& infoSink, EShLanguage stage)
        : TIntermTraverser(true, false, false, false)
        , infoSink(infoSink)
        , stage(stage)
        , indentDepth(0)
        , atLineStart(true)
    {}

    virtual ~GLSLSkSLCodeGenerator() = default;

    // Main entry point for code generation
    bool generateCode(TIntermNode* root, std::string_view glslSource = {});

    // Output utilities
    void write(const char* s);
    void write(const std::string& s);
    void writeLine(const char* s);
    void writeLine(const std::string& s);
    void finishLine();
    void writeIndent();

    // Type conversion from GLSL to SkSL
    std::string getTypeName(const TType& type);
    std::string getBasicTypeName(TBasicType basicType);
    std::string getPrecisionQualifier(TPrecisionQualifier precision);

    // Operator conversion
    std::string getOperatorName(TOperator op);
    std::string getBinaryOperatorSymbol(TOperator op);
    std::string getUnaryOperatorSymbol(TOperator op);

    // Qualifier conversion
    std::string getQualifierString(const TQualifier& qualifier, bool isGlobal);
    std::string getLayoutString(const TQualifier& qualifier);

    // Expression writers
    void writeExpression(TIntermTyped* expr);
    void writeBinaryExpression(TIntermBinary* node);
    void writeUnaryExpression(TIntermUnary* node);
    void writeAggregateExpression(TIntermAggregate* node);
    void writeConstantUnion(TIntermConstantUnion* node);
    void writeSymbol(TIntermSymbol* node);
    void writeSelection(TIntermSelection* node);
    void writeSwizzle(TIntermBinary* node);
    void writeMethod(TIntermMethod* node);

    // Statement writers
    void writeStatement(TIntermNode* node);
    void writeBlock(TIntermAggregate* node);
    void writeLoop(TIntermLoop* node);
    void writeBranch(TIntermBranch* node);
    void writeSwitch(TIntermSwitch* node);
    void writeVariableDecl(TIntermVariableDecl* node);

    // Program structure writers
    void writeFunction(TIntermAggregate* node);
    void writeFunctionParameters(TIntermAggregate* node);
    void writeGlobalDeclaration(TIntermAggregate* node);

    // Built-in function mappings
    std::string getBuiltInFunctionName(TOperator op);
    bool isBuiltInFunction(TOperator op);

    // Texture/Sampler handling
    std::string getSamplerType(const TType& type);
    std::string getTextureFunctionName(TOperator op);

    // Traversal callbacks
    virtual void visitSymbol(TIntermSymbol* symbol) override;
    virtual void visitConstantUnion(TIntermConstantUnion* constantUnion) override;
    virtual bool visitBinary(TVisit visit, TIntermBinary* binary) override;
    virtual bool visitUnary(TVisit visit, TIntermUnary* unary) override;
    virtual bool visitSelection(TVisit visit, TIntermSelection* selection) override;
    virtual bool visitAggregate(TVisit visit, TIntermAggregate* aggregate) override;
    virtual bool visitLoop(TVisit visit, TIntermLoop* loop) override;
    virtual bool visitBranch(TVisit visit, TIntermBranch* branch) override;
    virtual bool visitSwitch(TVisit visit, TIntermSwitch* switchNode) override;
    virtual bool visitVariableDecl(TVisit visit, TIntermVariableDecl* decl) override;

protected:
    TInfoSink& infoSink;
    EShLanguage stage;
    int indentDepth;
    bool atLineStart;

    // Track current function for context
    std::string currentFunctionName;
    std::string fragColorParamName;
    bool inFunctionBody = false;

    // Track declared variables to avoid re-declaring on reassignment
    std::unordered_set<long long> declaredVariables;
    // Name-based tracking for synthesized locals / entry-point parameters
    // that don't have real glslang internal IDs
    std::unordered_set<std::string> declaredLocalNames;

    // Names declared at global scope (uniforms, globals). The pre-scan
    // must not redeclare these inside function bodies.
    std::unordered_set<std::string> globalNames;

    // Name of the uniform that carries the rendering resolution (e.g. iResolution).
    // Detected during global-declaration processing and used to adjust coordinates
    // when converting texture() → .eval(), since eval() expects pixel coordinates
    // while texture() expects normalized UV.
    std::string resolutionVarName;

    // Names of symbols referenced inside any function body. Used to omit
    // declarations of uniforms that are never read (avoids resource waste).
    std::unordered_set<std::string> usedSymbolNames;
    bool usedSymbolNamesComputed = false;
    void collectUsedSymbolNames(TIntermNode* root);

    // For-loop variables that were originally typed `float` but have been
    // rebound to `int` because the loop is integer-valued. Uses of these
    // symbols inside the loop body must be wrapped with `float(...)`.
    std::unordered_set<long long> floatLoopVarsRetypedToInt;
    // While >0, suppress the float() cast and emit float constants as ints.
    int suppressFloatLoopVarCast = 0;

    // SkSL fragment entry transform: when we encounter `mainImage`, emit it
    // as `half4 main(float2 fragCoord)` with an injected local `float4
    // fragColor;` and a trailing `return half4(fragColor);`.
    bool inMainImageBody = false;
    std::string mainImageFragColorName;
    std::string mainImageFragCoordName;

    // Binding registry integration
    binding::BindingContext getBindingContext() const;
    const binding::BuiltinBinding* getBuiltinBindingByGlslName(std::string_view glslName) const;
    const binding::IntrinsicBinding* getIntrinsicBindingByGlslName(std::string_view glslName) const;

public:
    // Sideband provenance config for reverse translation (input)
    void setProvenanceConfig(const binding::ProvenanceConfig* config) {
        provenanceConfig = config;
    }
    // Reverse provenance config recorded during GLSL→SKSL translation (output)
    const binding::ProvenanceConfig& getReverseProvenance() const {
        return reverseProvenance;
    }

private:
    // Walk an expression tree and record any gl_* builtin references for provenance,
    // without emitting output. Used when a statement is suppressed but we still
    // need to track which GLSL builtins were involved.
    void recordGlslBuiltins(TIntermNode* node) {
        if (!node) return;
        if (auto* sym = node->getAsSymbolNode()) {
            std::string_view name(sym->getName().c_str());
            if (name.substr(0, 3) == "gl_") {
                const binding::BuiltinBinding* builtin = getBuiltinBindingByGlslName(name);
                if (builtin && !builtin->sksl_spelling.empty() && !builtin->helper_key.empty()) {
                    reverseProvenance.recordBuiltinVariable(
                        builtin->sksl_spelling, builtin->glsl_primary_spelling,
                        builtin->helper_key);
                }
            }
            return;
        }
        if (auto* bin = node->getAsBinaryNode()) {
            recordGlslBuiltins(bin->getLeft());
            recordGlslBuiltins(bin->getRight());
            return;
        }
        if (auto* un = node->getAsUnaryNode()) {
            recordGlslBuiltins(un->getOperand());
            return;
        }
        if (auto* agg = node->getAsAggregate()) {
            for (auto* child : agg->getSequence()) {
                recordGlslBuiltins(child);
            }
            return;
        }
        if (auto* sel = node->getAsSelectionNode()) {
            recordGlslBuiltins(sel->getCondition());
            recordGlslBuiltins(sel->getTrueBlock());
            recordGlslBuiltins(sel->getFalseBlock());
            return;
        }
        if (auto* sw = node->getAsSwitchNode()) {
            recordGlslBuiltins(sw->getCondition());
            recordGlslBuiltins(sw->getBody());
            return;
        }
    }

private:
    const binding::ProvenanceConfig* provenanceConfig = nullptr;
    binding::ProvenanceConfig reverseProvenance;

};

// Forward declaration of utility function used before its definition
static std::string cleanFunctionName(const TString& mangledName);

namespace {
// Helper traverser: collect every symbol name referenced anywhere under the
// given root. We use this on the root AST (which includes function bodies)
// so we can drop uniform declarations that are never read.
class UsedSymbolCollector : public TIntermTraverser {
public:
    explicit UsedSymbolCollector(std::unordered_set<std::string>& out)
        : TIntermTraverser(true, true, true, false), out(out) {}
    void visitSymbol(TIntermSymbol* s) override {
        if (s) out.insert(s->getName().c_str());
    }
private:
    std::unordered_set<std::string>& out;
};
} // namespace

void GLSLSkSLCodeGenerator::collectUsedSymbolNames(TIntermNode* root) {
    if (usedSymbolNamesComputed || !root) return;
    usedSymbolNamesComputed = true;
    UsedSymbolCollector collector(usedSymbolNames);
    // Only walk function bodies (skip the linker-objects list, which would
    // mark every declared global as "used"). We approximate this by walking
    // all EOpFunction children of the root aggregate.
    if (auto* agg = root->getAsAggregate()) {
        for (auto* child : agg->getSequence()) {
            if (auto* c = child ? child->getAsAggregate() : nullptr) {
                if (c->getOp() == EOpFunction) c->traverse(&collector);
            }
        }
    } else {
        root->traverse(&collector);
    }
}

// ============================================================================
// Output utilities
// ============================================================================

void GLSLSkSLCodeGenerator::write(const char* s) {
    infoSink.debug << s;
    atLineStart = false;
}

void GLSLSkSLCodeGenerator::write(const std::string& s) {
    infoSink.debug << s.c_str();
    atLineStart = false;
}

void GLSLSkSLCodeGenerator::writeLine(const char* s) {
    write(s);
    infoSink.debug << "\n";
    atLineStart = true;
}

void GLSLSkSLCodeGenerator::writeLine(const std::string& s) {
    write(s);
    infoSink.debug << "\n";
    atLineStart = true;
}

void GLSLSkSLCodeGenerator::finishLine() {
    if (!atLineStart) {
        infoSink.debug << "\n";
        atLineStart = true;
    }
}

void GLSLSkSLCodeGenerator::writeIndent() {
    if (atLineStart) {
        for (int i = 0; i < indentDepth; ++i) {
            infoSink.debug << "    ";
        }
        atLineStart = false;
    }
}

// ============================================================================
// Type conversion: GLSL -> SkSL
// ============================================================================

std::string GLSLSkSLCodeGenerator::getBasicTypeName(TBasicType basicType) {
    switch (basicType) {
        case EbtVoid:
            return "void";
        case EbtFloat:
            return "float";
        case EbtDouble:
            // TODO: SkSL doesn't support double - need workaround
            return "double";  // May need to emit as float with comment
        case EbtFloat16:
            return "half";
        case EbtBFloat16:
            return "bfloat16";  // TODO: Check SkSL support
        case EbtFloatE5M2:
        case EbtFloatE4M3:
            // TODO: SkSL may not support these float formats
            return "float";  // Fallback
        case EbtInt:
            return "int";
        case EbtUint:
            return "uint";
        case EbtInt8:
            return "byte";  // TODO: Check SkSL naming
        case EbtUint8:
            return "ubyte";
        case EbtInt16:
            return "short";
        case EbtUint16:
            return "ushort";
        case EbtInt64:
            // TODO: SkSL may not support int64 - need workaround
            return "long";
        case EbtUint64:
            // TODO: SkSL may not support uint64 - need workaround
            return "ulong";
        case EbtBool:
            return "bool";
        case EbtAtomicUint:
            // TODO: Atomic handling in SkSL
            return "atomic_uint";
        case EbtSampler:
            return "sampler";  // Will be refined in getSamplerType
        case EbtStruct:
            return "";  // Struct name handled separately
        case EbtBlock:
            return "";  // Block name handled separately
        case EbtCoopmat:
        case EbtTensorLayoutNV:
        case EbtTensorViewNV:
            // TODO: Cooperative matrix types - may need special handling
            return "cooperative_matrix";  // Placeholder
        case EbtAccStruct:
            return "accelerationStructure";  // TODO: Ray tracing support
        case EbtRayQuery:
            return "rayQuery";  // TODO: Ray query support
        case EbtHitObjectNV:
            return "hitObjectNV";  // TODO: Hit object support
        case EbtSpirvType:
            // TODO: SPIR-V type handling
            return "spirv_type";  // Placeholder
        default:
            return "unknown_type";  // TODO: Handle other types
    }
}

std::string GLSLSkSLCodeGenerator::getTypeName(const TType& type) {
    std::string result;

    // Handle basic type
    TBasicType basicType = type.getBasicType();

    // Handle vectors
    if (type.isVector()) {
        int vecSize = type.getVectorSize();
        std::string baseType;

        switch (basicType) {
            case EbtFloat:
                baseType = "float";
                break;
            case EbtFloat16:
                baseType = "half";
                break;
            case EbtDouble:
                baseType = "double";  // TODO: Workaround needed
                break;
            case EbtInt:
                baseType = "int";
                break;
            case EbtUint:
                baseType = "uint";
                break;
            case EbtBool:
                baseType = "bool";
                break;
            case EbtInt8:
                baseType = "byte";
                break;
            case EbtUint8:
                baseType = "ubyte";
                break;
            case EbtInt16:
                baseType = "short";
                break;
            case EbtUint16:
                baseType = "ushort";
                break;
            case EbtInt64:
                baseType = "long";
                break;
            case EbtUint64:
                baseType = "ulong";
                break;
            default:
                baseType = getBasicTypeName(basicType);
                break;
        }

        result = baseType + std::to_string(vecSize);
    }
    // Handle matrices
    else if (type.isMatrix()) {
        int cols = type.getMatrixCols();
        int rows = type.getMatrixRows();
        std::string baseType;

        switch (basicType) {
            case EbtFloat:
                baseType = "float";
                break;
            case EbtFloat16:
                baseType = "half";
                break;
            case EbtDouble:
                baseType = "double";
                break;
            default:
                baseType = "float";
                break;
        }

        // SkSL uses floatNxM notation (columns x rows)
        result = baseType + std::to_string(cols) + "x" + std::to_string(rows);
    }
    // Handle arrays
    else if (type.isArray()) {
        result = getBasicTypeName(type.getBasicType());
        // Array notation handled separately in declaration
    }
    // Handle structs
    else if (basicType == EbtStruct) {
        const TString& structName = type.getTypeName();
        if (!structName.empty()) {
            result = structName.c_str();
        } else {
            result = "struct";
        }
    }
    // Handle blocks
    else if (basicType == EbtBlock) {
        const TString& blockName = type.getTypeName();
        if (!blockName.empty()) {
            result = blockName.c_str();
        } else {
            result = "block";
        }
    }
    // Handle samplers
    else if (basicType == EbtSampler) {
        result = getSamplerType(type);
    }
    // Basic scalar types
    else {
        result = getBasicTypeName(basicType);
    }

    return result;
}

std::string GLSLSkSLCodeGenerator::getSamplerType(const TType& type) {
    const TSampler& sampler = type.getSampler();

    // Pure separate sampler — keep as "sampler"
    if (sampler.isPureSampler()) {
        return "sampler";
    }

    // Subpass inputs — keep GLSL naming
    if (sampler.isSubpass()) {
        if (sampler.isMultiSample())
            return "subpassInputMS";
        return "subpassInput";
    }

    // Write-only image types (storage images) — keep as "image2D" etc.
    if (sampler.isImageClass()) {
        std::string result = "image";
        switch (sampler.dim) {
            case Esd1D:  result += "1D";  break;
            case Esd2D:  result += "2D";  break;
            case Esd3D:  result += "3D";  break;
            case EsdCube: result += "Cube"; break;
            case EsdRect: result += "2DRect"; break;
            case EsdBuffer: result += "Buffer"; break;
            default: break;
        }
        if (sampler.isArrayed()) result += "Array";
        if (sampler.isMultiSample()) result += "MS";
        return result;
    }

    // Read-only or writable texture types (non-combined, non-image)
    if (!sampler.isCombined()) {
        // Separate texture (not combined with a sampler object)
        std::string result = "texture";
        switch (sampler.dim) {
            case Esd1D:  result += "1D";  break;
            case Esd2D:  result += "2D";  break;
            case Esd3D:  result += "3D";  break;
            case EsdCube: result += "Cube"; break;
            case EsdRect: result += "2DRect"; break;
            case EsdBuffer: result += "Buffer"; break;
            default: break;
        }
        if (sampler.isArrayed()) result += "Array";
        if (sampler.isMultiSample()) result += "MS";
        if (sampler.isShadow()) result += "Shadow";
        return result;
    }

    // Combined sampler (e.g. sampler2D) type mapping.
    // In SkSL runtime effects: → "shader" (child shader).
    // In non-runtime-effect modes: preserve as the original sampler type (e.g. "sampler2D").
    // The runtime_effect_mode flag is set from provenance metadata when available.
    if (sampler.isExternal())
        return "shader";  // samplerExternalOES → shader (always, this is a runtime-effect construct)

    if (getBindingContext().runtime_effect_mode)
        return "shader";

    // Not a runtime effect — preserve GLSL sampler type
    {
        std::string result = "sampler";
        switch (sampler.dim) {
            case Esd1D:  result += "1D";  break;
            case Esd2D:  result += "2D";  break;
            case Esd3D:  result += "3D";  break;
            case EsdCube: result += "Cube"; break;
            case EsdRect: result += "2DRect"; break;
            case EsdBuffer: result += "Buffer"; break;
            default:     result += "2D";  break;
        }
        if (sampler.isArrayed()) result += "Array";
        if (sampler.isMultiSample()) result += "MS";
        if (sampler.isShadow()) result += "Shadow";
        return result;
    }
}

std::string GLSLSkSLCodeGenerator::getPrecisionQualifier(TPrecisionQualifier precision) {
    switch (precision) {
        case EpqHigh:
            return "highp";
        case EpqMedium:
            return "mediump";
        case EpqLow:
            return "lowp";
        case EpqNone:
        default:
            return "";
    }
}

// ============================================================================
// Operator conversion
// ============================================================================

std::string GLSLSkSLCodeGenerator::getBinaryOperatorSymbol(TOperator op) {
    switch (op) {
        // Arithmetic
        case EOpAdd:
            return " + ";
        case EOpSub:
            return " - ";
        case EOpMul:
            return " * ";
        case EOpDiv:
            return " / ";
        case EOpMod:
            return " % ";

        // Bitwise
        case EOpRightShift:
            return " >> ";
        case EOpLeftShift:
            return " << ";
        case EOpAnd:
            return " & ";
        case EOpInclusiveOr:
            return " | ";
        case EOpExclusiveOr:
            return " ^ ";

        // Logical
        case EOpLogicalOr:
            return " || ";
        case EOpLogicalXor:
            // SkSL does not support ^^ operator; this must be expanded to (a && !b) || (!a && b)
            return " /*^^*/ ";
        case EOpLogicalAnd:
            return " && ";

        // Comparison
        case EOpEqual:
        case EOpVectorEqual:
            return " == ";
        case EOpNotEqual:
        case EOpVectorNotEqual:
            return " != ";
        case EOpLessThan:
            return " < ";
        case EOpGreaterThan:
            return " > ";
        case EOpLessThanEqual:
            return " <= ";
        case EOpGreaterThanEqual:
            return " >= ";

        // Assignment
        case EOpAssign:
            return " = ";
        case EOpAddAssign:
            return " += ";
        case EOpSubAssign:
            return " -= ";
        case EOpMulAssign:
            return " *= ";
        case EOpDivAssign:
            return " /= ";
        case EOpModAssign:
            return " %= ";
        case EOpAndAssign:
            return " &= ";
        case EOpInclusiveOrAssign:
            return " |= ";
        case EOpExclusiveOrAssign:
            return " ^= ";
        case EOpLeftShiftAssign:
            return " <<= ";
        case EOpRightShiftAssign:
            return " >>= ";

        // Matrix/vector operations
        case EOpVectorTimesScalar:
            return " * ";
        case EOpVectorTimesMatrix:
            return " * ";
        case EOpMatrixTimesVector:
            return " * ";
        case EOpMatrixTimesScalar:
            return " * ";
        case EOpMatrixTimesMatrix:
            return " * ";
        case EOpVectorTimesMatrixAssign:
            return " *= ";
        case EOpVectorTimesScalarAssign:
            return " *= ";
        case EOpMatrixTimesScalarAssign:
            return " *= ";
        case EOpMatrixTimesMatrixAssign:
            return " *= ";

        // Comma
        case EOpComma:
            return ", ";

        default:
            return " /* unknown operator */ ";
    }
}

std::string GLSLSkSLCodeGenerator::getUnaryOperatorSymbol(TOperator op) {
    switch (op) {
        case EOpNegative:
            return "-";
        case EOpLogicalNot:
        case EOpVectorLogicalNot:
            return "!";
        case EOpBitwiseNot:
            return "~";
        case EOpPostIncrement:
            return "++";
        case EOpPostDecrement:
            return "--";
        case EOpPreIncrement:
            return "++";
        case EOpPreDecrement:
            return "--";
        default:
            return "";
    }
}

std::string GLSLSkSLCodeGenerator::getOperatorName(TOperator op) {
    // Built-in function names
    switch (op) {
        // Trigonometric
        case EOpRadians:
            return "radians";
        case EOpDegrees:
            return "degrees";
        case EOpSin:
            return "sin";
        case EOpCos:
            return "cos";
        case EOpTan:
            return "tan";
        case EOpAsin:
            return "asin";
        case EOpAcos:
            return "acos";
        case EOpAtan:
            return "atan";
        case EOpSinh:
            return "sinh";
        case EOpCosh:
            return "cosh";
        case EOpTanh:
            return "tanh";
        case EOpAsinh:
            return "asinh";
        case EOpAcosh:
            return "acosh";
        case EOpAtanh:
            return "atanh";

        // Exponential
        case EOpPow:
            return "pow";
        case EOpExp:
            return "exp";
        case EOpLog:
            return "log";
        case EOpExp2:
            return "exp2";
        case EOpLog2:
            return "log2";
        case EOpSqrt:
            return "sqrt";
        case EOpInverseSqrt:
            return "inversesqrt";

        // Common
        case EOpAbs:
            return "abs";
        case EOpSign:
            return "sign";
        case EOpFloor:
            return "floor";
        case EOpTrunc:
            return "trunc";
        case EOpRound:
            return "round";
        case EOpRoundEven:
            return "roundEven";  // TODO: Check SkSL support
        case EOpCeil:
            return "ceil";
        case EOpFract:
            return "fract";
        case EOpMod:
            return "mod";
        case EOpModf:
            return "modf";
        case EOpMin:
            return "min";
        case EOpMax:
            return "max";
        case EOpClamp:
            return "clamp";
        case EOpMix:
            return "mix";
        case EOpStep:
            return "step";
        case EOpSmoothStep:
            return "smoothstep";

        // Float/Int bit operations
        case EOpFloatBitsToInt:
            return "floatBitsToInt";
        case EOpFloatBitsToUint:
            return "floatBitsToUint";
        case EOpIntBitsToFloat:
            return "intBitsToFloat";
        case EOpUintBitsToFloat:
            return "uintBitsToFloat";

        // Geometric
        case EOpLength:
            return "length";
        case EOpDistance:
            return "distance";
        case EOpDot:
            return "dot";
        case EOpCross:
            return "cross";
        case EOpNormalize:
            return "normalize";
        case EOpFaceForward:
            return "faceforward";
        case EOpReflect:
            return "reflect";
        case EOpRefract:
            return "refract";

        // Matrix
        case EOpOuterProduct:
            return "outerProduct";
        case EOpDeterminant:
            return "determinant";
        case EOpMatrixInverse:
            return "inverse";
        case EOpTranspose:
            return "transpose";

        // Vector relational
        case EOpLessThan:
            return "lessThan";
        case EOpGreaterThan:
            return "greaterThan";
        case EOpLessThanEqual:
            return "lessThanEqual";
        case EOpGreaterThanEqual:
            return "greaterThanEqual";
        case EOpVectorEqual:
            return "equal";
        case EOpVectorNotEqual:
            return "notEqual";
        case EOpAny:
            return "any";
        case EOpAll:
            return "all";

        // Integer/Bit
        case EOpBitfieldExtract:
            return "bitfieldExtract";
        case EOpBitfieldInsert:
            return "bitfieldInsert";
        case EOpBitFieldReverse:
            return "bitfieldReverse";
        case EOpBitCount:
            return "bitCount";
        case EOpFindLSB:
            return "findLSB";
        case EOpFindMSB:
            return "findMSB";

        // Texture
        case EOpTexture:
        case EOpTextureProj:
        case EOpTextureLod:
        case EOpTextureOffset:
        case EOpTextureFetch:
        case EOpTextureFetchOffset:
        case EOpTextureGrad:
        case EOpTextureGradOffset:
        case EOpTextureGather:
        case EOpTextureGatherOffset:
            return getTextureFunctionName(op);

        // Derivatives
        case EOpDPdx:
            return "dFdx";
        case EOpDPdy:
            return "dFdy";
        case EOpFwidth:
            return "fwidth";
        case EOpDPdxFine:
            return "dFdxFine";
        case EOpDPdyFine:
            return "dFdyFine";
        case EOpFwidthFine:
            return "fwidthFine";
        case EOpDPdxCoarse:
            return "dFdxCoarse";
        case EOpDPdyCoarse:
            return "dFdyCoarse";
        case EOpFwidthCoarse:
            return "fwidthCoarse";

        // Interpolation
        case EOpInterpolateAtCentroid:
            return "interpolateAtCentroid";
        case EOpInterpolateAtSample:
            return "interpolateAtSample";
        case EOpInterpolateAtOffset:
            return "interpolateAtOffset";

        // Atomic
        case EOpAtomicAdd:
            return "atomicAdd";
        case EOpAtomicMin:
            return "atomicMin";
        case EOpAtomicMax:
            return "atomicMax";
        case EOpAtomicAnd:
            return "atomicAnd";
        case EOpAtomicOr:
            return "atomicOr";
        case EOpAtomicXor:
            return "atomicXor";
        case EOpAtomicExchange:
            return "atomicExchange";
        case EOpAtomicCompSwap:
            return "atomicCompSwap";

        // Image
        case EOpImageLoad:
            return "imageLoad";
        case EOpImageStore:
            return "imageStore";

        // Misc
        case EOpArrayLength:
            return "arrayLength";

        default:
            return "/* unknown builtin */";
    }
}

std::string GLSLSkSLCodeGenerator::getTextureFunctionName(TOperator op) {
    // TODO: SkSL texture functions may differ from GLSL
    // In SkSL, texture sampling is often done via sample() method
    switch (op) {
        case EOpTexture:
            return "texture";
        case EOpTextureProj:
            return "textureProj";
        case EOpTextureLod:
            return "textureLod";
        case EOpTextureOffset:
            return "textureOffset";
        case EOpTextureFetch:
            return "texelFetch";
        case EOpTextureFetchOffset:
            return "texelFetchOffset";
        case EOpTextureGrad:
            return "textureGrad";
        case EOpTextureGradOffset:
            return "textureGradOffset";
        case EOpTextureGather:
            return "textureGather";
        case EOpTextureGatherOffset:
            return "textureGatherOffset";
        case EOpTextureQuerySize:
            return "textureSize";
        case EOpTextureQueryLod:
            return "textureQueryLod";
        case EOpTextureQueryLevels:
            return "textureQueryLevels";
        case EOpTextureQuerySamples:
            return "textureSamples";
        default:
            return "texture";  // Fallback
    }
}

bool GLSLSkSLCodeGenerator::isBuiltInFunction(TOperator op) {
    switch (op) {
        // All the built-in function operators
        case EOpRadians:
        case EOpDegrees:
        case EOpSin:
        case EOpCos:
        case EOpTan:
        case EOpAsin:
        case EOpAcos:
        case EOpAtan:
        case EOpSinh:
        case EOpCosh:
        case EOpTanh:
        case EOpAsinh:
        case EOpAcosh:
        case EOpAtanh:
        case EOpPow:
        case EOpExp:
        case EOpLog:
        case EOpExp2:
        case EOpLog2:
        case EOpSqrt:
        case EOpInverseSqrt:
        case EOpAbs:
        case EOpSign:
        case EOpFloor:
        case EOpTrunc:
        case EOpRound:
        case EOpRoundEven:
        case EOpCeil:
        case EOpFract:
        case EOpMod:
        case EOpModf:
        case EOpMin:
        case EOpMax:
        case EOpClamp:
        case EOpMix:
        case EOpStep:
        case EOpSmoothStep:
        case EOpLength:
        case EOpDistance:
        case EOpDot:
        case EOpCross:
        case EOpNormalize:
        case EOpFaceForward:
        case EOpReflect:
        case EOpRefract:
        case EOpOuterProduct:
        case EOpDeterminant:
        case EOpMatrixInverse:
        case EOpTranspose:
        case EOpDPdx:
        case EOpDPdy:
        case EOpFwidth:
        case EOpAny:
        case EOpAll:
        case EOpFloatBitsToInt:
        case EOpFloatBitsToUint:
        case EOpIntBitsToFloat:
        case EOpUintBitsToFloat:
        case EOpPackSnorm2x16:
        case EOpUnpackSnorm2x16:
        case EOpPackUnorm2x16:
        case EOpUnpackUnorm2x16:
        case EOpPackHalf2x16:
        case EOpUnpackHalf2x16:
        case EOpBitCount:
        case EOpFindLSB:
        case EOpFindMSB:
        case EOpBitfieldExtract:
        case EOpBitfieldInsert:
        case EOpBitFieldReverse:
            return true;

        // Texture operations
        case EOpTexture:
        case EOpTextureProj:
        case EOpTextureLod:
        case EOpTextureOffset:
        case EOpTextureFetch:
        case EOpTextureFetchOffset:
        case EOpTextureGrad:
        case EOpTextureGradOffset:
        case EOpTextureGather:
        case EOpTextureGatherOffset:
        case EOpTextureQuerySize:
        case EOpTextureQueryLod:
        case EOpTextureQueryLevels:
        case EOpTextureQuerySamples:
            return true;

        // Atomic operations
        case EOpAtomicAdd:
        case EOpAtomicMin:
        case EOpAtomicMax:
        case EOpAtomicAnd:
        case EOpAtomicOr:
        case EOpAtomicXor:
        case EOpAtomicExchange:
        case EOpAtomicCompSwap:
            return true;

        default:
            return false;
    }
}

// ============================================================================
// Qualifier conversion
// ============================================================================

std::string GLSLSkSLCodeGenerator::getQualifierString(const TQualifier& qualifier, bool isGlobal) {
    std::string result;

    // Storage qualifiers
    switch (qualifier.storage) {
        case EvqConst:
            result += "const ";
            break;
        case EvqVaryingIn:
            if (isGlobal) {
                result += "in ";
            }
            break;
        case EvqVaryingOut:
            if (isGlobal) {
                result += "out ";
            }
            break;
        case EvqUniform:
            result += "uniform ";
            break;
        default:
            break;
    }

    // SkSL does not use:
    //   - buffer / shared storage qualifiers
    //   - precision qualifiers (highp/mediump/lowp) — use half/float types instead
    //   - interpolation qualifiers (flat/smooth/noperspective/centroid/sample)
    //   - memory qualifiers (readonly/writeonly/coherent/volatile/restrict)
    // All of these are silently stripped.

    return result;
}

std::string GLSLSkSLCodeGenerator::getLayoutString(const TQualifier&) {
    // SkSL does not support layout() qualifiers — always strip them
    return "";
}

// ============================================================================
// Binding Registry Integration
// ============================================================================

binding::BindingContext GLSLSkSLCodeGenerator::getBindingContext() const {
    binding::BindingContext context;

    // Start with defaults from the GLSL source stage
    switch (stage) {
        case EShLangVertex:
            context.stage = binding::ShaderStage::kVertex;
            break;
        case EShLangFragment:
            context.stage = binding::ShaderStage::kFragment;
            break;
        case EShLangCompute:
            context.stage = binding::ShaderStage::kCompute;
            break;
        default:
            context.stage = binding::ShaderStage::kFragment;
            break;
    }

    // If input provenance has metadata, it carries the authoritative context from the
    // forward translation. Override our defaults with the provenance metadata.
    if (provenanceConfig && provenanceConfig->hasMetadata()) {
        const auto& meta = provenanceConfig->metadata();

        // Override shader stage from provenance metadata
        if (meta.shader_stage == "kVertex") {
            context.stage = binding::ShaderStage::kVertex;
        } else if (meta.shader_stage == "kFragment") {
            context.stage = binding::ShaderStage::kFragment;
        } else if (meta.shader_stage == "kCompute") {
            context.stage = binding::ShaderStage::kCompute;
        }

        // Override GLSL dialect from provenance metadata
        if (meta.glsl_dialect == "kGLES") {
            context.glsl_dialect = binding::GlslDialect::kGLES;
        } else if (meta.glsl_dialect == "kVulkanGLSL") {
            context.glsl_dialect = binding::GlslDialect::kVulkanGLSL;
        } else {
            context.glsl_dialect = binding::GlslDialect::kOpenGLCore;
        }

        context.glsl_version = meta.glsl_version;
        context.runtime_effect_mode = meta.runtime_effect_mode;
        context.use_rt_flip = meta.use_rt_flip;
        context.use_fragcoord_workaround = meta.use_fragcoord_workaround;
    } else {
        // No provenance metadata — use universal defaults that produce
        // SkSL compatible with Skia RuntimeEffect. In particular,
        // runtime_effect_mode=true ensures sampler2D→shader and
        // texture()→.eval() conversion, which is the common case for
        // both Shadertoy-style shaders and round-tripped SKSL→GLSL files.
        context.glsl_dialect = binding::GlslDialect::kOpenGLCore;
        context.glsl_version = 450;
        context.runtime_effect_mode = true;
    }

    return context;
}

const binding::BuiltinBinding* GLSLSkSLCodeGenerator::getBuiltinBindingByGlslName(std::string_view glslName) const {
    // Try exact match first (bindings store full GLSL names like "gl_FragCoord")
    const binding::BuiltinBinding* binding = binding::MatchBuiltinByGlslPrimarySpelling(glslName);
    if (binding && binding::BuiltinAppliesToContext(*binding, this->getBindingContext())) {
        return binding;
    }

    // Also try surface-form match for dialect variants (e.g. gl_VertexIndex vs gl_VertexID)
    binding = binding::MatchBuiltinBySurfaceForm(
            binding::SurfaceLanguage::kGLSL,
            binding::SurfaceFormKind::kIdentifier,
            glslName);
    if (binding && binding::BuiltinAppliesToContext(*binding, this->getBindingContext())) {
        return binding;
    }

    return nullptr;
}

const binding::IntrinsicBinding* GLSLSkSLCodeGenerator::getIntrinsicBindingByGlslName(std::string_view glslName) const {
    const binding::IntrinsicBinding* binding = binding::MatchIntrinsicByGlslPrimarySpelling(glslName);
    if (binding && binding::IntrinsicAppliesToContext(*binding, this->getBindingContext())) {
        return binding;
    }
    return nullptr;
}

// ============================================================================
// Expression writers
// ============================================================================

void GLSLSkSLCodeGenerator::writeExpression(TIntermTyped* expr) {
    if (!expr) return;

    if (auto* binary = expr->getAsBinaryNode()) {
        writeBinaryExpression(binary);
    }
    else if (auto* unary = expr->getAsUnaryNode()) {
        writeUnaryExpression(unary);
    }
    else if (auto* aggregate = expr->getAsAggregate()) {
        writeAggregateExpression(aggregate);
    }
    else if (auto* constant = expr->getAsConstantUnion()) {
        writeConstantUnion(constant);
    }
    else if (auto* symbol = expr->getAsSymbolNode()) {
        writeSymbol(symbol);
    }
    else if (auto* selection = expr->getAsSelectionNode()) {
        writeSelection(selection);
    }
    else if (auto* method = expr->getAsMethodNode()) {
        writeMethod(method);
    }
    else {
        // TODO: Handle other expression types
        write("/* unhandled expression type */");
    }
}

void GLSLSkSLCodeGenerator::writeBinaryExpression(TIntermBinary* node) {
    TOperator op = node->getOp();

    // Handle special binary operators
    switch (op) {
        case EOpIndexDirect:
            // Vector indexing → swizzle notation
            if (node->getLeft()->getType().isVector()) {
                writeExpression(node->getLeft());
                write(".");
                if (auto* constant = node->getRight()->getAsConstantUnion()) {
                    int idx = constant->getConstArray()[0].getIConst();
                    static const char* rgba = "rgba";
                    if (idx >= 0 && idx < 4) {
                        write(std::string(1, rgba[idx]));
                    } else {
                        write("x");  // fallback
                    }
                } else {
                    write("x");  // dynamic index on vector — fallback
                }
                return;
            }
            // Array/matrix index
            writeExpression(node->getLeft());
            write("[");
            writeExpression(node->getRight());
            write("]");
            return;

        case EOpIndexIndirect:
            // Array/matrix/vector indirect index
            writeExpression(node->getLeft());
            write("[");
            writeExpression(node->getRight());
            write("]");
            return;

        case EOpIndexDirectStruct:
            // Struct field access
            {
                const TType& leftType = node->getLeft()->getType();
                // Uniform block member access: u.member → member (drop block instance name)
                if (leftType.getBasicType() == EbtBlock && leftType.getQualifier().storage == EvqUniform) {
                    if (auto* constant = node->getRight()->getAsConstantUnion()) {
                        const TTypeList* members = leftType.getStruct();
                        if (members) {
                            int fieldIndex = constant->getConstArray()[0].getIConst();
                            const TString& fieldName = (*members)[fieldIndex].type->getFieldName();
                            write(fieldName.c_str());
                            return;
                        }
                    }
                }
                // Regular struct field access
                writeExpression(node->getLeft());
                write(".");
                if (auto* constant = node->getRight()->getAsConstantUnion()) {
                    const TTypeList* members = nullptr;
                    if (leftType.isReference()) {
                        members = leftType.getReferentType()->getStruct();
                    } else {
                        members = leftType.getStruct();
                    }
                    if (members) {
                        int fieldIndex = constant->getConstArray()[0].getIConst();
                        const TString& fieldName = (*members)[fieldIndex].type->getFieldName();
                        write(fieldName.c_str());
                    } else {
                        write("_field_" + std::to_string(constant->getConstArray()[0].getIConst()));
                    }
                }
            }
            return;

        case EOpVectorSwizzle:
            // Vector swizzle
            writeExpression(node->getLeft());
            write(".");
            // The swizzle is encoded in an aggregate sequence of constant integers
            if (auto* agg = node->getRight()->getAsAggregate()) {
                static const char* components = "xyzw";
                for (auto* child : agg->getSequence()) {
                    if (auto* constant = child->getAsConstantUnion()) {
                        int idx = constant->getConstArray()[0].getIConst();
                        if (idx >= 0 && idx < 4) {
                            write(std::string(1, components[idx]));
                        }
                    }
                }
            } else if (auto* constant = node->getRight()->getAsConstantUnion()) {
                // Single component swizzle
                static const char* components = "xyzw";
                int idx = constant->getConstArray()[0].getIConst();
                if (idx >= 0 && idx < 4) {
                    write(std::string(1, components[idx]));
                }
            }
            return;
    }

    // GLSL % on float types: rewrite as mod(a, b) for SkSL compatibility
    if (op == EOpMod && (node->getLeft()->getType().getBasicType() == EbtFloat ||
                         node->getLeft()->getType().getBasicType() == EbtFloat16)) {
        write("mod(");
        writeExpression(node->getLeft());
        write(", ");
        writeExpression(node->getRight());
        write(")");
        return;
    }
    if (op == EOpModAssign && (node->getLeft()->getType().getBasicType() == EbtFloat ||
                                node->getLeft()->getType().getBasicType() == EbtFloat16)) {
        writeExpression(node->getLeft());
        write(" = mod(");
        writeExpression(node->getLeft());
        write(", ");
        writeExpression(node->getRight());
        write(")");
        return;
    }

    // Standard binary operators - add parentheses based on precedence
    int parentPrec = getOperatorPrecedence(op);

    // Left child
    bool needLeftParen = false;
    if (auto* leftBin = node->getLeft()->getAsBinaryNode()) {
        int leftPrec = getOperatorPrecedence(leftBin->getOp());
        if (leftPrec < parentPrec) needLeftParen = true;
    }
    if (needLeftParen) write("(");
    writeExpression(node->getLeft());
    if (needLeftParen) write(")");

    write(getBinaryOperatorSymbol(op));

    // Right child
    bool needRightParen = false;
    if (auto* rightBin = node->getRight()->getAsBinaryNode()) {
        int rightPrec = getOperatorPrecedence(rightBin->getOp());
        if (rightPrec <= parentPrec) needRightParen = true;
    }
    if (needRightParen) write("(");
    writeExpression(node->getRight());
    if (needRightParen) write(")");
}

void GLSLSkSLCodeGenerator::writeUnaryExpression(TIntermUnary* node) {
    TOperator op = node->getOp();

    switch (op) {
        case EOpPreIncrement:
        case EOpPreDecrement:
            // Prefix
            write(getUnaryOperatorSymbol(op));
            writeExpression(node->getOperand());
            break;

        case EOpPostIncrement:
        case EOpPostDecrement:
            // Postfix
            writeExpression(node->getOperand());
            write(getUnaryOperatorSymbol(op));
            break;

        case EOpConvNumeric:
            // Type conversion
            write(getTypeName(node->getType()));
            write("(");
            writeExpression(node->getOperand());
            write(")");
            break;

        default:
            // Check if it's a built-in function
            if (isBuiltInFunction(op)) {
                // fwidth / dFdx / dFdy: GPU derivatives → resolution-based approx
                if (op == EOpFwidth || op == EOpFwidthFine || op == EOpFwidthCoarse ||
                    op == EOpDPdx || op == EOpDPdy ||
                    op == EOpDPdxFine || op == EOpDPdyFine ||
                    op == EOpDPdxCoarse || op == EOpDPdyCoarse) {
                    // GPU derivatives — preserve as-is for GPU-backed rendering.
                    // SkSL RuntimeEffect (CPU) rejects these, but skslc with .frag
                    // mode passes them through to the GPU GLSL backend where they
                    // are natively supported.
                    write(getOperatorName(op));
                    write("(");
                    writeExpression(node->getOperand());
                    write(")");
                } else {
                    write(getOperatorName(op));
                    write("(");
                    writeExpression(node->getOperand());
                    write(")");
                }
            } else {
                // Regular unary operator
                write(getUnaryOperatorSymbol(op));
                writeExpression(node->getOperand());
            }
            break;
    }
}

void GLSLSkSLCodeGenerator::writeAggregateExpression(TIntermAggregate* node) {
    TOperator op = node->getOp();

    switch (op) {
        case EOpFunctionCall:
            // User-defined function call - clean name (strip mangled params)
            write(cleanFunctionName(node->getName()));
            write("(");
            {
                auto separator = false;
                for (auto* child : node->getSequence()) {
                    if (separator) write(", ");
                    separator = true;
                    writeExpression(child->getAsTyped());
                }
            }
            write(")");
            return;

        case EOpConstructInt:
        case EOpConstructUint:
        case EOpConstructFloat:
        case EOpConstructBool:
        case EOpConstructVec2:
        case EOpConstructVec3:
        case EOpConstructVec4:
        case EOpConstructMat2x2:
        case EOpConstructMat2x3:
        case EOpConstructMat2x4:
        case EOpConstructMat3x2:
        case EOpConstructMat3x3:
        case EOpConstructMat3x4:
        case EOpConstructMat4x2:
        case EOpConstructMat4x3:
        case EOpConstructMat4x4:
        case EOpConstructIVec2:
        case EOpConstructIVec3:
        case EOpConstructIVec4:
        case EOpConstructUVec2:
        case EOpConstructUVec3:
        case EOpConstructUVec4:
        case EOpConstructBVec2:
        case EOpConstructBVec3:
        case EOpConstructBVec4:
        case EOpConstructStruct:
            // Constructor
            write(getTypeName(node->getType()));
            write("(");
            {
                auto separator = false;
                for (auto* child : node->getSequence()) {
                    if (separator) write(", ");
                    separator = true;
                    writeExpression(child->getAsTyped());
                }
            }
            write(")");
            return;

        case EOpSequence:
            // Sequence of statements - handled elsewhere
            {
                for (auto* child : node->getSequence()) {
                    writeStatement(child);
                }
            }
            return;

        case EOpComma:
            // Comma expression: "a, b" → "a, b" in SkSL
            {
                auto& seq = node->getSequence();
                bool first = true;
                for (auto* child : seq) {
                    if (!first) write(", ");
                    first = false;
                    if (auto* typed = child->getAsTyped()) {
                        writeExpression(typed);
                    }
                }
            }
            return;

        default:
            // Transform texture(sampler2D, coord, ...) → sampler.eval(coord)
            // ONLY when we are in runtime-effect mode (indicated by provenance metadata)
            // or when the input provenance explicitly says this was a child.eval() call.
            // In non-runtime-effect GLSL, texture() calls are kept as-is.
            {
                bool isTextureOp = (op == EOpTexture || op == EOpTextureProj ||
                                    op == EOpTextureLod || op == EOpTextureOffset ||
                                    op == EOpTextureFetch || op == EOpTextureFetchOffset ||
                                    op == EOpTextureGrad || op == EOpTextureGradOffset ||
                                    op == EOpTextureGather || op == EOpTextureGatherOffset);

                auto it = node->getSequence().begin();
                if (isTextureOp && node->getSequence().size() >= 2) {
                    if (auto* calleeSym = (*it)->getAsSymbolNode()) {
                        const TType& samplerType = calleeSym->getType();
                        if (samplerType.getBasicType() == EbtSampler &&
                            samplerType.getSampler().isCombined()) {

                            // Determine whether this texture() should become .eval()
                            bool shouldEmitEval = false;
                            bool hasExplicitProvenance = false;

                            // Check provenance metadata first (applies to all calls)
                            if (getBindingContext().runtime_effect_mode) {
                                shouldEmitEval = true;
                            }

                            // Check if forward provenance explicitly confirms this call
                            if (provenanceConfig) {
                                std::string calleeName = calleeSym->getName().c_str();
                                const binding::ProvenanceEntry* entry =
                                    provenanceConfig->findFunctionCall(calleeName, "texture");
                                if (entry && entry->helper_key == "runtime_effect_child_eval") {
                                    shouldEmitEval = true;
                                    hasExplicitProvenance = true;
                                } else if (entry == nullptr && !getBindingContext().runtime_effect_mode) {
                                    // No provenance entry and not in runtime effect mode —
                                    // this is a plain GLSL texture() call, keep it.
                                    shouldEmitEval = false;
                                }
                            }

                            if (shouldEmitEval) {
                                // Record reverse provenance for bidirectional symmetry
                                if (hasExplicitProvenance) {
                                    std::string calleeName = calleeSym->getName().c_str();
                                    reverseProvenance.recordFunctionCall(
                                        calleeName, "eval", "texture",
                                        "runtime_effect_child_eval");
                                } else if (getBindingContext().runtime_effect_mode) {
                                    std::string calleeName = calleeSym->getName().c_str();
                                    reverseProvenance.recordFunctionCall(
                                        calleeName, "eval", "texture",
                                        "runtime_effect_child_eval");
                                }

                                writeExpression((*it)->getAsTyped());
                                write(".eval(");
                                ++it;
                                {
                                    TIntermTyped* coord = (*it)->getAsTyped();
                                    // SkSL .eval() expects pixel coordinates, but GLSL
                                    // texture() expects normalized UV [0,1].  Two cases:
                                    //
                                    // 1) coord is  pixel / resolution  → just pass pixel
                                    //    (the division normalized pixel coords for GLSL;
                                    //     .eval() wants the raw pixel coords)
                                    //
                                    // 2) coord is already a normalized value (no division)
                                    //    → multiply back by the resolution uniform so that
                                    //    .eval() receives pixel coordinates.
                                    if (auto* divBin = coord->getAsBinaryNode()) {
                                        if (divBin->getOp() == EOpDiv) {
                                            // Case 1: strip the division, emit the numerator
                                            writeExpression(divBin->getLeft());
                                            write(")");
                                            return;
                                        }
                                    }
                                    // Case 2: coord is already normalized.
                                    // Multiply by resolution uniform to convert normalized
                                    // UV → pixel coordinates for .eval(). Defaults to the
                                    // global resolutionVarName (iResolution). Textures
                                    // should match output resolution for correct mapping.
                                    if (!resolutionVarName.empty()) {
                                        write(resolutionVarName);
                                        write(".xy * (");
                                        writeExpression(coord);
                                        write(")");
                                    } else {
                                        writeExpression(coord);
                                    }
                                }
                                write(")");
                                return;
                            }
                            // else: fall through to default aggregate handling (emit as texture())
                        }
                    }
                }
            }

            // Built-in function call
            if (isBuiltInFunction(op)) {
                // fwidth / dFdx / dFdy are GPU derivative functions not directly
                // available in SkSL RuntimeEffect. Approximate using the resolution
                // uniform: fwidth(x) ≈ abs(x) * (1.0 / iResolution.y) + (1.0 / iResolution.y)
                // This gives pixel-sized smoothing for anti-aliasing.
                std::string funcName = getOperatorName(op);

                const binding::IntrinsicBinding* intrinsic = getIntrinsicBindingByGlslName(funcName);

                // SkSL RuntimeEffect hyperbolic functions only accept half types.
                // GLSL uses float for these — wrap arguments with half() casts.
                bool needsHalfArgs = (op == EOpTanh || op == EOpSinh || op == EOpCosh ||
                                      op == EOpAsinh || op == EOpAcosh || op == EOpAtanh);

                // Use SkSL spelling if available
                if (intrinsic && !intrinsic->sksl_spelling.empty()) {
                    write(std::string(intrinsic->sksl_spelling));
                } else {
                    write(funcName);
                }

                write("(");
                auto separator = false;
                for (auto* child : node->getSequence()) {
                    if (separator) write(", ");
                    separator = true;
                    auto* typed = child->getAsTyped();
                    if (needsHalfArgs && typed &&
                        (typed->getType().getBasicType() == EbtFloat ||
                         typed->getType().getBasicType() == EbtFloat16)) {
                        write("half(");
                        writeExpression(typed);
                        write(")");
                    } else {
                        writeExpression(typed);
                    }
                }
                write(")");
            } else {
                // TODO: Handle other aggregate operations
                write("/* unhandled aggregate: ");
                write(node->getName().c_str());
                write(" */");
            }
            break;
    }
}

void GLSLSkSLCodeGenerator::writeConstantUnion(TIntermConstantUnion* node) {
    const TType& type = node->getType();
    const TConstUnionArray& constArray = node->getConstArray();

    if (type.isArray()) {
        write(getTypeName(type));
        write("(");
        int totalSize = type.getCumulativeArraySize();
        for (int i = 0; i < totalSize; ++i) {
            if (i > 0) write(", ");
            switch (constArray[i].getType()) {
                case EbtInt:    write(std::to_string(constArray[i].getIConst())); break;
                case EbtUint:   write(std::to_string(constArray[i].getUConst()) + "u"); break;
                case EbtFloat:
                case EbtDouble: write(formatFloat(constArray[i].getDConst())); break;
                case EbtBool:   write(constArray[i].getBConst() ? "true" : "false"); break;
                default:        write("0"); break;
            }
        }
        write(")");
    } else if (type.isVector()) {
        if (type.getVectorSize() == 1) {
            switch (constArray[0].getType()) {
                case EbtInt:    write(std::to_string(constArray[0].getIConst())); break;
                case EbtUint:   write(std::to_string(constArray[0].getUConst()) + "u"); break;
                case EbtFloat:
                case EbtDouble: write(formatFloat(constArray[0].getDConst())); break;
                case EbtBool:   write(constArray[0].getBConst() ? "true" : "false"); break;
                default:        write("0"); break;
            }
        } else {
            write(getTypeName(type));
            write("(");
            for (int i = 0; i < type.getVectorSize(); ++i) {
                if (i > 0) write(", ");
                switch (constArray[i].getType()) {
                    case EbtInt:    write(std::to_string(constArray[i].getIConst())); break;
                    case EbtUint:   write(std::to_string(constArray[i].getUConst())); break;
                    case EbtFloat:
                    case EbtDouble: write(formatFloat(constArray[i].getDConst())); break;
                    case EbtBool:   write(constArray[i].getBConst() ? "true" : "false"); break;
                    default:        write("0"); break;
                }
            }
            write(")");
        }
    } else if (type.isMatrix()) {
        write(getTypeName(type));
        write("(");
        int cols = type.getMatrixCols();
        int rows = type.getMatrixRows();
        for (int i = 0; i < cols * rows; ++i) {
            if (i > 0) write(", ");
            switch (constArray[i].getType()) {
                case EbtFloat:
                case EbtDouble: write(formatFloat(constArray[i].getDConst())); break;
                default:        write("0.0"); break;
            }
        }
        write(")");
    } else {
        switch (constArray[0].getType()) {
            case EbtInt:
                write(std::to_string(constArray[0].getIConst()));
                break;
            case EbtUint:
                write(std::to_string(constArray[0].getUConst()));
                write("u");
                break;
            case EbtFloat:
            case EbtDouble:
                write(formatFloat(constArray[0].getDConst()));
                break;
            case EbtBool:
                write(constArray[0].getBConst() ? "true" : "false");
                break;
            default:
                write("0");
                break;
        }
    }
}

void GLSLSkSLCodeGenerator::writeSymbol(TIntermSymbol* node) {
    const TString& name = node->getName();
    std::string_view nameStr(name.c_str());

    // Inside mainImage/main body, redirect gl_FragCoord → fragCoord parameter
    if (inMainImageBody && !mainImageFragCoordName.empty() &&
        nameStr.substr(0, 3) == "gl_" && nameStr == "gl_FragCoord") {
        write(mainImageFragCoordName);
        return;
    }

    // Check if this is a GLSL builtin (starts with "gl_")
    if (nameStr.substr(0, 3) == "gl_") {
        const binding::BuiltinBinding* builtin = getBuiltinBindingByGlslName(nameStr);
        if (builtin) {
            // Use SkSL spelling from binding registry
            if (!builtin->sksl_spelling.empty()) {
                // Record reverse provenance for bidirectional round-trip
                if (!builtin->helper_key.empty()) {
                    reverseProvenance.recordBuiltinVariable(
                        builtin->sksl_spelling,
                        builtin->glsl_primary_spelling,
                        builtin->helper_key);
                }
                write(std::string(builtin->sksl_spelling));
                return;
            }
        }
    }

    write(name.c_str());
}

void GLSLSkSLCodeGenerator::writeSelection(TIntermSelection* node) {
    // Ternary operator or if-else
    if (node->getBasicType() != EbtVoid) {
        // Ternary expression
        writeExpression(node->getCondition());
        write(" ? ");
        writeExpression(node->getTrueBlock()->getAsTyped());
        write(" : ");
        writeExpression(node->getFalseBlock()->getAsTyped());
    } else {
        // If-else statement
        writeIndent();
        write("if (");
        writeExpression(node->getCondition());
        writeLine(") {");
        indentDepth++;
        writeStatement(node->getTrueBlock());
        indentDepth--;
        writeIndent();
        if (node->getFalseBlock()) {
            writeLine("} else {");
            indentDepth++;
            writeStatement(node->getFalseBlock());
            indentDepth--;
            writeIndent();
        }
        writeLine("}");
    }
}

void GLSLSkSLCodeGenerator::writeMethod(TIntermMethod* node) {
    // Method call on an object
    writeExpression(node->getObject());
    write(".");
    write(node->getMethodName().c_str());
    // TODO: Add arguments
}

// ============================================================================
// Statement writers
// ============================================================================

void GLSLSkSLCodeGenerator::writeStatement(TIntermNode* node) {
    if (!node) return;

    if (auto* aggregate = node->getAsAggregate()) {
        TOperator op = aggregate->getOp();
        if (op == EOpSequence || op == EOpScope) {
            auto& seq = aggregate->getSequence();

            // Detect variable declaration pattern:
            // EOpSequence with a single child that is EOpAssign to a temp variable
            if (seq.size() == 1) {
                auto* s1child = seq[0];
                                auto* child = seq[0];
                if (auto* binary = child->getAsBinaryNode()) {
                    if (binary->getOp() == EOpAssign) {
                        auto* left = binary->getLeft()->getAsSymbolNode();
                        if (left && left->getType().getQualifier().storage == EvqTemporary) {
                            // In entry-point mode, skip redeclaration of the fragCoord param.
                            if (inMainImageBody && mainImageFragCoordName == left->getName().c_str()) {
                                // Still record gl_* builtins from the rhs for provenance
                                recordGlslBuiltins(binary->getRight());
                                return;
                            }
                            long long varId = left->getId();
                            if (declaredVariables.find(varId) == declaredVariables.end()) {
                                declaredVariables.insert(varId);
                                writeIndent();
                                const TType& varType = left->getType();
                                if (varType.getQualifier().isConstant()) {
                                    write("const ");
                                }
                                write(getTypeName(varType));
                                write(" ");
                                write(left->getName().c_str());
                                write(" = ");
                                writeExpression(binary->getRight());
                                writeLine(";");
                                return;
                            }
                        }
                    }
                }
            }

            // Look ahead for for-loop initializer pattern:
            // A declaration sequence followed by a loop node
            for (size_t i = 0; i < seq.size(); ++i) {
                // Check if this is a declaration followed by a loop
                if (i + 1 < seq.size()) {
                    auto* nextLoop = seq[i + 1]->getAsLoopNode();
                    if (nextLoop && nextLoop->testFirst()) {
                        // Check if current node is a declaration sequence
                        auto* declAgg = seq[i]->getAsAggregate();
                        if (declAgg && (declAgg->getOp() == EOpSequence || declAgg->getOp() == EOpScope)) {
                            auto& declSeq = declAgg->getSequence();
                            if (declSeq.size() == 1) {
                                auto* binary = declSeq[0]->getAsBinaryNode();
                                if (binary && binary->getOp() == EOpAssign) {
                                    auto* left = binary->getLeft()->getAsSymbolNode();
                                    if (left && left->getType().getQualifier().storage == EvqTemporary) {
                                        declaredVariables.insert(left->getId());
                                        // Emit as for-loop with initializer
                                        writeIndent();
                                        write("for (");
                                        const TType& varType = left->getType();
                                        write(getTypeName(varType));
                                        write(" ");
                                        write(left->getName().c_str());
                                        write(" = ");
                                        writeExpression(binary->getRight());
                                        write("; ");
                                        if (nextLoop->getTest()) {
                                            TIntermTyped* testExpr = nextLoop->getTestExpr();
                                            if (testExpr) writeExpression(testExpr);
                                        }
                                        write("; ");
                                        if (nextLoop->getTerminal()) {
                                            writeExpression(nextLoop->getTerminal());
                                        }
                                        writeLine(") {");
                                        indentDepth++;
                                        writeStatement(nextLoop->getBody());
                                        indentDepth--;
                                        writeIndent();
                                        writeLine("}");
                                        i++; // skip the loop node
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }
                // In mainImage body: detect "outColor = expr; return;" → "return half4(expr);"
                if (inMainImageBody && !mainImageFragColorName.empty() &&
                    i + 1 < seq.size()) {
                    auto* nextBranch = seq[i + 1]->getAsBranchNode();
                    if (nextBranch && nextBranch->getFlowOp() == EOpReturn &&
                        !nextBranch->getExpression()) {
                        auto* assignBin = seq[i]->getAsBinaryNode();
                        if (assignBin && assignBin->getOp() == EOpAssign) {
                            auto* lhs = assignBin->getLeft()->getAsSymbolNode();
                            if (lhs && lhs->getName().c_str() == mainImageFragColorName) {
                                writeIndent();
                                write("return half4(");
                                writeExpression(assignBin->getRight());
                                writeLine(");");
                                i++;  // skip the bare return
                                continue;
                            }
                        }
                    }
                }
                writeStatement(seq[i]);
            }
        } else if (op == EOpFunction) {
            writeFunction(aggregate);
        } else if (op == EOpParameters) {
            // Handled by writeFunction
        } else if (op == EOpDeclare) {
            writeIndent();
            for (auto* child : aggregate->getSequence()) {
                if (auto* varDecl = child->getAsVariableDecl()) {
                    writeVariableDecl(varDecl);
                } else if (auto* symbol = child->getAsSymbolNode()) {
                    const TType& varType = symbol->getType();
                    if (varType.getQualifier().storage == EvqUniform ||
                        varType.getQualifier().storage == EvqVaryingIn ||
                        varType.getQualifier().storage == EvqVaryingOut ||
                        varType.getQualifier().storage == EvqConst)
                        continue;  // handled by writeGlobalDeclaration
                    write(getTypeName(varType));
                    write(" ");
                    write(symbol->getName().c_str());
                    writeLine(";");
                } else if (auto* binary = child->getAsBinaryNode()) {
                    // Mixed declaration like "mat3 m = mat3(...)" inside EOpDeclare
                    if (binary->getOp() == EOpAssign) {
                        if (auto* sym = binary->getLeft()->getAsSymbolNode()) {
                            const TType& varType = sym->getType();
                            if (varType.getQualifier().storage == EvqUniform)
                                continue;
                            write(getTypeName(varType));
                            write(" ");
                            write(sym->getName().c_str());
                            write(" = ");
                            writeExpression(binary->getRight());
                            writeLine(";");
                            if (auto* s = binary->getLeft()->getAsSymbolNode())
                                declaredVariables.insert(s->getId());
                        }
                    }
                }
            }
        } else {
            // Expression statement
            writeIndent();
            writeExpression(aggregate->getAsTyped());
            writeLine(";");
        }
    }
    else if (auto* selection = node->getAsSelectionNode()) {
        writeSelection(selection);
    }
    else if (auto* loop = node->getAsLoopNode()) {
        writeLoop(loop);
    }
    else if (auto* branch = node->getAsBranchNode()) {
        writeBranch(branch);
    }
    else if (auto* switchNode = node->getAsSwitchNode()) {
        writeSwitch(switchNode);
    }
    else if (auto* varDecl = node->getAsVariableDecl()) {
        writeVariableDecl(varDecl);
    }
    else if (auto* symbol = node->getAsSymbolNode()) {
        // Standalone symbol: variable declaration without initializer
        // (e.g. "float s;" from multi-declaration "float s, i;")
        const TType& varType = symbol->getType();
        writeIndent();
        write(getTypeName(varType));
        write(" ");
        write(symbol->getName().c_str());
        writeLine(";");
    }
    else if (auto* binary = node->getAsBinaryNode()) {
        // Multi-declaration initializer: "float d = ..., i = ...;" splits into
        // individual EOpAssign nodes in an EOpSequence. Each needs a type prefix.
        if (binary->getOp() == EOpAssign) {
            if (auto* left = binary->getLeft()->getAsSymbolNode()) {
                long long varId = left->getId();
                std::string varName = left->getName().c_str();
                if (declaredVariables.find(varId) == declaredVariables.end() &&
                    declaredLocalNames.find(varName) == declaredLocalNames.end()) {
                    declaredVariables.insert(varId);
                    declaredLocalNames.insert(varName);
                    writeIndent();
                    write(getTypeName(left->getType()));
                    write(" ");
                    write(varName);
                    write(" = ");
                    writeExpression(binary->getRight());
                    writeLine(";");
                    return;
                }
            }
        }
        // Fall through: other binary ops become expression statements
        writeIndent();
        writeExpression(binary);
        writeLine(";");
    }
    else if (auto* typed = node->getAsTyped()) {
        // Expression statement
        writeIndent();
        writeExpression(typed);
        writeLine(";");
    }
    else {
        writeIndent();
        write("/* unhandled stmt */");
        writeLine("");
    }
}

void GLSLSkSLCodeGenerator::writeBlock(TIntermAggregate* node) {
    writeIndent();
    writeLine("{");
    indentDepth++;

    for (auto* child : node->getSequence()) {
        writeStatement(child);
    }

    indentDepth--;
    writeIndent();
    writeLine("}");
}

void GLSLSkSLCodeGenerator::writeLoop(TIntermLoop* node) {
    if (node->testFirst()) {
        // SkSL requires for-loop init variable to appear on the left-hand
        // side of the condition. When the init was not emitted by the
        // surrounding EOpSequence handler (i.e. the loop has a truly empty
        // init or uses externally-declared variables), extract the loop
        // variable from the condition or terminal and emit a no-op
        // initializer like "float i = i;" so SkSL accepts the loop.
        writeIndent();
        write("for (");

        // Attempt to find the loop variable from the condition or terminal
        TIntermTyped* testExpr = nullptr;
        if (node->getTest()) {
            if (node->getTest()->getAsVariableDecl())
                testExpr = node->getTestExpr();
            else
                testExpr = node->getTest()->getAsTyped();
        }
        // Extract a symbol name from the condition (leftmost symbol)
        std::string loopVar;
        if (testExpr) {
            class LoopVarExtractor : public TIntermTraverser {
            public:
                std::string& found;
                LoopVarExtractor(std::string& f) : TIntermTraverser(true, false, false, false), found(f) {}
                void visitSymbol(TIntermSymbol* s) override {
                    if (s && found.empty()) found = s->getName().c_str();
                }
            };
            LoopVarExtractor extractor(loopVar);
            testExpr->traverse(&extractor);
        }
        // Also check the terminal for a loop variable
        if (loopVar.empty() && node->getTerminal()) {
            class LoopVarExtractor : public TIntermTraverser {
            public:
                std::string& found;
                LoopVarExtractor(std::string& f) : TIntermTraverser(true, false, false, false), found(f) {}
                void visitSymbol(TIntermSymbol* s) override {
                    if (s && found.empty()) found = s->getName().c_str();
                }
            };
            LoopVarExtractor extractor2(loopVar);
            node->getTerminal()->traverse(&extractor2);
        }

        if (!loopVar.empty() && declaredLocalNames.find(loopVar) == declaredLocalNames.end()) {
            // Variable not yet declared — emit "float var = 0.0" as init
            write("float ");
            write(loopVar);
            write(" = 0.0");
        }
        // else: variable already declared — leave init empty (SkSL accepts "; cond; inc")
        write("; ");

        // Condition
        if (testExpr) writeExpression(testExpr);
        write("; ");

        // Iterator
        if (node->getTerminal()) {
            writeExpression(node->getTerminal());
        }
        writeLine(") {");

        indentDepth++;
        writeStatement(node->getBody());
        indentDepth--;

        writeIndent();
        writeLine("}");
    } else {
        // do-while loop
        writeIndent();
        writeLine("do {");

        indentDepth++;
        writeStatement(node->getBody());
        indentDepth--;

        writeIndent();
        write("} while (");
        if (node->getTest()) {
            if (auto* typed = node->getTest()->getAsTyped()) {
                writeExpression(typed);
            }
        }
        writeLine(");");
    }
}

void GLSLSkSLCodeGenerator::writeBranch(TIntermBranch* node) {
    // In entry-point mode, skip bare "return;" since we synthesize our own
    if (inMainImageBody && node->getFlowOp() == EOpReturn && !node->getExpression()) {
        return;
    }

    writeIndent();

    switch (node->getFlowOp()) {
        case EOpReturn:
            write("return");
            if (node->getExpression()) {
                write(" ");
                writeExpression(node->getExpression());
            }
            writeLine(";");
            break;

        case EOpBreak:
            writeLine("break;");
            break;

        case EOpContinue:
            writeLine("continue;");
            break;

        case EOpKill:
        case EOpTerminateInvocation:
            writeLine("discard;");
            break;

        case EOpDemote:
            // TODO: SkSL demote handling
            writeLine("demote;");  // May need special handling
            break;

        default:
            writeLine("/* unknown branch */");
            break;
    }
}

void GLSLSkSLCodeGenerator::writeSwitch(TIntermSwitch* node) {
    writeIndent();
    write("switch (");
    writeExpression(node->getCondition()->getAsTyped());
    writeLine(") {");

    indentDepth++;

    if (auto* body = node->getBody()) {
        for (auto* child : body->getSequence()) {
            if (auto* branch = child->getAsBranchNode()) {
                TOperator op = branch->getFlowOp();
                if (op == EOpCase) {
                    // Finish previous case body before starting new case
                    writeIndent();
                    write("case ");
                    if (auto* expr = branch->getExpression()) {
                        writeExpression(expr);
                    }
                    writeLine(":");
                    indentDepth++;
                } else if (op == EOpDefault) {
                    writeIndent();
                    writeLine("default:");
                    indentDepth++;
                }
            } else {
                // This is a case/default body statement
                writeStatement(child);
            }
        }
    }

    // Decrease indent for each level added by cases
    // (We over-indented above; clean up here)
    indentDepth--;
    writeIndent();
    writeLine("}");
}

void GLSLSkSLCodeGenerator::writeVariableDecl(TIntermVariableDecl* node) {
    if (!node) return;

    writeIndent();

    // Get the declaration symbol
    const TIntermSymbol* declSymbol = node->getDeclSymbol();
    if (!declSymbol) {
        writeLine("/* empty variable declaration */");
        return;
    }

    const TType& type = declSymbol->getType();
    const TQualifier& qualifier = type.getQualifier();

    // Write const qualifier
    if (qualifier.storage == EvqConst) {
        write("const ");
    }

    // Write type
    write(getTypeName(type));

    // Write array dimensions if present
    if (type.isArray()) {
        write("[");
        if (type.getArraySizes()) {
            for (int i = 0; i < type.getArraySizes()->getNumDims(); ++i) {
                if (i > 0) write("][");
                int size = type.getArraySizes()->getDimSize(i);
                if (size > 0) {
                    write(std::to_string(size));
                }
            }
        }
        write("]");
    }

    // Write variable name
    write(" ");
    write(declSymbol->getName().c_str());

    // Write initializer if present
    const TIntermNode* initNode = node->getInitNode();
    if (initNode) {
        write(" = ");
        if (auto* typed = initNode->getAsTyped()) {
            writeExpression(const_cast<TIntermTyped*>(typed));
        } else if (auto* agg = initNode->getAsAggregate()) {
            writeAggregateExpression(const_cast<TIntermAggregate*>(agg));
        }
    }

    writeLine(";");
}

// ============================================================================
// Program structure writers
// ============================================================================

static std::string cleanFunctionName(const TString& mangledName) {
    std::string name(mangledName.c_str());
    // glslang stores mangled names like "sampleImage(vf2;" or "main("
    // The clean name is everything before the first '('
    size_t parenPos = name.find('(');
    if (parenPos != std::string::npos) {
        name = name.substr(0, parenPos);
    }
    return name;
}

void GLSLSkSLCodeGenerator::writeFunction(TIntermAggregate* node) {
    const TString& name = node->getName();
    currentFunctionName = cleanFunctionName(name);
    declaredVariables.clear();
    declaredLocalNames.clear();

    // Find return type and parameters
    TIntermSequence& sequence = node->getSequence();
    TIntermTyped* returnType = nullptr;
    TIntermAggregate* params = nullptr;
    TIntermAggregate* body = nullptr;

    for (auto* child : sequence) {
        if (auto* typed = child->getAsTyped()) {
            if (!returnType && typed->getType().getBasicType() != EbtVoid) {
                // Could be return type node in some representations
            }
        }
        if (auto* agg = child->getAsAggregate()) {
            if (agg->getOp() == EOpParameters) {
                params = agg;
            } else if (agg->getOp() == EOpSequence || agg->getOp() == EOpScope) {
                body = agg;
            }
        }
    }

    // Write function signature
    writeIndent();

    std::string cleanName = cleanFunctionName(name);
    bool isShaderToyEntry = (stage == EShLangFragment) && (cleanName == "mainImage" || cleanName == "main");

    // Detect the ShaderToy-style fragColor out-parameter so that uses of it
    // inside the body emit the local variable name unchanged, and we can
    // synthesize the SkSL `half4 main(float2 fragCoord)` entry.
    // std::string fragColorParamName;
    std::string fragCoordParamName = "fragCoord";
    if (isShaderToyEntry && params) {
        auto& pseq = params->getSequence();
        if (pseq.size() >= 1) {
            if (auto* s = pseq[0]->getAsSymbolNode()) fragColorParamName = s->getName().c_str();
        }
        if (pseq.size() >= 2) {
            if (auto* s = pseq[1]->getAsSymbolNode()) fragCoordParamName = s->getName().c_str();
        }
    }

    if (isShaderToyEntry) {
        // Record provenance: ShaderToy entry-point → SkSL runtime effect entry
        reverseProvenance.recordBuiltinVariable(
            std::string(fragColorParamName.empty() ? "fragColor" : fragColorParamName),
            "out vec4 fragColor", "entry_point_transform");

        // SkSL fragment entry: half4 main(float2 fragCoord)
        write("half4 main(float2 ");
        write(fragCoordParamName);
        writeLine(") {");
        indentDepth++;
        // Synthesize the local variable that the body writes into.
        writeIndent();
        write("float4 ");
        write(fragColorParamName);
        writeLine(";");
        // Track synthesized locals so body EOpAssign nodes don't re-emit the type
        declaredLocalNames.insert(fragColorParamName);
        declaredLocalNames.insert(fragCoordParamName);
        inMainImageBody = true;
        mainImageFragColorName = fragColorParamName;
        mainImageFragCoordName = fragCoordParamName;
    } else {
        // Return type
        const TType& retType = node->getType();
        write(getTypeName(retType));
        write(" ");

        // Function name (strip glslang's mangled parameter encoding)
        write(cleanName);

        // Parameters
        write("(");
        if (params) {
            writeFunctionParameters(params);
        }
        writeLine(") {");
        indentDepth++;
    }

    // Function body
    // Pre-scan: glslang may strip pure declarations (e.g. "float s, i;")
    // from the function body. Walk the body to find variables that are
    // USED but neither declared as parameters/synthesized-locals nor
    // assigned via EOpAssign (which the existing handlers cover with
    // combined "type name = value;" output). Only emit bare "type name;"
    // declarations for the remaining "pure reader" variables.
    if (body) {
        std::unordered_set<std::string> usedInBody;
        std::unordered_set<std::string> preDeclared;

        // Pass 1: collect all declaration targets (EOpAssign + EOpDeclare)
        class DeclScanner : public TIntermTraverser {
        public:
            std::unordered_set<std::string>& declSet;
            DeclScanner(std::unordered_set<std::string>& d)
                : TIntermTraverser(true, true, true, false), declSet(d) {}
            bool visitBinary(TVisit visit, TIntermBinary* b) override {
                if (visit == EvPreVisit && b->getOp() == EOpAssign) {
                    if (auto* sym = b->getLeft()->getAsSymbolNode())
                        declSet.insert(sym->getName().c_str());
                }
                return true;
            }
            bool visitAggregate(TVisit visit, TIntermAggregate* agg) override {
                if (visit == EvPreVisit && agg->getOp() == EOpDeclare) {
                    for (auto* child : agg->getSequence()) {
                        if (auto* sym = child->getAsSymbolNode())
                            declSet.insert(sym->getName().c_str());
                        else if (auto* vd = child->getAsVariableDecl())
                            if (auto* ds = vd->getDeclSymbol())
                                declSet.insert(ds->getName().c_str());
                    }
                }
                return true;
            }
        };
        DeclScanner declScanner(preDeclared);
        body->traverse(&declScanner);

        // Pass 2: collect ALL symbol references
        class AllSymScanner : public TIntermTraverser {
        public:
            std::unordered_set<std::string>& usedSet;
            AllSymScanner(std::unordered_set<std::string>& u)
                : TIntermTraverser(true, true, true, false), usedSet(u) {}
            void visitSymbol(TIntermSymbol* s) override {
                if (s) usedSet.insert(s->getName().c_str());
            }
        };
        AllSymScanner symScanner(usedInBody);
        body->traverse(&symScanner);

        // Function parameters and Shadertoy-synthesized locals are declared
        if (params) {
            for (auto* child : params->getSequence()) {
                if (auto* sym = child->getAsSymbolNode())
                    preDeclared.insert(sym->getName().c_str());
            }
        }
        for (auto& n : declaredLocalNames) preDeclared.insert(n);

        // Only pre-declare symbols that are NOT EOpAssign targets
        std::vector<std::string> undeclared;
        for (auto& name : usedInBody) {
            if (preDeclared.find(name) == preDeclared.end())
                undeclared.push_back(name);
        }
        if (!undeclared.empty()) {
            std::sort(undeclared.begin(), undeclared.end());
            std::unordered_map<std::string, std::string> varTypes;
            class TypeFinder : public TIntermTraverser {
            public:
                std::unordered_set<std::string>& targets;
                std::unordered_map<std::string, std::string>& types;
                GLSLSkSLCodeGenerator* gen;
                TypeFinder(std::unordered_set<std::string>& t,
                          std::unordered_map<std::string, std::string>& tp,
                          GLSLSkSLCodeGenerator* g)
                    : TIntermTraverser(true, true, true, false), targets(t), types(tp), gen(g) {}
                void visitSymbol(TIntermSymbol* s) override {
                    if (!s) return;
                    std::string n = s->getName().c_str();
                    if (targets.find(n) != targets.end() && types.find(n) == types.end()) {
                        TStorageQualifier sq = s->getType().getQualifier().storage;
                        if (sq == EvqUniform || sq == EvqVaryingIn ||
                            sq == EvqVaryingOut || sq == EvqConst ||
                            sq == EvqGlobal ||
                            sq == EvqFragCoord || sq == EvqFragColor ||
                            sq == EvqFragDepth || sq == EvqVertexId ||
                            sq == EvqInstanceId || sq == EvqPosition ||
                            sq == EvqPointSize || sq == EvqFace ||
                            sq == EvqPointCoord || sq == EvqClipVertex)
                            return;
                        types[n] = gen->getTypeName(s->getType());
                    }
                }
            };
            std::unordered_set<std::string> targetSet(undeclared.begin(), undeclared.end());
            TypeFinder typeFinder(targetSet, varTypes, this);
            body->traverse(&typeFinder);
            for (auto& name : undeclared) {
                auto it = varTypes.find(name);
                if (it != varTypes.end()) {
                    writeIndent();
                    write(it->second);
                    write(" ");
                    write(name);
                    writeLine(";");
                    declaredLocalNames.insert(name);
                }
            }
        }
    }

    inFunctionBody = true;

    if (body) {
        for (auto* child : body->getSequence()) {
            writeStatement(child);
        }
    }

    inFunctionBody = false;

    if (isShaderToyEntry) {
        writeIndent();
        write("return half4(");
        write(fragColorParamName);
        // Shadertoy ignores alpha; force 1.0 so the output matches GLSL renderer
        // (which writes RGB-only PPM and discards alpha).
        writeLine(".rgb, 1.0);");
        inMainImageBody = false;
        mainImageFragColorName.clear();
        mainImageFragCoordName.clear();
    }

    indentDepth--;

    writeIndent();
    writeLine("}");
    writeLine("");

    currentFunctionName.clear();
}

void GLSLSkSLCodeGenerator::writeFunctionParameters(TIntermAggregate* node) {
    if (!node) return;

    auto& sequence = node->getSequence();
    
    bool first = true;

    for (auto* child : sequence) {
        if (!first) write(", ");
        first = false;

        if (auto* symbol = child->getAsSymbolNode()) {
            const TType& type = symbol->getType();
            const TQualifier& qualifier = type.getQualifier();

            // Parameter qualifiers
            if (qualifier.isConstant()) {
                write("const ");
            }
            if (qualifier.storage == EvqInOut) {
                write("inout ");
            } else if (qualifier.storage == EvqOut) {
                write("out ");
            }
            // SkSL: strip "in" qualifier — parameters are "in" by default

            // Type
            write(getTypeName(type));

            // Array
            if (type.isArray()) {
                write("[]");
            }

            // Name
            write(" ");
            write(symbol->getName().c_str());

            // Track parameter as declared to prevent the EOpAssign→declaration
            // logic from re-emitting a type prefix for the parameter's first
            // assignment in the function body.
            declaredVariables.insert(symbol->getId());
        }
    }
}

void GLSLSkSLCodeGenerator::writeGlobalDeclaration(TIntermAggregate* node) {
    for (auto* child : node->getSequence()) {
        if (auto* symbol = child->getAsSymbolNode()) {
            // Track global names so the pre-scan doesn't shadow them
            globalNames.insert(symbol->getName().c_str());
            const TType& varType = symbol->getType();
            const TQualifier& qualifier = varType.getQualifier();

            // Drop unused uniforms / inputs to avoid resource waste.
            // Exception: iChannelResolution is needed for .eval() coordinate conversion
            // even though it may not be directly referenced in the GLSL source.
            std::string symName = symbol->getName().c_str();
            if ((qualifier.storage == EvqUniform || qualifier.storage == EvqVaryingIn ||
                 qualifier.storage == EvqVaryingOut) &&
                varType.getBasicType() != EbtBlock &&
                usedSymbolNamesComputed &&
                usedSymbolNames.find(symName) == usedSymbolNames.end() &&
                symName != "iChannelResolution") {
                continue;
            }

            // Detect resolution uniform (same heuristic as the inline path)
            if (qualifier.storage == EvqUniform &&
                varType.isVector() && (varType.getVectorSize() == 2 || varType.getVectorSize() == 3) &&
                varType.getBasicType() == EbtFloat) {
                std::string name(symbol->getName().c_str());
                std::string lower = name;
                for (auto& c : lower) c = char(tolower((unsigned char)c));
                if (lower.find("res") != std::string::npos) {
                    if (resolutionVarName.empty() || name == "iResolution")
                        resolutionVarName = name;
                } else if (resolutionVarName.empty()) {
                    resolutionVarName = name;
                }
            }

            // Handle uniform blocks — expand members as individual uniforms
            // without the block instance prefix (e.g. u.iResolution → iResolution)
            if (varType.getBasicType() == EbtBlock && qualifier.storage == EvqUniform) {
                const TTypeList* members = varType.getStruct();
                if (members) {
                    for (size_t i = 0; i < members->size(); ++i) {
                        const TType* memberType = (*members)[i].type;
                        // Detect resolution uniform among block members
                        if (memberType->isVector() && (memberType->getVectorSize() == 2 || memberType->getVectorSize() == 3) &&
                            memberType->getBasicType() == EbtFloat) {
                            std::string mname(memberType->getFieldName().c_str());
                            std::string mlower = mname;
                            for (auto& c : mlower) c = char(tolower((unsigned char)c));
                            if (mlower.find("res") != std::string::npos) {
                                resolutionVarName = mname;
                            } else if (resolutionVarName.empty()) {
                                resolutionVarName = mname;
                            }
                        }
                        writeIndent();
                        write("uniform ");
                        write(getTypeName(*memberType));
                        write(" ");
                        write(memberType->getFieldName().c_str());
                        writeLine(";");
                    }
                }
                continue;
            }

            writeIndent();
            if (qualifier.storage != EvqConst) {
                std::string layoutStr = getLayoutString(qualifier);
                if (!layoutStr.empty()) {
                    write(layoutStr);
                }
            }
            write(getQualifierString(qualifier, true));
            write(getTypeName(varType));
            write(" ");
            write(symbol->getName().c_str());
            if (varType.isArray()) {
                write("[");
                if (varType.getArraySizes()) {
                    for (int d = 0; d < varType.getArraySizes()->getNumDims(); ++d) {
                        if (d > 0) write("][");
                        int sz = varType.getArraySizes()->getDimSize(d);
                        if (sz > 0) write(std::to_string(sz));
                    }
                }
                write("]");
            }
            writeLine(";");
        }
    }
}

// ============================================================================
// Traversal callbacks
// ============================================================================

void GLSLSkSLCodeGenerator::visitSymbol(TIntermSymbol* symbol) {
    writeSymbol(symbol);
}

void GLSLSkSLCodeGenerator::visitConstantUnion(TIntermConstantUnion* constantUnion) {
    writeConstantUnion(constantUnion);
}

bool GLSLSkSLCodeGenerator::visitBinary(TVisit visit, TIntermBinary* binary) {
    if (visit == EvPreVisit) {
        writeBinaryExpression(binary);
        return false;  // Children already processed by writeBinaryExpression
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitUnary(TVisit visit, TIntermUnary* unary) {
    if (visit == EvPreVisit) {
        writeUnaryExpression(unary);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitSelection(TVisit visit, TIntermSelection* selection) {
    if (visit == EvPreVisit) {
        writeSelection(selection);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitAggregate(TVisit visit, TIntermAggregate* aggregate) {
    if (visit == EvPreVisit) {
        writeAggregateExpression(aggregate);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitLoop(TVisit visit, TIntermLoop* loop) {
    if (visit == EvPreVisit) {
        writeLoop(loop);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitBranch(TVisit visit, TIntermBranch* branch) {
    if (visit == EvPreVisit) {
        writeBranch(branch);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitSwitch(TVisit visit, TIntermSwitch* switchNode) {
    if (visit == EvPreVisit) {
        writeSwitch(switchNode);
        return false;
    }
    return true;
}

bool GLSLSkSLCodeGenerator::visitVariableDecl(TVisit visit, TIntermVariableDecl* decl) {
    if (visit == EvPreVisit) {
        writeVariableDecl(decl);
        return false;
    }
    return true;
}

// ============================================================================
// Main code generation entry point
// ============================================================================
const char* StorageQualifierToString(TStorageQualifier q) {
    switch (q) {
        case EvqTemporary: return "EvqTemporary";
        case EvqGlobal: return "EvqGlobal";
        case EvqConst: return "EvqConst";
        case EvqVaryingIn: return "EvqVaryingIn";
        case EvqVaryingOut: return "EvqVaryingOut";
        case EvqUniform: return "EvqUniform";
        case EvqBuffer: return "EvqBuffer";
        case EvqShared: return "EvqShared";
        case EvqSpirvStorageClass: return "EvqSpirvStorageClass";
        case EvqPayload: return "EvqPayload";
        case EvqPayloadIn: return "EvqPayloadIn";
        case EvqHitAttr: return "EvqHitAttr";
        case EvqCallableData: return "EvqCallableData";
        case EvqCallableDataIn: return "EvqCallableDataIn";
        case EvqHitObjectAttrNV: return "EvqHitObjectAttrNV";
        case EvqHitObjectAttrEXT: return "EvqHitObjectAttrEXT";
        case EvqtaskPayloadSharedEXT: return "EvqtaskPayloadSharedEXT";
        case EvqIn: return "EvqIn";
        case EvqOut: return "EvqOut";
        case EvqInOut: return "EvqInOut";
        case EvqConstReadOnly: return "EvqConstReadOnly";
        case EvqVertexId: return "EvqVertexId";
        case EvqInstanceId: return "EvqInstanceId";
        case EvqPosition: return "EvqPosition";
        case EvqPointSize: return "EvqPointSize";
        case EvqClipVertex: return "EvqClipVertex";
        case EvqFace: return "EvqFace";
        case EvqFragCoord: return "EvqFragCoord";
        case EvqPointCoord: return "EvqPointCoord";
        case EvqFragColor: return "EvqFragColor";
        case EvqFragDepth: return "EvqFragDepth";
        case EvqFragStencil: return "EvqFragStencil";
        case EvqTileImageEXT: return "EvqTileImageEXT";
        case EvqSamplerHeap: return "EvqSamplerHeap";
        case EvqResourceHeap: return "EvqResourceHeap";
        case EvqLast: return "EvqLast";
    }
    return "Unknown";
}

bool GLSLSkSLCodeGenerator::generateCode(TIntermNode* root, std::string_view /*glslSource*/) {
    if (!root) {
        infoSink.info << "Error: NULL root node\n";
        return false;
    }

    // Write SkSL header
    writeLine("// Generated by GLSL to SkSL Code Generator");
    writeLine("");

    // Write shader type
    switch (stage) {
        case EShLangVertex:
            writeLine("// Vertex Shader");
            break;
        case EShLangFragment:
            writeLine("// Fragment Shader");
            break;
        case EShLangCompute:
            writeLine("// Compute Shader");
            break;
        case EShLangGeometry:
            writeLine("// Geometry Shader");
            break;
        case EShLangTessControl:
            writeLine("// Tessellation Control Shader");
            break;
        case EShLangTessEvaluation:
            writeLine("// Tessellation Evaluation Shader");
            break;
        case EShLangRayGen:
            writeLine("// Ray Generation Shader");
            break;
        case EShLangIntersect:
            writeLine("// Intersection Shader");
            break;
        case EShLangAnyHit:
            writeLine("// Any Hit Shader");
            break;
        case EShLangClosestHit:
            writeLine("// Closest Hit Shader");
            break;
        case EShLangMiss:
            writeLine("// Miss Shader");
            break;
        case EShLangCallable:
            writeLine("// Callable Shader");
            break;
        case EShLangMesh:
            writeLine("// Mesh Shader");
            break;
        case EShLangTask:
            writeLine("// Task Shader");
            break;
        default:
            writeLine("// Unknown Shader Type");
            break;
    }
    writeLine("");

    // Process the AST
    // For a proper implementation, we would traverse and output:
    // 1. Global declarations (uniforms, inputs, outputs)
    // 2. Struct definitions
    // 3. Function declarations
    // 4. Function definitions

    // Pre-pass: collect every symbol referenced inside function bodies so
    // we can omit unused uniform/varying declarations from the output.
    collectUsedSymbolNames(root);

    if (auto* aggregate = root->getAsAggregate()) {
        for (auto* child : aggregate->getSequence()) {
            if (auto* agg = child->getAsAggregate()) {
                TOperator op = agg->getOp();
                switch (op) {
                    case EOpLinkerObjects:
                        // Global declarations (uniforms, inputs, outputs)
                        for (auto* obj : agg->getSequence()) {
                            if (auto* objAgg = obj->getAsAggregate()) {
                                writeGlobalDeclaration(objAgg);
                            } else if (auto* symbol = obj->getAsSymbolNode()) {
                                globalNames.insert(symbol->getName().c_str());
                                const TType& varType = symbol->getType();
                                const TQualifier& qualifier = varType.getQualifier();
                                if(qualifier.storage == EvqVaryingOut){
                                    this->fragColorParamName = symbol->getName().c_str();
                                    // In fragment shaders, the out variable is handled by the
                                    // entry-point transform (half4 main). Suppress the global.
                                    if (stage == EShLangFragment) {
                                        continue;
                                    }
                                }
                                // Drop unused uniforms / varyings.
                                std::string symName2 = symbol->getName().c_str();
                                if ((qualifier.storage == EvqUniform ||
                                     qualifier.storage == EvqVaryingIn ||
                                     qualifier.storage == EvqVaryingOut) &&
                                    varType.getBasicType() != EbtBlock &&
                                    usedSymbolNamesComputed &&
                                    usedSymbolNames.find(symName2) == usedSymbolNames.end() &&
                                    symName2 != "iChannelResolution") {
                                    continue;
                                }

                                // Detect resolution uniform: a float2 uniform whose name
                                // suggests it carries the rendering resolution. Used later
                                // to adjust texture() → .eval() coordinate spaces.
                                if (qualifier.storage == EvqUniform &&
                                    varType.isVector() &&
                                    (varType.getVectorSize() == 2 || varType.getVectorSize() == 3) &&
                                    varType.getBasicType() == EbtFloat) {
                                    std::string name(symbol->getName().c_str());
                                    std::string lower = name;
                                    for (auto& c : lower) c = char(tolower((unsigned char)c));
                                    if (lower.find("res") != std::string::npos) {
                                        if (resolutionVarName.empty() || name == "iResolution")
                                            resolutionVarName = name;
                                    } else if (resolutionVarName.empty()) {
                                        resolutionVarName = name;
                                    }
                                }

                                // Handle uniform blocks — expand members as individual uniforms
                                // without the block instance name prefix
                                if (varType.getBasicType() == EbtBlock && qualifier.storage == EvqUniform) {
                                    const TTypeList* members = varType.getStruct();
                                    if (members) {
                                        for (size_t i = 0; i < members->size(); ++i) {
                                            const TType* memberType = (*members)[i].type;
                                            // Detect resolution uniform among block members
                                            if (memberType->isVector() && (memberType->getVectorSize() == 2 || memberType->getVectorSize() == 3) &&
                                                memberType->getBasicType() == EbtFloat) {
                                                std::string mname(memberType->getFieldName().c_str());
                                                std::string mlower = mname;
                                                for (auto& c : mlower) c = char(tolower((unsigned char)c));
                                                if (mlower.find("res") != std::string::npos) {
                                                    resolutionVarName = mname;
                                                } else if (resolutionVarName.empty()) {
                                                    resolutionVarName = mname;
                                                }
                                            }
                                            writeIndent();
                                            write("uniform ");
                                            write(getTypeName(*memberType));
                                            write(" ");
                                            write(memberType->getFieldName().c_str());
                                            writeLine(";");
                                        }
                                    }
                                    continue;
                                }

                                writeIndent();
                                // Layout qualifiers (skip for const globals)
                                if (qualifier.storage != EvqConst) {
                                    std::string layoutStr = getLayoutString(qualifier);
                                    if (!layoutStr.empty()) {
                                        write(layoutStr);
                                    }
                                }
                                // Storage qualifiers (uniform, in, out, const)
                                write(getQualifierString(qualifier, true));
                                // Type
                                write(getTypeName(varType));
                                write(" ");
                                // Name
                                write(symbol->getName().c_str());
                                // Array dimensions
                                if (varType.isArray()) {
                                    write("[");
                                    if (varType.getArraySizes()) {
                                        for (int d = 0; d < varType.getArraySizes()->getNumDims(); ++d) {
                                            if (d > 0) write("][");
                                            int sz = varType.getArraySizes()->getDimSize(d);
                                            if (sz > 0) write(std::to_string(sz));
                                        }
                                    }
                                    write("]");
                                }
                                // Const initializer
                                if (qualifier.storage == EvqConst) {
                                    const TConstUnionArray& ca = symbol->getConstArray();
                                    if (ca.size() > 0) {
                                        write(" = ");
                                        if (varType.isVector()) {
                                            write(getTypeName(varType));
                                            write("(");
                                            for (int i = 0; i < varType.getVectorSize(); ++i) {
                                                if (i > 0) write(", ");
                                                switch (ca[i].getType()) {
                                                    case EbtInt:    write(std::to_string(ca[i].getIConst())); break;
                                                    case EbtUint:   write(std::to_string(ca[i].getUConst()) + "u"); break;
                                                    case EbtFloat:
                                                    case EbtDouble: write(formatFloat(ca[i].getDConst())); break;
                                                    case EbtBool:   write(ca[i].getBConst() ? "true" : "false"); break;
                                                    default:        write("0"); break;
                                                }
                                            }
                                            write(")");
                                        } else {
                                            switch (ca[0].getType()) {
                                                case EbtInt:    write(std::to_string(ca[0].getIConst())); break;
                                                case EbtUint:   write(std::to_string(ca[0].getUConst()) + "u"); break;
                                                case EbtFloat:
                                                case EbtDouble: write(formatFloat(ca[0].getDConst())); break;
                                                case EbtBool:   write(ca[0].getBConst() ? "true" : "false"); break;
                                                default:        write("0"); break;
                                            }
                                        }
                                    }
                                }
                                writeLine(";");
                            }
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        writeLine("");
        for (auto* child : aggregate->getSequence()) {
            if (auto* agg = child->getAsAggregate()) {
                TOperator op = agg->getOp();
                switch (op) {
                    case EOpFunction:
                        writeFunction(agg);
                        break;

                    case EOpSequence:
                    case EOpScope:
                        // General sequence
                        writeStatement(agg);
                        break;
                    case EOpLinkerObjects:
                        // Already handled global declarations in the first pass; skip
                        break;  
                    default:
                        // Skip unnamed/empty aggregates (glslang internal nodes)
                        {
                            const TString& aggName = agg->getName();
                            if (aggName.size() > 0) {
                                writeIndent();
                                write("// TODO: Unhandled root aggregate: ");
                                writeLine(aggName.c_str());
                            }
                        }
                        break;
                }
            }
        }
    } else {
        // Single node traversal
        root->traverse(this);
    }

    return true;
}

//
// External interface function
//
bool OutputSkSL(TIntermNode* root, EShLanguage stage, TInfoSink& infoSink,
                const binding::ProvenanceConfig* inConfig,
                binding::ProvenanceConfig* outConfig) {
    GLSLSkSLCodeGenerator generator(infoSink, stage);
    if (inConfig) {
        generator.setProvenanceConfig(inConfig);
    }
    bool ok = generator.generateCode(root);
    if (outConfig) {
        *outConfig = generator.getReverseProvenance();

        // Populate metadata on the reverse provenance for bidirectional round-trip support.
        // If input provenance has metadata, carry it forward (it describes the original SkSL context).
        // Otherwise, populate metadata from the GLSL source and conversion defaults.
        binding::ProvenanceMetadata meta;
        if (inConfig && inConfig->hasMetadata()) {
            meta = inConfig->metadata();
        } else {
            // Infer context from the GLSL source stage
            switch (stage) {
                case EShLangVertex:  meta.source_program_kind = "kVertex";  break;
                case EShLangFragment: meta.source_program_kind = "kFragment"; break;
                case EShLangCompute:  meta.source_program_kind = "kCompute";  break;
                default:             meta.source_program_kind = "kFragment"; break;
            }
            meta.glsl_dialect = "kOpenGLCore";
            meta.glsl_version = 450;
            meta.runtime_effect_mode = false;
            meta.use_rt_flip = false;
            meta.use_fragcoord_workaround = false;
        }
        // shader_stage always reflects the GLSL source stage
        switch (stage) {
            case EShLangVertex:     meta.shader_stage = "kVertex";   break;
            case EShLangFragment:   meta.shader_stage = "kFragment"; break;
            case EShLangCompute:    meta.shader_stage = "kCompute";  break;
            default: break;
        }
        outConfig->setMetadata(meta);
    }
    return ok;
}

} // end namespace glslang
