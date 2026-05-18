//
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
// GLSL to SkSL demo
// Reads GLSL fragment shaders, converts them to SkSL using GLSLSkSLCodeGenerator
//
// NOTE on glslang namespace conventions:
// - EShLanguage (EShLangFragment, etc.), EShMessages (EShMsgDefault, etc.), EProfile (ENoProfile)
//   are C enums at GLOBAL scope (before namespace glslang in ShaderLang.h / Versions.h)
// - TIntermNode is at GLOBAL scope (intermediate.h comment: "outside the glslang namespace")
// - TInfoSink is at GLOBAL scope (InfoSink.h: after namespace glslang)
// - GetDefaultResources() is at GLOBAL scope (Public/ResourceLimits.h)
// - EShSource*, EShClient*, EShTarget* etc. are inside namespace glslang (ShaderLang.h line 129-222)
// - TShader, TProgram, TIntermediate, InitializeProcess(), FinalizeProcess()
//   are inside namespace glslang (ShaderLang.h line 404+)
//

#include "glslang/Include/intermediate.h"
#include "glslang/Include/InfoSink.h"
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/GenericCodeGen/GLSLSkSLCodeGenerator.h"
#include "binding_registry/provenance.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

// Forward declarations
char* ReadFileData(const char* fileName);
void FreeFileData(char* data);
std::string GetFileBaseName(const std::string& path);
void EnsureDirectory(const std::string& path);

int main(int argc, char* argv[]) {
    // Determine input directory and output directory
    std::string inputDir = "glslang_demo";
    std::string outputDir = "glslang_demo/result/sksl";

    if (argc > 1) {
        inputDir = argv[1];
    }
    if (argc > 2) {
        outputDir = argv[2];
    }

    // Initialize glslang
    if (!glslang::InitializeProcess()) {
        fprintf(stderr, "ERROR: Failed to initialize glslang process\n");
        return 1;
    }

    // Ensure output directory exists
    EnsureDirectory(outputDir);

    // Open input directory
    DIR* dir = opendir(inputDir.c_str());
    if (!dir) {
        fprintf(stderr, "ERROR: Cannot open input directory '%s'\n", inputDir.c_str());
        glslang::FinalizeProcess();
        return 1;
    }

    // Get default resources (global scope function, not in glslang:: namespace)
    const TBuiltInResource* resources = GetDefaultResources();

    // Process all .frag files
    struct dirent* entry;
    std::vector<std::string> shaderFiles;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".frag") {
            shaderFiles.push_back(inputDir + "/" + name);
        }
    }
    closedir(dir);

    if (shaderFiles.empty()) {
        fprintf(stderr, "WARNING: No .frag files found in '%s'\n", inputDir.c_str());
    }

    int successCount = 0;
    int failureCount = 0;

    for (const auto& filePath : shaderFiles) {
        // Read shader source
        char* shaderSource = ReadFileData(filePath.c_str());
        if (!shaderSource) {
            fprintf(stderr, "ERROR: Cannot read shader file '%s'\n", filePath.c_str());
            failureCount++;
            continue;
        }

        // Create shader (EShLangFragment is global scope enum)
        glslang::TShader shader(EShLangFragment);

        // Set environment: EShSourceGlsl etc are inside glslang:: namespace
        shader.setEnvInput(glslang::EShSourceGlsl, EShLangFragment,
                           glslang::EShClientNone, 450);

        // For non-SPIR-V mode (just GLSL→SkSL), skip setEnvClient/setEnvTarget
        // as StandAlone.cpp only does these when EOptionSpv is set.

        // Set strings and parse
        const char* strings[1] = { shaderSource };
        shader.setStrings(strings, 1);

        // Parse: ENoProfile (Versions.h), EShMsgDefault (ShaderLang.h) are global scope enums
        bool parseResult = shader.parse(resources, 450, ENoProfile, false, false,
                                        EShMsgDefault);

        if (!parseResult) {
            fprintf(stderr, "ERROR: Failed to parse '%s'\n", filePath.c_str());
            fprintf(stderr, "  Info log: %s\n", shader.getInfoLog());
            fprintf(stderr, "  Debug log: %s\n", shader.getInfoDebugLog());
            FreeFileData(shaderSource);
            failureCount++;
            continue;
        }

        // Get intermediate representation (TIntermediate is in glslang:: namespace)
        glslang::TIntermediate* intermediate = shader.getIntermediate();
        if (!intermediate) {
            fprintf(stderr, "ERROR: No intermediate representation for '%s'\n", filePath.c_str());
            FreeFileData(shaderSource);
            failureCount++;
            continue;
        }

        // Get the AST root node.
        // TIntermNode is at GLOBAL scope (intermediate.h comment: outside the glslang namespace)
        TIntermNode* root = intermediate->getTreeRoot();
        if (!root) {
            fprintf(stderr, "ERROR: No AST root for '%s'\n", filePath.c_str());
            FreeFileData(shaderSource);
            failureCount++;
            continue;
        }

        // Generate SkSL.
        // TInfoSink is at GLOBAL scope (InfoSink.h: after namespace glslang)
        TInfoSink infoSink;
        // Try to load sideband provenance config if it exists
        sksl_glsl_binding::ProvenanceConfig provConfig;
        std::string provPath = filePath + ".provenance";
        {
            char* provData = ReadFileData(provPath.c_str());
            if (provData) {
                provConfig.fromJson(provData);
                FreeFileData(provData);
            }
        }

        // Collect reverse provenance entries recorded during GLSL→SKSL translation
        sksl_glsl_binding::ProvenanceConfig reverseProv;

        bool skslResult = glslang::OutputSkSL(root, EShLangFragment, infoSink,
                                               provConfig.empty() ? nullptr : &provConfig,
                                               &reverseProv);

        // Write output
        std::string baseName = GetFileBaseName(filePath);
        std::string outputPath = outputDir + "/" + baseName + ".sksl";

        // Write reverse provenance config for bidirectional symmetry
        if (!reverseProv.empty()) {
            std::string provOutputPath = outputPath + ".provenance";
            FILE* provFile = fopen(provOutputPath.c_str(), "w");
            if (provFile) {
                std::string json = reverseProv.toJson();
                fprintf(provFile, "%s", json.c_str());
                fclose(provFile);
            }
        }

        FILE* outFile = fopen(outputPath.c_str(), "w");
        if (!outFile) {
            fprintf(stderr, "ERROR: Cannot write output file '%s'\n", outputPath.c_str());
            FreeFileData(shaderSource);
            failureCount++;
            continue;
        }

        // Write the SkSL output
        const char* skslStr = infoSink.info.c_str();
        // Also try debug sink if info is empty
        if (strlen(skslStr) == 0) {
            skslStr = infoSink.debug.c_str();
        }

        fprintf(outFile, "// Generated from: %s\n", filePath.c_str());
        if (skslResult) {
            fprintf(outFile, "// OutputSkSL status: SUCCESS\n\n");
        } else {
            fprintf(outFile, "// OutputSkSL status: FAILED (generator returned false)\n\n");
        }

        if (strlen(skslStr) > 0) {
            fprintf(outFile, "%s\n", skslStr);
        } else {
            fprintf(outFile, "// No SkSL output generated\n");
        }

        fclose(outFile);
        printf("[OK] %s -> %s\n", filePath.c_str(), outputPath.c_str());

        // Also write debug info if available
        std::string debugOutputPath = outputDir + "/" + baseName + ".debug";
        FILE* debugFile = fopen(debugOutputPath.c_str(), "w");
        if (debugFile) {
            fprintf(debugFile, "// Debug info for %s\n", baseName.c_str());
            fprintf(debugFile, "// Info log: %s\n", shader.getInfoLog());
            fprintf(debugFile, "// Debug log: %s\n", shader.getInfoDebugLog());
            if (strlen(infoSink.debug.c_str()) > 0) {
                fprintf(debugFile, "// Debug sink:\n%s\n", infoSink.debug.c_str());
            }
            fclose(debugFile);
        }

        FreeFileData(shaderSource);
        successCount++;
    }

    printf("\nSummary: %d succeeded, %d failed, %d total\n",
           successCount, failureCount, (int)(successCount + failureCount));

    glslang::FinalizeProcess();
    return failureCount > 0 ? 1 : 0;
}

