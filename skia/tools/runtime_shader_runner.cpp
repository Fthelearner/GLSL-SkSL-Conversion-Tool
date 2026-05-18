#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/encode/SkPngEncoder.h"

#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct ChildSpec {
    std::string name;
    std::string path;
    bool raw = false;
};

struct UniformSpec {
    std::string name;
    std::string value;
};

struct Config {
    std::string shaderPath;
    std::string outputPath;
    std::optional<int> width;
    std::optional<int> height;
    std::vector<ChildSpec> children;
    std::vector<UniformSpec> uniforms;
    bool dumpInterface = false;
};

void show_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s --shader file.sksl --output out.png [options]\n"
                 "\n"
                 "Options:\n"
                 "  --size WIDTHxHEIGHT        Override render size. Defaults to first child image size.\n"
                 "  --child name=path         Bind an image child via makeShader(). Repeatable.\n"
                 "  --raw-child name=path     Bind an image child via makeRawShader(). Repeatable.\n"
                 "  --uniform name=v[,v...]   Bind a uniform. Repeatable.\n"
                 "                            Special value '@size' expands to WIDTH,HEIGHT.\n"
                 "  --dump-interface          Print reflected uniforms and children before rendering.\n"
                 "  --help                    Show this message.\n",
                 argv0);
}

std::optional<std::pair<std::string, std::string>> split_assignment(const std::string& text) {
    size_t pos = text.find('=');
    if (pos == std::string::npos || pos == 0 || pos == text.size() - 1) {
        return std::nullopt;
    }
    return std::make_pair(text.substr(0, pos), text.substr(pos + 1));
}

std::optional<std::pair<int, int>> parse_size(std::string_view text) {
    size_t pos = text.find('x');
    if (pos == std::string::npos) {
        pos = text.find('X');
    }
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    int width = 0;
    int height = 0;
    auto widthPart = text.substr(0, pos);
    auto heightPart = text.substr(pos + 1);
    auto [wptr, werr] = std::from_chars(widthPart.data(), widthPart.data() + widthPart.size(), width);
    auto [hptr, herr] = std::from_chars(heightPart.data(), heightPart.data() + heightPart.size(),
                                        height);
    if (werr != std::errc() || herr != std::errc() || wptr != widthPart.data() + widthPart.size() ||
        hptr != heightPart.data() + heightPart.size() || width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return std::make_pair(width, height);
}

std::optional<Config> parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const char* option) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", option);
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--shader") {
            const char* value = require_value("--shader");
            if (!value) {
                return std::nullopt;
            }
            config.shaderPath = value;
        } else if (arg == "--output") {
            const char* value = require_value("--output");
            if (!value) {
                return std::nullopt;
            }
            config.outputPath = value;
        } else if (arg == "--size") {
            const char* value = require_value("--size");
            if (!value) {
                return std::nullopt;
            }
            std::optional<std::pair<int, int>> size = parse_size(value);
            if (!size) {
                std::fprintf(stderr, "invalid --size '%s'\n", value);
                return std::nullopt;
            }
            config.width = size->first;
            config.height = size->second;
        } else if (arg == "--child" || arg == "--raw-child") {
            const char* value = require_value(arg.c_str());
            if (!value) {
                return std::nullopt;
            }
            std::optional<std::pair<std::string, std::string>> spec = split_assignment(value);
            if (!spec) {
                std::fprintf(stderr, "invalid child spec '%s'\n", value);
                return std::nullopt;
            }
            ChildSpec childSpec;
            childSpec.name = std::move(spec->first);
            childSpec.path = std::move(spec->second);
            childSpec.raw = arg == "--raw-child";
            config.children.push_back(std::move(childSpec));
        } else if (arg == "--uniform") {
            const char* value = require_value("--uniform");
            if (!value) {
                return std::nullopt;
            }
            std::optional<std::pair<std::string, std::string>> spec = split_assignment(value);
            if (!spec) {
                std::fprintf(stderr, "invalid uniform spec '%s'\n", value);
                return std::nullopt;
            }
            UniformSpec uniformSpec;
            uniformSpec.name = std::move(spec->first);
            uniformSpec.value = std::move(spec->second);
            config.uniforms.push_back(std::move(uniformSpec));
        } else if (arg == "--dump-interface") {
            config.dumpInterface = true;
        } else if (arg == "--help" || arg == "-h") {
            show_usage(argv[0]);
            return std::nullopt;
        } else {
            std::fprintf(stderr, "unknown argument '%s'\n", arg.c_str());
            return std::nullopt;
        }
    }

    if (config.shaderPath.empty() || config.outputPath.empty()) {
        show_usage(argv[0]);
        return std::nullopt;
    }
    return config;
}

