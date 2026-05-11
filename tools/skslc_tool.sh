#!/usr/bin/env bash
# skslc 编译和 SkSL 文件处理脚本
# 用法见下方或运行: ./skslc_tool.sh --help

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)
SKIA_DIR="$PROJECT_ROOT/skia"
SKSLC_DEFAULT="$SKIA_DIR/out/stog/skslc"

# ============================================================================
# 帮助信息
# ============================================================================
show_help() {
    cat << 'EOF'
skslc_tool.sh - SkSL 编译工具套件

命令:
  build          重新编译 skslc
  compile        编译 SkSL 文件到目标格式
  batch          批量编译目录下的所有 SkSL 文件
  info           显示当前配置信息

用法:
  ./skslc_tool.sh build [BUILD_DIR]
    重新编译 skslc。BUILD_DIR 默认为 out/stog

  ./skslc_tool.sh compile <input> <output> [--skslc PATH]
    编译单个 SkSL 文件
    input:  输入文件路径
    output: 输出文件路径（后缀决定输出格式）

  ./skslc_tool.sh batch [INPUT_DIR] [OUTPUT_DIR] [--format FORMAT]
    批量编译目录下所有支持的文件
    INPUT_DIR:  输入目录，默认为 tests/shaders
    OUTPUT_DIR: 输出目录，默认为 results/skslc
    --format:   输出格式 (glsl, hlsl, metal, spirv, ast, ir)，默认 glsl

支持的输入后缀:
  .vert      - 顶点着色器
  .frag      - 片段着色器
  .sksl      - SkSL 着色器 (等同于 .frag)
  .mvert     - Mesh 顶点着色器
  .mfrag     - Mesh 片段着色器
  .compute   - 计算着色器
  .rtb       - Runtime Blender
  .rtcf      - Runtime Color Filter
  .rts       - Runtime Shader
  .privrts   - Private Runtime Shader
  .fp        - Fragment Processor (需转换为 .sksl 或 .frag)

支持的输出后缀:
  .glsl      - GLSL 源码
  .hlsl      - HLSL 源码
  .metal     - Metal 源码
  .spirv     - SPIR-V 二进制
  .asm.vert  - SPIR-V 汇编 (顶点)
  .asm.frag  - SPIR-V 汇编 (片段)
  .asm.comp  - SPIR-V 汇编 (计算)
  .wgsl      - WGSL 源码
  .ast       - AST 树形输出 (调试用)
  .ir        - IR 描述输出 (调试用)
  .skrp      - Raster Pipeline 输出
  .stage     - Pipeline Stage 输出

示例:
  # 重新编译 skslc
  ./skslc_tool.sh build

  # 编译单个文件为 GLSL
  ./skslc_tool.sh compile shader.sksl output.glsl

  # 编译为 AST 和 GLSL
  ./skslc_tool.sh compile shader.rts debug.ast
  ./skslc_tool.sh compile shader.rts output.glsl

  # 批量编译所有 .sksl 文件
  ./skslc_tool.sh batch

  # 批量编译并输出 SPIR-V 汇编
  ./skslc_tool.sh batch ./my_shaders ./output --format asm.frag

选项:
  --skslc PATH   指定 skslc 路径
  --help, -h     显示此帮助信息

EOF
}

# ============================================================================
# 工具函数
# ============================================================================

find_skslc() {
    local candidates=(
        "$SKSLC_DEFAULT"
        "$SKIA_DIR/out/SkSL/skslc"
        "$SKIA_DIR/out/SkSL/gcc_like_host/skslc"
        "$SKIA_DIR/out/Debug/skslc"
    )

    for candidate in "${candidates[@]}"; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

get_program_kind() {
    local input="$1"
    local base="${input##*/}"

    # 获取文件扩展名（可能有多重扩展名）
    local ext="${input##*.}"

    case "$ext" in
        vert)   echo "vertex" ;;
        frag)   echo "fragment" ;;
        sksl)   echo "runtime_shader" ;;
        mvert)  echo "mesh_vertex" ;;
        mfrag)  echo "mesh_fragment" ;;
        compute) echo "compute" ;;
        rtb)    echo "runtime_blender" ;;
        rtcf)   echo "runtime_color_filter" ;;
        rts)    echo "runtime_shader" ;;
        privrts) echo "private_runtime_shader" ;;
        fp)     echo "fragment_processor" ;;
        *)      echo "unknown" ;;
    esac
}

