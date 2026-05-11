#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLFileOutputStream.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/codegen/SkSLCodeGenTypes.h"
#include "src/sksl/codegen/SkSLGLSLCodeGenerator.h"
#include "src/sksl/ir/SkSLProgram.h"
#include "binding_registry/provenance.h"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace SkOpts {
size_t raster_pipeline_highp_stride = 1;
}

void SkDebugf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

namespace {

class CombinedCapsFactory : public SkSL::ShaderCapsFactory {
public:
    static std::unique_ptr<SkSL::ShaderCaps> MakeCaps() {
        std::unique_ptr<SkSL::ShaderCaps> caps = MakeShaderCaps();

        // The builtin enum tops out at k400, but the emitted version string can still be 450 core.
        caps->fVersionDeclString = "#version 450 core";
        caps->fGLSLGeneration = SkSL::GLSLGeneration::k400;

        // Match the capabilities required by translation/test/comprehensive_coverage.sksl.
        caps->fIntegerSupport = true;
        caps->fNonsquareMatrixSupport = true;
        caps->fFBFetchSupport = true;
        caps->fFBFetchColorName = "FramebufferFragColor";
        caps->fDualSourceBlendingSupport = true;
        caps->fAdvBlendEqInteraction = SkSL::ShaderCaps::kGeneralEnable_AdvBlendEqInteraction;
        return caps;
    }
};

SkSL::ProgramKind detect_program_kind(const std::string& path) {
    auto ends_with = [&](const char* suffix) {
        size_t suffixLen = std::char_traits<char>::length(suffix);
        return path.size() >= suffixLen &&
               path.compare(path.size() - suffixLen, suffixLen, suffix) == 0;
    };

    if (ends_with(".vert")) {
        return SkSL::ProgramKind::kVertex;
    }
    if (ends_with(".frag") || ends_with(".sksl")) {
        return SkSL::ProgramKind::kFragment;
    }
    if (ends_with(".mvert")) {
        return SkSL::ProgramKind::kMeshVertex;
    }
    if (ends_with(".mfrag")) {
        return SkSL::ProgramKind::kMeshFragment;
    }
    if (ends_with(".compute")) {
        return SkSL::ProgramKind::kCompute;
    }
    if (ends_with(".rtb")) {
        return SkSL::ProgramKind::kRuntimeBlender;
    }
    if (ends_with(".rtcf")) {
        return SkSL::ProgramKind::kRuntimeColorFilter;
    }
    if (ends_with(".rts")) {
        return SkSL::ProgramKind::kRuntimeShader;
    }
    if (ends_with(".privrts")) {
        return SkSL::ProgramKind::kPrivateRuntimeShader;
    }

    std::fprintf(stderr,
                 "unsupported input extension: '%s'\n"
                 "expected one of .vert, .frag, .sksl, .mvert, .mfrag, .compute, .rtb, .rtcf, "
                 ".rts, or .privrts\n",
                 path.c_str());
    std::exit(2);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <input> <output.glsl>\n", argv[0]);
        return 2;
    }

    const std::string inputPath = argv[1];
    const char* outputPath = argv[2];

    std::ifstream in(inputPath);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.rdstate()) {
        std::fprintf(stderr, "error reading '%s'\n", inputPath.c_str());
        return 1;
    }

    SkSL::ProgramSettings settings;
    settings.fRTFlipOffset = 16384;
    settings.fRTFlipSet = 0;
    settings.fRTFlipBinding = 0;

    SkSL::Compiler compiler;
    std::unique_ptr<SkSL::Program> program =
            compiler.convertProgram(detect_program_kind(inputPath), text, settings);
    if (!program) {
        std::fprintf(stderr, "%s", compiler.errorText().c_str());
        return 1;
    }

    std::unique_ptr<SkSL::ShaderCaps> caps = CombinedCapsFactory::MakeCaps();
    SkSL::FileOutputStream out(outputPath);
    if (!out.isValid()) {
        std::fprintf(stderr, "error writing '%s'\n", outputPath);
        return 1;
    }

    if (!SkSL::ToGLSL(*program, caps.get(), out, SkSL::PrettyPrint::kYes)) {
        out.close();
        std::fprintf(stderr, "%s", compiler.errorText().c_str());
        return 1;
    }

    // Write provenance sideband file for round-trip fidelity
    const auto& prov = SkSL::GetLastProvenanceConfig();
    if (!prov.empty()) {
        std::string provPath = std::string(outputPath) + ".provenance";
        SkSL::FileOutputStream provOut(provPath.c_str());
        if (provOut.isValid()) {
            std::string json = prov.toJson();
            provOut.writeText(json.c_str());
            provOut.close();
        }
    }

    if (!out.close()) {
        std::fprintf(stderr, "error writing '%s'\n", outputPath);
        return 1;
    }

    return 0;
}