std::optional<std::string> read_text_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        std::fprintf(stderr, "failed to open shader source: %s\n", path.c_str());
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

sk_sp<SkImage> load_image(const std::string& path) {
    sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
    if (!data) {
        std::fprintf(stderr, "failed to read image: %s\n", path.c_str());
        return nullptr;
    }
    sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(std::move(data));
    if (!image) {
        std::fprintf(stderr, "failed to decode image: %s\n", path.c_str());
        return nullptr;
    }
    return image;
}

const char* uniform_type_name(SkRuntimeEffect::Uniform::Type type) {
    using Type = SkRuntimeEffect::Uniform::Type;
    switch (type) {
        case Type::kFloat:
            return "float";
        case Type::kFloat2:
            return "float2";
        case Type::kFloat3:
            return "float3";
        case Type::kFloat4:
            return "float4";
        case Type::kFloat2x2:
            return "float2x2";
        case Type::kFloat3x3:
            return "float3x3";
        case Type::kFloat4x4:
            return "float4x4";
        case Type::kInt:
            return "int";
        case Type::kInt2:
            return "int2";
        case Type::kInt3:
            return "int3";
        case Type::kInt4:
            return "int4";
    }
    return "<unknown>";
}

int components_per_element(SkRuntimeEffect::Uniform::Type type) {
    using Type = SkRuntimeEffect::Uniform::Type;
    switch (type) {
        case Type::kFloat:
        case Type::kInt:
            return 1;
        case Type::kFloat2:
        case Type::kInt2:
            return 2;
        case Type::kFloat3:
        case Type::kInt3:
            return 3;
        case Type::kFloat4:
        case Type::kInt4:
        case Type::kFloat2x2:
            return 4;
        case Type::kFloat3x3:
            return 9;
        case Type::kFloat4x4:
            return 16;
    }
    return 0;
}

bool uniform_uses_float_storage(SkRuntimeEffect::Uniform::Type type) {
    using Type = SkRuntimeEffect::Uniform::Type;
    switch (type) {
        case Type::kFloat:
        case Type::kFloat2:
        case Type::kFloat3:
        case Type::kFloat4:
        case Type::kFloat2x2:
        case Type::kFloat3x3:
        case Type::kFloat4x4:
            return true;
        case Type::kInt:
        case Type::kInt2:
        case Type::kInt3:
        case Type::kInt4:
            return false;
    }
    return false;
}

std::vector<std::string_view> split_csv(std::string_view text) {
    std::vector<std::string_view> result;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(',', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        result.push_back(text.substr(start, end - start));
        start = end + 1;
        if (end == text.size()) {
            break;
        }
    }
    return result;
}

template <typename T>
std::optional<std::vector<T>> parse_numeric_list(std::string_view text) {
    std::vector<std::string_view> parts = split_csv(text);
    std::vector<T> values;
    values.reserve(parts.size());
    for (std::string_view part : parts) {
        if (part.empty()) {
            return std::nullopt;
        }

        std::string owned(part);
        char* end = nullptr;
        errno = 0;
        if constexpr (std::is_same_v<T, float>) {
            float value = std::strtof(owned.c_str(), &end);
            if (errno != 0 || !end || *end != '\0') {
                return std::nullopt;
            }
            values.push_back(value);
        } else {
            long value = std::strtol(owned.c_str(), &end, 10);
            if (errno != 0 || !end || *end != '\0') {
                return std::nullopt;
            }
            values.push_back(static_cast<T>(value));
        }
    }
    return values;
}