prepare_input_file() {
    local input="$1"
    local tmp_dir="$2"
    local ext="${input##*.}"
    local base=$(basename "${input%.*}")

    # .fp 和 .sksl 文件在处理 runtime shader 特性时需要转换为 .rts
    # 检测是否包含 runtime shader 特性
    local needs_rts=false
    if [ "$ext" = "fp" ] || [ "$ext" = "sksl" ]; then
        if grep -q "uniform shader\|\.eval(" "$input" 2>/dev/null; then
            needs_rts=true
        fi
    fi

    if [ "$ext" = "fp" ]; then
        local target_ext="rts"
        [ "$needs_rts" = false ] && target_ext="frag"
        local prepared="$tmp_dir/$base.$target_ext"
        cp "$input" "$prepared"
        echo "$prepared"
    elif [ "$ext" = "sksl" ] && [ "$needs_rts" = true ]; then
        # 包含 runtime shader 特性的 .sksl 文件需要转换为 .rts
        local prepared="$tmp_dir/$base.rts"
        cp "$input" "$prepared"
        echo "$prepared"
    else
        echo "$input"
    fi
}

# ============================================================================
# 命令实现
# ============================================================================

cmd_build() {
    local build_dir="${1:-out/stog}"
    local full_build_dir="$SKIA_DIR/$build_dir"

    echo "=== 编译 skslc ==="
    echo "Skia 目录: $SKIA_DIR"
    echo "构建目录: $build_dir"

    # 检查 args.gn 是否存在，如果不存在则创建
    if [ ! -f "$full_build_dir/args.gn" ]; then
        echo "创建构建配置..."
        mkdir -p "$full_build_dir"
        echo "skia_compile_sksl_tests = true" > "$full_build_dir/args.gn"
    fi

    # 生成构建文件
    echo "生成 GN 构建文件..."
    cd "$SKIA_DIR"
    ./bin/gn gen "$build_dir" 2>&1

    # 编译 skslc
    echo "编译 skslc..."
    ninja -C "$build_dir" skslc 2>&1

    local skslc_path="$full_build_dir/skslc"
    if [ ! -x "$skslc_path" ]; then
        echo "✗ 编译失败"
        return 1
    fi

    # 编译并复制 SkSL 模块文件
    echo "复制 SkSL 模块文件..."
    ninja -C "$build_dir" sksl_modules 2>&1 || true

    # 模块文件可能被复制到 gcc_like_host 子目录，需要复制到主目录
    local module_subdir="$full_build_dir/gcc_like_host"
    if [ -d "$module_subdir" ]; then
        for f in "$module_subdir"/*.sksl; do
            [ -f "$f" ] && cp "$f" "$full_build_dir/"
        done
    fi

    echo ""
    echo "✓ 编译成功: $skslc_path"
    ls -lh "$skslc_path"

    # 显示模块文件
    echo ""
    echo "SkSL 模块文件:"
    ls -la "$full_build_dir"/*.sksl 2>/dev/null | head -10 || echo "  (未找到)"
}

cmd_compile() {
    local input=""
    local output=""
    local skslc_path=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --skslc) skslc_path="$2"; shift 2 ;;
            --help|-h) show_help; return 0 ;;
            -*)
                echo "未知选项: $1" >&2
                return 1
                ;;
            *)
                if [ -z "$input" ]; then
                    input="$1"
                elif [ -z "$output" ]; then
                    output="$1"
                fi
                shift
                ;;
        esac
    done

    if [ -z "$input" ] || [ -z "$output" ]; then
        echo "错误: 需要指定输入和输出文件" >&2
        echo "用法: $0 compile <input> <output>" >&2
        return 1
    fi

    # 查找 skslc
    if [ -z "$skslc_path" ]; then
        skslc_path=$(find_skslc) || {
            echo "错误: 找不到 skslc，请先运行: $0 build" >&2
            return 1
        }
    fi

    # 处理输入文件
    local tmp_dir=$(mktemp -d)
    trap "rm -rf '$tmp_dir'" EXIT

    local prepared_input=$(prepare_input_file "$input" "$tmp_dir")
    local kind=$(get_program_kind "$input")

    echo "=== 编译 SkSL ==="
    echo "输入: $input (类型: $kind)"
    echo "输出: $output"
    echo "skslc: $skslc_path"

    # 运行 skslc
    "$skslc_path" "$prepared_input" "$output"
    local result=$?

    if [ $result -eq 0 ] && [ -f "$output" ]; then
        echo ""
        echo "✓ 编译成功"
        echo "输出文件: $output ($(wc -l < "$output") 行)"
    else
        echo "✗ 编译失败 (退出码: $result)"
        [ -f "$output" ] && cat "$output"
        return 1
    fi
}

cmd_batch() {
    local input_dir="$PROJECT_ROOT/tests/shaders"
    local output_dir="$PROJECT_ROOT/results/skslc"
    local format="glsl"
    local extensions=("sksl" "rts" "frag")

    while [ $# -gt 0 ]; do
        case "$1" in
            --format) format="$2"; shift 2 ;;
            --ext) IFS=',' read -ra extensions <<< "$2"; shift 2 ;;
            --help|-h) show_help; return 0 ;;
            -*)
                echo "未知选项: $1" >&2
                return 1
                ;;
            *)
                if [ ! -d "$1" ]; then
                    echo "目录不存在: $1" >&2
                    return 1
                fi
                if [ -z "${input_dir:-}" ] || [ "$input_dir" = "$PROJECT_ROOT/tests/shaders" ]; then
                    input_dir="$1"
                else
                    output_dir="$1"
                fi
                shift
                ;;
        esac
    done

    # 查找 skslc
    local skslc_path=$(find_skslc) || {
        echo "错误: 找不到 skslc，请先运行: $0 build" >&2
        return 1
    }

    mkdir -p "$output_dir"

    echo "=== 批量编译 SkSL ==="
    echo "输入目录: $input_dir"
    echo "输出目录: $output_dir"
    echo "输出格式: $format"
    echo "skslc: $skslc_path"
    echo ""

    local tmp_dir=$(mktemp -d)
    trap "rm -rf '$tmp_dir'" EXIT

    local count=0
    local failed=0

    for ext in "${extensions[@]}"; do
        for shader in "$input_dir"/*."$ext"; do
            [ -f "$shader" ] || continue

            local base=$(basename "${shader%.*}")
            local prepared=$(prepare_input_file "$shader" "$tmp_dir")
            local output="$output_dir/$base.$format"

            printf "编译 %s... " "$base"

            if "$skslc_path" "$prepared" "$output" 2>/dev/null; then
                local lines=$(wc -l < "$output" 2>/dev/null || echo "?")
                echo "✓ ($lines 行)"
                ((count++)) || true
            else
                echo "✗"
                ((failed++)) || true
            fi
        done
    done

    echo ""
    echo "完成: $count 个成功, $failed 个失败"
    echo "输出目录: $output_dir"
}

cmd_info() {
    echo "=== skslc 配置信息 ==="
    echo ""

    echo "项目根目录: $PROJECT_ROOT"
    echo "Skia 目录: $SKIA_DIR"
    echo ""

    echo "skslc 搜索路径:"
    local candidates=(
        "$SKSLC_DEFAULT"
        "$SKIA_DIR/out/SkSL/skslc"
        "$SKIA_DIR/out/SkSL/gcc_like_host/skslc"
        "$SKIA_DIR/out/Debug/skslc"
    )
    for candidate in "${candidates[@]}"; do
        if [ -x "$candidate" ]; then
            echo "  ✓ $candidate"
        else
            echo "  ✗ $candidate (不存在)"
        fi
    done
    echo ""

    local skslc=$(find_skslc 2>/dev/null || echo "未找到")
    echo "当前 skslc: $skslc"

    if [ -x "$skslc" ]; then
        echo ""
        echo "文件信息:"
        ls -lh "$skslc"
    fi
}

# ============================================================================
# 主入口
# ============================================================================

main() {
    local command="${1:-help}"
    shift || true

    case "$command" in
        build)    cmd_build "$@" ;;
        compile)  cmd_compile "$@" ;;
        batch)    cmd_batch "$@" ;;
        info)     cmd_info ;;
        help|--help|-h) show_help ;;
        *)
            echo "未知命令: $command" >&2
            echo "运行 '$0 --help' 查看用法" >&2
            return 1
            ;;
    esac
}

main "$@"
