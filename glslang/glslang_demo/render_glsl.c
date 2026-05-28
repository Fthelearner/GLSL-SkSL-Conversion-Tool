//
// Offscreen renderer for fragment shaders — GLX + OpenGL, outputs PPM
// Handles both standard GLSL (void main) and Shadertoy-style (mainImage)
//

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WIN_W 800
#define WIN_H 600
#define MAX_TEXTURES 8
#define MAX_UNIFORMS 32

static const char* fullscreen_vs =
    "#version 330 core\n"
    "const vec2 pos[3] = vec2[3](\n"
    "    vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));\n"
    "void main() {\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

typedef struct {
    char name[64];
    char path[512];
} TexArg;

typedef struct {
    char name[64];
    int   nvalues;
    float values[4];
} UniformArg;

#define MAX_RAWTEX 8
static char rawtex_names[MAX_RAWTEX][64];
static int nrawtex = 0;

static int is_rawtex(const char* name) {
    for (int i = 0; i < nrawtex; i++) {
        if (strcmp(rawtex_names[i], name) == 0) return 1;
    }
    return 0;
}

// Check if a uniform name was already set by the user via --uniform or --itime.
// Used to prevent Shadertoy auto-defaults from overwriting explicit user values.
static int was_uniform_set(const char* name, UniformArg* uniforms, int nuniforms,
                           int itime_set_flag) {
    if (strcmp(name, "iTime") == 0 && itime_set_flag) return 1;
    for (int i = 0; i < nuniforms; i++) {
        if (strcmp(uniforms[i].name, name) == 0) return 1;
    }
    return 0;
}