void dump_interface(const SkRuntimeEffect& effect) {
    std::printf("uniforms:\n");
    for (const SkRuntimeEffect::Uniform& uniform : effect.uniforms()) {
        std::printf("  %.*s: %s",
                    static_cast<int>(uniform.name.size()),
                    uniform.name.data(),
                    uniform_type_name(uniform.type));
        if (uniform.isArray()) {
            std::printf("[%d]", uniform.count);
        }
        std::printf(" (bytes=%zu)\n", uniform.sizeInBytes());
    }

    std::printf("children:\n");
    for (const SkRuntimeEffect::Child& child : effect.children()) {
        const char* type = "<unknown>";
        switch (child.type) {
            case SkRuntimeEffect::ChildType::kShader:
                type = "shader";
                break;
            case SkRuntimeEffect::ChildType::kColorFilter:
                type = "colorFilter";
                break;
            case SkRuntimeEffect::ChildType::kBlender:
                type = "blender";
                break;
        }
        std::printf("  %.*s: %s\n",
                    static_cast<int>(child.name.size()),
                    child.name.data(),
                    type);
    }
}

bool bind_uniform(SkRuntimeEffectBuilder* builder,
                  const SkRuntimeEffect::Uniform& uniform,
                  std::string_view value,
                  int width,
                  int height) {
    const int expectedValues = components_per_element(uniform.type) * uniform.count;
    if (expectedValues <= 0) {
        std::fprintf(stderr, "unsupported uniform type for '%.*s'\n",
                     static_cast<int>(uniform.name.size()),
                     uniform.name.data());
        return false;
    }

    if (value == "@size") {
        if (expectedValues != 2) {
            std::fprintf(stderr,
                         "uniform '%.*s' expects %d values, but @size expands to 2 values\n",
                         static_cast<int>(uniform.name.size()),
                         uniform.name.data(),
                         expectedValues);
            return false;
        }
        if (uniform_uses_float_storage(uniform.type)) {
            float values[2] = {static_cast<float>(width), static_cast<float>(height)};
            return builder->uniform(uniform.name).set(values, 2);
        }
        int values[2] = {width, height};
        return builder->uniform(uniform.name).set(values, 2);
    }

    if (uniform_uses_float_storage(uniform.type)) {
        std::optional<std::vector<float>> values = parse_numeric_list<float>(value);
        if (!values || static_cast<int>(values->size()) != expectedValues) {
            std::fprintf(stderr,
                         "uniform '%.*s' expects %d float value(s), got '%.*s'\n",
                         static_cast<int>(uniform.name.size()),
                         uniform.name.data(),
                         expectedValues,
                         static_cast<int>(value.size()),
                         value.data());
            return false;
        }
        return builder->uniform(uniform.name).set(values->data(), expectedValues);
    }

    std::optional<std::vector<int>> values = parse_numeric_list<int>(value);
    if (!values || static_cast<int>(values->size()) != expectedValues) {
        std::fprintf(stderr,
                     "uniform '%.*s' expects %d int value(s), got '%.*s'\n",
                     static_cast<int>(uniform.name.size()),
                     uniform.name.data(),
                     expectedValues,
                     static_cast<int>(value.size()),
                     value.data());
        return false;
    }
    return builder->uniform(uniform.name).set(values->data(), expectedValues);
}