char* ReadFileData(const char* fileName) {
    FILE* in = fopen(fileName, "rb");
    if (in == nullptr) {
        return nullptr;
    }

    // Get file size
    fseek(in, 0, SEEK_END);
    long count = ftell(in);
    fseek(in, 0, SEEK_SET);

    // Handle BOM
    if (count > 3) {
        unsigned char head[3];
        if (fread(head, 1, 3, in) == 3) {
            if (head[0] == 0xef && head[1] == 0xbb && head[2] == 0xbf) {
                count -= 3;
            } else {
                fseek(in, 0, SEEK_SET);
            }
        } else {
            fclose(in);
            return nullptr;
        }
    }

    char* return_data = (char*)malloc(count + 1);
    if ((int)fread(return_data, 1, count, in) != count) {
        free(return_data);
        fclose(in);
        return nullptr;
    }

    return_data[count] = '\0';
    fclose(in);
    return return_data;
}

void FreeFileData(char* data) {
    free(data);
}

std::string GetFileBaseName(const std::string& path) {
    // Extract filename without extension
    size_t lastSlash = path.find_last_of("/\\");
    size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t lastDot = path.rfind('.');
    size_t end = (lastDot == std::string::npos || lastDot < lastSlash) ? path.size() : lastDot;
    return path.substr(start, end - start);
}

void EnsureDirectory(const std::string& path) {
    // Create directory (handle parent directories)
    size_t pos = 0;
    while ((pos = path.find_first_of("/\\", pos + 1)) != std::string::npos) {
        std::string subdir = path.substr(0, pos);
        mkdir(subdir.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}