// Find a uniform location, trying multiple name patterns to handle
// uniform block members (e.g. "u.iResolution", "iResolution").
static GLint find_uniform(GLuint prog, const char* name) {
    // Try the name exactly as given
    GLint loc = glGetUniformLocation(prog, name);
    if (loc >= 0) return loc;

    // Try common uniform block instance prefixes (e.g. "u.name", "params.name")
    static const char* prefixes[] = {"u.", "params.", "p.", "ub.", NULL};
    char buf[128];
    for (int i = 0; prefixes[i]; i++) {
        int n = snprintf(buf, sizeof(buf), "%s%s", prefixes[i], name);
        if (n > 0 && n < (int)sizeof(buf)) {
            loc = glGetUniformLocation(prog, buf);
            if (loc >= 0) return loc;
        }
    }

    return -1;
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, sz, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static int has_mainimage(const char* src) {
    return strstr(src, "mainImage") != NULL;
}

static int has_void_main(const char* src) {
    return strstr(src, "void main(") != NULL;
}

static void build_shadertoy_wrapper(const char* src, char* out, size_t out_sz) {
    snprintf(out, out_sz,
        "#version 330 core\n"
        "out vec4 outColor;\n"
        "%s\n"
        "void main() {\n"
        "    vec4 c;\n"
        "    mainImage(c, gl_FragCoord.xy);\n"
        "    outColor = c;\n"
        "}\n",
        src);
}

static void build_standard_wrapper(const char* src, char* out, size_t out_sz) {
    // Strip any #version line and pick the right GLSL version
    const char* body = src;
    char first_line[256] = {0};
    const char* nl = strchr(src, '\n');
    if (nl) {
        size_t len = nl - src;
        if (len < sizeof(first_line)) {
            memcpy(first_line, src, len);
            first_line[len] = '\0';
        }
    }

    if (strstr(first_line, "#version")) {
        body = nl + 1;
        while (*body == '\n' || *body == '\r') body++;
    }

    // Use 420 core when layout(binding) qualifiers are present (require 420+)
    const char* ver = (strstr(body, "layout(binding") != NULL) ? "420" : "330";

    // Ensure there's an out vec4 declaration if not present
    int has_out = (strstr(body, "out vec4") != NULL || strstr(body, "out vec4") != NULL ||
                   strstr(body, "layout(location") != NULL);

    if (has_out) {
        snprintf(out, out_sz,
            "#version %s core\n"
            "%s\n",
            ver, body);
    } else {
        // gl_FragColor fallback — add out declaration
        snprintf(out, out_sz,
            "#version %s core\n"
            "out vec4 outColor;\n"
            "%s\n",
            ver, body);
    }
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error (type=%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static int load_ppm_texture(const char* path, GLuint texture_unit, int use_nearest) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open texture '%s'\n", path);
        return 0;
    }

    char magic[3];
    if (!fgets(magic, sizeof(magic), f) || strncmp(magic, "P6", 2) != 0) {
        fprintf(stderr, "ERROR: '%s' is not a P6 PPM file\n", path);
        fclose(f);
        return 0;
    }

    int w, h, maxval;
    if (fscanf(f, "%d %d\n%d\n", &w, &h, &maxval) != 3 || maxval != 255) {
        fprintf(stderr, "ERROR: bad PPM header in '%s'\n", path);
        fclose(f);
        return 0;
    }

    unsigned char* data = malloc(w * h * 3);
    size_t n = fread(data, 1, w * h * 3, f);
    fclose(f);

    if ((int)n != w * h * 3) {
        fprintf(stderr, "ERROR: short read on '%s' (%zu != %d)\n", path, n, w * h * 3);
        free(data);
        return 0;
    }

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint filter = use_nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    free(data);
    return 1;
}

static int load_raw_rgba_texture(const char* path, GLuint texture_unit, int use_nearest) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open texture '%s'\n", path);
        return 0;
    }

    int w, h;
    if (fread(&w, sizeof(int), 1, f) != 1 || fread(&h, sizeof(int), 1, f) != 1) {
        fprintf(stderr, "ERROR: bad header in raw RGBA file '%s'\n", path);
        fclose(f);
        return 0;
    }

    int npixels = w * h;
    if (npixels <= 0 || npixels > 8192 * 8192) {
        fprintf(stderr, "ERROR: invalid dimensions in '%s' (%dx%d)\n", path, w, h);
        fclose(f);
        return 0;
    }

    unsigned char* data = malloc(npixels * 4);
    if (!data) {
        fprintf(stderr, "ERROR: out of memory for '%s'\n", path);
        fclose(f);
        return 0;
    }

    size_t n = fread(data, 1, npixels * 4, f);
    fclose(f);

    if ((int)n != npixels * 4) {
        fprintf(stderr, "ERROR: short read on '%s' (%zu != %d)\n", path, n, npixels * 4);
        free(data);
        return 0;
    }

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint filter = use_nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    free(data);
    return 1;
}

typedef GLXContext (*glXCreateContextAttribsARBProc)(
    Display*, GLXFBConfig, GLXContext, Bool, const int*);