bool ensure_parent_directory(const std::string& outputPath) {
    std::filesystem::path path(outputPath);
    std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        std::fprintf(stderr, "failed to create output directory '%s': %s\n",
                     parent.string().c_str(),
                     ec.message().c_str());
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::optional<Config> configOpt = parse_args(argc, argv);
    if (!configOpt) {
        return 1;
    }
    const Config& config = *configOpt;

    std::optional<std::string> shaderText = read_text_file(config.shaderPath);
    if (!shaderText) {
        return 1;
    }

    auto [effect, errorText] = SkRuntimeEffect::MakeForShader(SkString(shaderText->c_str()));
    if (!effect) {
        std::fprintf(stderr, "failed to compile runtime shader '%s'\n%s\n",
                     config.shaderPath.c_str(),
                     errorText.c_str());
        return 1;
    }

    if (config.dumpInterface) {
        dump_interface(*effect);
    }

    struct PreparedChild {
        std::string name;
        sk_sp<SkImage> image;
        bool raw = false;
    };
    std::vector<PreparedChild> preparedChildren;
    preparedChildren.reserve(config.children.size());

    std::optional<int> width = config.width;
    std::optional<int> height = config.height;
    for (const ChildSpec& childSpec : config.children) {
        sk_sp<SkImage> image = load_image(childSpec.path);
        if (!image) {
            return 1;
        }
        if (!width) {
            width = image->width();
        }
        if (!height) {
            height = image->height();
        }
        PreparedChild preparedChild;
        preparedChild.name = childSpec.name;
        preparedChild.image = std::move(image);
        preparedChild.raw = childSpec.raw;
        preparedChildren.push_back(std::move(preparedChild));
    }

    if (!width) {
        width = 1280;
    }
    if (!height) {
        height = 720;
    }

    SkRuntimeShaderBuilder builder(effect);
    for (const PreparedChild& child : preparedChildren) {
        if (child.raw) {
            builder.child(child.name) =
                    child.image->makeRawShader(SkTileMode::kClamp,
                                               SkTileMode::kClamp,
                                               SkSamplingOptions(),
                                               nullptr);
        } else {
            builder.child(child.name) =
                    child.image->makeShader(SkTileMode::kClamp,
                                            SkTileMode::kClamp,
                                            SkSamplingOptions(),
                                            nullptr);
        }
    }

    for (const UniformSpec& uniformSpec : config.uniforms) {
        const SkRuntimeEffect::Uniform* uniform = effect->findUniform(uniformSpec.name);
        if (!uniform) {
            std::fprintf(stderr, "unknown uniform '%s'\n", uniformSpec.name.c_str());
            return 1;
        }
        if (!bind_uniform(&builder, *uniform, uniformSpec.value, *width, *height)) {
            return 1;
        }
    }

    sk_sp<SkShader> shader = builder.makeShader();
    if (!shader) {
        std::fprintf(stderr, "failed to build shader instance for '%s'\n", config.shaderPath.c_str());
        return 1;
    }

    sk_sp<SkSurface> surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(*width, *height));
    if (!surface) {
        std::fprintf(stderr, "failed to create raster surface (%dx%d)\n", *width, *height);
        return 1;
    }

    SkPaint paint;
    paint.setShader(std::move(shader));
    surface->getCanvas()->drawRect(SkRect::MakeWH(*width, *height), paint);

    SkPixmap pixmap;
    if (!surface->peekPixels(&pixmap)) {
        std::fprintf(stderr, "failed to read back raster pixels\n");
        return 1;
    }

    if (!ensure_parent_directory(config.outputPath)) {
        return 1;
    }
    SkFILEWStream stream(config.outputPath.c_str());
    if (!stream.isValid()) {
        std::fprintf(stderr, "failed to open output file '%s'\n", config.outputPath.c_str());
        return 1;
    }
    if (!SkPngEncoder::Encode(&stream, pixmap, {})) {
        std::fprintf(stderr, "failed to encode png '%s'\n", config.outputPath.c_str());
        return 1;
    }

    std::printf("rendered %s -> %s (%dx%d)\n",
                config.shaderPath.c_str(),
                config.outputPath.c_str(),
                *width,
                *height);
    return 0;
}
