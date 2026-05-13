#!/usr/bin/env python3
"""Pre-process GLSL source: add default initializers (=0) to variables declared
without an initializer, so glslang preserves them in the AST instead of
stripping them during optimization."""

import re
import sys
from pathlib import Path

SKIP_PATTERNS = [
    r'^\s*uniform\s', r'^\s*in\s', r'^\s*out\s', r'^\s*const\s',
    r'^\s*layout\s', r'^\s*precision\s', r'^\s*flat\s', r'^\s*smooth\s',
    r'^\s*void\s', r'^\s*#', r'^\s*//', r'^\s*/\*',
]

GLSL_TO_ZERO = {
    'float': '0.0', 'int': '0', 'uint': '0u', 'bool': 'false',
    'double': '0.0',
}


def _zero_for_type(typename):
    if typename in GLSL_TO_ZERO:
        return GLSL_TO_ZERO[typename]
    m = re.match(r'(vec|ivec|uvec|bvec|dvec)([234])', typename)
    if m:
        base = {'vec': 'float', 'ivec': 'int', 'uvec': 'uint',
                'bvec': 'bool', 'dvec': 'double'}[m.group(1)]
        return f'{typename}({", ".join([_zero_for_type(base)] * int(m.group(2)))})'
    m = re.match(r'(mat|dmat)([234])(?:x([234]))?', typename)
    if m:
        return f'{typename}({_zero_for_type("float")})'
    return None


def _should_skip(line):
    for pat in SKIP_PATTERNS:
        if re.match(pat, line):
            return True
    return False


def _split_declarators(decl_list):
    parts = []
    depth = 0
    current = []
    for ch in decl_list:
        if ch == '(': depth += 1
        elif ch == ')': depth -= 1
        elif ch == ',' and depth == 0:
            parts.append(''.join(current))
            current = []
        else:
            current.append(ch)
    if current:
        parts.append(''.join(current))
    return parts


def process(source):
    lines = source.split('\n')
    decl_re = re.compile(
        r'^(\s*)'
        r'((?:float|int|uint|bool|double|'
        r'vec[234]|ivec[234]|uvec[234]|bvec[234]|dvec[234]|'
        r'mat[234](?:x[234])?|dmat[234](?:x[234])?))\s+'
        r'(.+?)\s*;\s*$'
    )

    out = []
    brace_depth = 0  # track { } to skip uniform blocks / structs
    for line in lines:
        # Count braces in this line (naive but works for typical GLSL)
        brace_depth += line.count('{') - line.count('}')

        if _should_skip(line) or not line.strip():
            out.append(line)
            continue

        # Skip declarations inside blocks (uniform blocks, structs, interfaces)
        if brace_depth > 0 and not _is_function_header(line):
            out.append(line)
            continue

        m = decl_re.match(line)
        if not m:
            out.append(line)
            continue

        indent = m.group(1)
        typename = m.group(2)
        decl_list = m.group(3)
        zero_val = _zero_for_type(typename)
        if zero_val is None:
            out.append(line)
            continue

        parts = _split_declarators(decl_list)
        new_parts = []
        any_mod = False
        for p in parts:
            p = p.strip()
            has_eq = False
            depth = 0
            for ch in p:
                if ch == '(': depth += 1
                elif ch == ')': depth -= 1
                elif ch == '=' and depth == 0: has_eq = True
            if has_eq:
                new_parts.append(p)
            else:
                var_name = p.split('[')[0].strip()
                array_part = p[len(var_name):] if '[' in p else ''
                new_parts.append(f'{var_name}{array_part} = {zero_val}')
                any_mod = True

        if any_mod:
            out.append(f'{indent}{typename} {", ".join(new_parts)};')
        else:
            out.append(line)

    return '\n'.join(out)


def _is_function_header(line):
    return re.match(r'^\s*\w[\w\d]*\s+\w[\w\d]*\s*\(', line) is not None


def main():
    args = sys.argv[1:]
    if not args:
        sys.stderr.write(f'Usage: {sys.argv[0]} <input.frag> [--output output.frag]\n')
        sys.exit(2)
    input_path = args[0]
    output_path = None
    for i, a in enumerate(args):
        if a == '--output' and i + 1 < len(args):
            output_path = args[i + 1]
    src = Path(input_path).read_text(encoding='utf-8')
    result = process(src)
    if output_path:
        Path(output_path).write_text(result, encoding='utf-8')
    else:
        sys.stdout.write(result)


if __name__ == '__main__':
    main()