int main(int argc, char** argv) {
    const char* frag_path = NULL;
    const char* out_path = NULL;
    int w = WIN_W, h = WIN_H;
    int raw_out = 0;

    TexArg textures[MAX_TEXTURES];
    int ntex = 0;
    TexArg rgba_textures[MAX_TEXTURES];
    int n_rgbatex = 0;
    UniformArg uniforms[MAX_UNIFORMS];
    int nuniforms = 0;
    float cmdline_iTime = 0.0f;
    int cmdline_itime_set = 0;

    // Parse arguments
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--texture") == 0 && i + 2 < argc) {
            if (ntex < MAX_TEXTURES) {
                strncpy(textures[ntex].name, argv[i+1], 63);
                textures[ntex].name[63] = '\0';
                strncpy(textures[ntex].path, argv[i+2], 511);
                textures[ntex].path[511] = '\0';
                ntex++;
            }
            i += 3;
        } else if (strcmp(argv[i], "--rawtex") == 0 && i + 1 < argc) {
            if (nrawtex < MAX_RAWTEX) {
                strncpy(rawtex_names[nrawtex], argv[i+1], 63);
                rawtex_names[nrawtex][63] = '\0';
                nrawtex++;
            }
            i += 2;
        } else if (strcmp(argv[i], "--raw") == 0) {
            raw_out = 1;
            i++;
        } else if (strcmp(argv[i], "--rgatex") == 0 && i + 2 < argc) {
            if (n_rgbatex < MAX_TEXTURES) {
                strncpy(rgba_textures[n_rgbatex].name, argv[i+1], 63);
                rgba_textures[n_rgbatex].name[63] = '\0';
                strncpy(rgba_textures[n_rgbatex].path, argv[i+2], 511);
                rgba_textures[n_rgbatex].path[511] = '\0';
                n_rgbatex++;
            }
            i += 3;
        } else if (strcmp(argv[i], "--itime") == 0 && i + 1 < argc) {
            cmdline_iTime = strtof(argv[i+1], NULL);
            cmdline_itime_set = 1;
            i += 2;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s <frag_shader> <output.ppm> [width] [height] [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --texture name path   Load PPM texture (RGB only, no alpha)\n");
            printf("  --rgatex name path    Load raw RGBA texture (preserves alpha)\n");
            printf("  --rawtex name         Use nearest-neighbor for named texture\n");
            printf("  --raw                 Output raw RGBA (with alpha) instead of PPM\n");
            printf("  --uniform name v...   Set uniform (1-4 float values)\n");
            printf("  --itime value         Set iTime uniform (overrides Shadertoy default)\n");
            printf("  --help, -h            Show this help\n");
            return 0;
        } else if (strcmp(argv[i], "--uniform") == 0 && i + 2 < argc) {
            if (nuniforms < MAX_UNIFORMS) {
                strncpy(uniforms[nuniforms].name, argv[i+1], 63);
                uniforms[nuniforms].name[63] = '\0';
                // Count value arguments
                int nv = 0;
                int j = i + 2;
                while (j < argc && nv < 4) {
                    char* end;
                    strtof(argv[j], &end);
                    if (end == argv[j] || *end != '\0') break; // not a number
                    uniforms[nuniforms].values[nv] = strtof(argv[j], NULL);
                    nv++;
                    j++;
                }
                uniforms[nuniforms].nvalues = nv;
                nuniforms++;
                i = j;
            } else {
                i++;
            }
        } else if (argv[i][0] != '-') {
            if (!frag_path) frag_path = argv[i];
            else if (!out_path) out_path = argv[i];
            else if (w == WIN_W && !out_path) { /* skip */ }
            i++;
        } else {
            i++;
        }
    }

    // Positional args: frag_path, out_path, [width], [height]
    // Re-parse positional only (first 2 or 4 non-flag args)
    {
        int pos = 0;
        for (int k = 1; k < argc; k++) {
            // Handle bare "-" as a positional (stdout indicator), not a flag
            int is_positional = (argv[k][0] != '-') || (strcmp(argv[k], "-") == 0);
            if (is_positional) {
                if (pos == 0) frag_path = argv[k];
                else if (pos == 1) out_path = argv[k];
                else if (pos == 2) w = atoi(argv[k]);
                else if (pos == 3) h = atoi(argv[k]);
                pos++;
            } else {
                // Skip flag and its values
                if (strcmp(argv[k], "--texture") == 0 || strcmp(argv[k], "--rgatex") == 0) k += 2;
                else if (strcmp(argv[k], "--rawtex") == 0 || strcmp(argv[k], "--itime") == 0) k += 1;
                else if (strcmp(argv[k], "--uniform") == 0) {
                    k++;
                    while (k + 1 < argc && argv[k+1][0] != '-') k++;
                }
            }
        }
    }

    if (!frag_path || !out_path) {
        fprintf(stderr, "Usage: %s <frag_shader> <output.ppm> [width] [height] [--texture name path] [--rgatex name path] [--rawtex name] [--itime val] [--uniform name val...] ...\n", argv[0]);
        return 1;
    }

    // Read fragment shader
    char* frag_src = read_file(frag_path);
    if (!frag_src) {
        fprintf(stderr, "ERROR: cannot read %s\n", frag_path);
        return 1;
    }

    char full_frag[65536];
    int is_shadertoy = has_mainimage(frag_src) && !has_void_main(frag_src);
    if (is_shadertoy) {
        build_shadertoy_wrapper(frag_src, full_frag, sizeof(full_frag));
    } else {
        build_standard_wrapper(frag_src, full_frag, sizeof(full_frag));
    }
    free(frag_src);

    // X11 display
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ERROR: cannot open X display\n");
        return 1;
    }

    int fb_attr[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 0, GLX_STENCIL_SIZE, 0,
        GLX_DOUBLEBUFFER, False,
        None
    };

    int n_fb;
    GLXFBConfig* fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), fb_attr, &n_fb);
    if (!fbc || n_fb == 0) {
        fprintf(stderr, "ERROR: no matching GLXFBConfig\n");
        XCloseDisplay(dpy);
        return 1;
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddress(
            (const GLubyte*)"glXCreateContextAttribsARB");

    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "ERROR: glXCreateContextAttribsARB not found\n");
        XFree(fbc);
        XCloseDisplay(dpy);
        return 1;
    }

    int ctx_attr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXContext ctx = glXCreateContextAttribsARB(dpy, fbc[0], NULL, True, ctx_attr);
    if (!ctx) {
        fprintf(stderr, "ERROR: cannot create GLX context\n");
        XFree(fbc);
        XCloseDisplay(dpy);
        return 1;
    }

    int pb_attr[] = { GLX_PBUFFER_WIDTH, w, GLX_PBUFFER_HEIGHT, h, None };
    GLXPbuffer pbuf = glXCreatePbuffer(dpy, fbc[0], pb_attr);
    if (!pbuf) {
        fprintf(stderr, "ERROR: cannot create pbuffer\n");
        glXDestroyContext(dpy, ctx);
        XFree(fbc);
        XCloseDisplay(dpy);
        return 1;
    }

    glXMakeContextCurrent(dpy, pbuf, pbuf, ctx);
    fprintf(stderr, "OpenGL %s | %s (%dx%d)\n",
            glGetString(GL_VERSION), frag_path, w, h);

    // Compile shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, fullscreen_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, full_frag);
    if (!vs || !fs) {
        glXDestroyPbuffer(dpy, pbuf);
        glXDestroyContext(dpy, ctx);
        XFree(fbc);
        XCloseDisplay(dpy);
        return 1;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint link_ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "Link error:\n%s\n", log);
        glDeleteShader(vs); glDeleteShader(fs); glDeleteProgram(prog);
        glXDestroyPbuffer(dpy, pbuf);
        glXDestroyContext(dpy, ctx);
        XFree(fbc);
        XCloseDisplay(dpy);
        return 1;
    }
    glUseProgram(prog);

    // Load textures (PPM)
    int tex_unit = 0;
    for (int t = 0; t < ntex; t++) {
        if (!load_ppm_texture(textures[t].path, tex_unit, is_rawtex(textures[t].name))) {
            fprintf(stderr, "WARNING: failed to load texture '%s'\n", textures[t].name);
            continue;
        }
        GLint loc = find_uniform(prog, textures[t].name);
        if (loc >= 0) {
            glUniform1i(loc, tex_unit);
        } else {
            fprintf(stderr, "WARNING: sampler2D '%s' not found in shader\n", textures[t].name);
        }
        tex_unit++;
    }

    // Load RGBA textures (raw binary, preserves alpha)
    for (int t = 0; t < n_rgbatex; t++) {
        if (!load_raw_rgba_texture(rgba_textures[t].path, tex_unit, is_rawtex(rgba_textures[t].name))) {
            fprintf(stderr, "WARNING: failed to load RGBA texture '%s'\n", rgba_textures[t].name);
            continue;
        }
        GLint loc = find_uniform(prog, rgba_textures[t].name);
        if (loc >= 0) {
            glUniform1i(loc, tex_unit);
        } else {
            fprintf(stderr, "WARNING: sampler2D '%s' not found in shader\n", rgba_textures[t].name);
        }
        tex_unit++;
    }

    // Set uniforms
    for (int u = 0; u < nuniforms; u++) {
        GLint loc = find_uniform(prog, uniforms[u].name);
        if (loc < 0) {
            fprintf(stderr, "WARNING: uniform '%s' not found in shader\n", uniforms[u].name);
            continue;
        }
        switch (uniforms[u].nvalues) {
            case 1: glUniform1f(loc, uniforms[u].values[0]); break;
            case 2: glUniform2f(loc, uniforms[u].values[0], uniforms[u].values[1]); break;
            case 3: glUniform3f(loc, uniforms[u].values[0], uniforms[u].values[1], uniforms[u].values[2]); break;
            case 4: glUniform4f(loc, uniforms[u].values[0], uniforms[u].values[1], uniforms[u].values[2], uniforms[u].values[3]); break;
        }
    }

    // Check for common Shadertoy uniforms (only set if not already provided by user)
    {
        GLint loc;
        if (!was_uniform_set("iResolution", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iResolution");
            if (loc >= 0) glUniform3f(loc, (float)w, (float)h, 1.0f);
        }
        if (!was_uniform_set("iTime", uniforms, nuniforms, cmdline_itime_set)) {
            loc = find_uniform(prog, "iTime");
            if (loc >= 0) {
                if (cmdline_itime_set)
                    glUniform1f(loc, cmdline_iTime);
                else
                    glUniform1f(loc, 1.5f);
            }
        } else if (cmdline_itime_set) {
            // User set --itime; apply even if uniform was also set via --uniform
            // (--uniform takes precedence, so only apply if no --uniform iTime)
            loc = find_uniform(prog, "iTime");
            if (loc >= 0) glUniform1f(loc, cmdline_iTime);
        }
        if (!was_uniform_set("iTimeDelta", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iTimeDelta");
            if (loc >= 0) glUniform1f(loc, 1.0f / 30.0f);
        }
        if (!was_uniform_set("iFrameRate", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iFrameRate");
            if (loc >= 0) glUniform1f(loc, 30.0f);
        }
        if (!was_uniform_set("iFrame", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iFrame");
            if (loc >= 0) glUniform1i(loc, 45);
        }
        if (!was_uniform_set("iMouse", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iMouse");
            if (loc >= 0) glUniform4f(loc, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (!was_uniform_set("iDate", uniforms, nuniforms, 0)) {
            loc = find_uniform(prog, "iDate");
            if (loc >= 0) glUniform4f(loc, 2026.0f, 4.0f, 29.0f, 0.0f);
        }
    }

    // VAO required for core profile
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Render
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    // Read pixels (bottom-to-top from OpenGL)
    unsigned char* pixels = malloc(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    FILE* out;
    if (strcmp(out_path, "-") == 0) {
        out = stdout;
    } else {
        out = fopen(out_path, "wb");
    }
    if (out) {
        if (raw_out) {
            // Raw RGBA: [4B width][4B height][RGBA top-to-bottom]
            int32_t wh[2] = {w, h};
            fwrite(wh, 4, 2, out);
            for (int y = h - 1; y >= 0; y--) {
                fwrite(pixels + y * w * 4, 1, w * 4, out);
            }
        } else {
            // PPM P6 (RGB only, bottom-to-top as OpenGL gives)
            fprintf(out, "P6\n%d %d\n255\n", w, h);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = (y * w + x) * 4;
                    fputc(pixels[idx + 0], out);
                    fputc(pixels[idx + 1], out);
                    fputc(pixels[idx + 2], out);
                }
            }
        }
        fflush(out);
        if (strcmp(out_path, "-") != 0) {
            fclose(out);
            printf("%s\n", out_path);
        }
    } else {
        fprintf(stderr, "ERROR: cannot write output '%s'\n", out_path);
    }

    free(pixels);

    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteShader(fs);
    glDeleteShader(vs);
    glXDestroyPbuffer(dpy, pbuf);
    glXDestroyContext(dpy, ctx);
    XFree(fbc);
    XCloseDisplay(dpy);

    return 0;
}
