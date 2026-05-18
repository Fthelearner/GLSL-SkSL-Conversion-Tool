// Real-time GLSL shader preview window using SDL2 + OpenGL.
// Renders continuously with wall-clock iTime. Close window or Esc to exit.
//
// Build:
//   gcc -O2 -o render_glsl_live render_glsl_live.c \
//       $(pkg-config --cflags --libs sdl2 epoxy) -lGL -lm
//
// Usage:
//   ./render_glsl_live <frag_shader> [--width W] [--height H] [--fps N]
//                       [--texture name path] [--uniform name val...]

#include <epoxy/gl.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_TEXTURES 8
#define MAX_UNIFORMS 32
#define MAX_RAWTEX 8
#define MAX_RGBA_TEX 8

typedef struct { char name[64]; char path[512]; } TexArg;
typedef struct { char name[64]; int nvalues; float values[4]; } UniformArg;

// ── helpers ─────────────────────────────────────────────────────────

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
        "}\n", src);
}

static void build_standard_wrapper(const char* src, char* out, size_t out_sz) {
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
    int has_out = (strstr(body, "out vec4") != NULL ||
                   strstr(body, "layout(location") != NULL);
    if (has_out) {
        snprintf(out, out_sz, "#version 330 core\n%s\n", body);
    } else {
        snprintf(out, out_sz,
            "#version 330 core\n"
            "out vec4 outColor;\n"
            "%s\n", body);
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
        fprintf(stderr, "Shader compile error (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static int load_raw_rgba_texture(const char* path, GLuint texture_unit, int use_nearest) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "ERROR: cannot open '%s'\n", path); return 0; }
    int w, h;
    if (fread(&w, sizeof(int), 1, f) != 1 || fread(&h, sizeof(int), 1, f) != 1) {
        fprintf(stderr, "ERROR: bad raw RGBA header '%s'\n", path);
        fclose(f); return 0;
    }
    int npixels = w * h;
    if (npixels <= 0 || npixels > 8192 * 8192) {
        fclose(f); return 0;
    }
    unsigned char* data = malloc(npixels * 4);
    if (!data) { fclose(f); return 0; }
    size_t n = fread(data, 1, npixels * 4, f);
    fclose(f);
    if ((int)n != npixels * 4) { free(data); return 0; }
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

static int load_ppm_texture(const char* path, GLuint texture_unit, int use_nearest) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    char magic[3];
    if (!fgets(magic, sizeof(magic), f) || strncmp(magic, "P6", 2) != 0) {
        fclose(f); return 0;
    }
    int w, h, maxval;
    if (fscanf(f, "%d %d\n%d\n", &w, &h, &maxval) != 3 || maxval != 255) {
        fclose(f); return 0;
    }
    unsigned char* data = malloc(w * h * 3);
    size_t n = fread(data, 1, w * h * 3, f);
    fclose(f);
    if ((int)n != w * h * 3) { free(data); return 0; }
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

static int was_uniform_set(const char* name, UniformArg* uniforms, int nuniforms,
                           int itime_set_flag) {
    if (strcmp(name, "iTime") == 0 && itime_set_flag) return 1;
    for (int i = 0; i < nuniforms; i++) {
        if (strcmp(uniforms[i].name, name) == 0) return 1;
    }
    return 0;
}

// ── fullscreen vertex shader ────────────────────────────────────────

static const char* fullscreen_vs =
    "#version 330 core\n"
    "const vec2 pos[3] = vec2[3](\n"
    "    vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));\n"
    "void main() {\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

// ── main ────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    const char* frag_path = NULL;
    int width = 1280, height = 720;
    int target_fps = 0;  // 0 = unlimited
    const char* time_uniform = "iTime";

    TexArg textures[MAX_TEXTURES]; int ntex = 0;
    TexArg rgba_textures[MAX_RGBA_TEX]; int n_rgbatex = 0;
    char rawtex_names[MAX_RAWTEX][64]; int nrawtex = 0;
    UniformArg uniforms[MAX_UNIFORMS]; int nuniforms = 0;

    // Parse arguments
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--texture") == 0 && i + 2 < argc) {
            if (ntex < MAX_TEXTURES) {
                strncpy(textures[ntex].name, argv[i+1], 63);
                strncpy(textures[ntex].path, argv[i+2], 511);
                ntex++;
            }
            i += 3;
        } else if (strcmp(argv[i], "--rgatex") == 0 && i + 2 < argc) {
            if (n_rgbatex < MAX_RGBA_TEX) {
                strncpy(rgba_textures[n_rgbatex].name, argv[i+1], 63);
                strncpy(rgba_textures[n_rgbatex].path, argv[i+2], 511);
                n_rgbatex++;
            }
            i += 3;
        } else if (strcmp(argv[i], "--rawtex") == 0 && i + 1 < argc) {
            if (nrawtex < MAX_RAWTEX) {
                strncpy(rawtex_names[nrawtex], argv[i+1], 63);
                nrawtex++;
            }
            i += 2;
        } else if (strcmp(argv[i], "--uniform") == 0 && i + 2 < argc) {
            if (nuniforms < MAX_UNIFORMS) {
                strncpy(uniforms[nuniforms].name, argv[i+1], 63);
                int nv = 0, j = i + 2;
                while (j < argc && nv < 4) {
                    char* end;
                    strtof(argv[j], &end);
                    if (end == argv[j] || *end != '\0') break;
                    uniforms[nuniforms].values[nv] = strtof(argv[j], NULL);
                    nv++; j++;
                }
                uniforms[nuniforms].nvalues = nv;
                nuniforms++;
                i = j;
            } else { i++; }
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = atoi(argv[i+1]); i += 2;
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = atoi(argv[i+1]); i += 2;
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            target_fps = atoi(argv[i+1]); i += 2;
        } else if (strcmp(argv[i], "--time-uniform") == 0 && i + 1 < argc) {
            time_uniform = argv[i+1]; i += 2;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s <frag_shader> [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --width W --height H   Resolution (default: 1280x720)\n");
            printf("  --fps N                Target FPS (0=unlimited, default: 0)\n");
            printf("  --time-uniform NAME    Time uniform name (default: iTime)\n");
            printf("  --texture name path    PPM texture\n");
            printf("  --rgatex name path     Raw RGBA texture (preserves alpha)\n");
            printf("  --rawtex name          Nearest-neighbor for named texture\n");
            printf("  --itime value          Initial iTime value\n");
            printf("  --uniform name v...    Set uniform (1-4 floats)\n");
            return 0;
        } else if (argv[i][0] != '-' && !frag_path) {
            frag_path = argv[i]; i++;
        } else {
            i++;
        }
    }

    if (!frag_path) {
        fprintf(stderr, "Usage: %s <frag_shader> [--width W] [--height H] ...\n", argv[0]);
        return 1;
    }

    // Read and wrap fragment shader
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

    // Init SDL2 with OpenGL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "GLSL Live Preview", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "ERROR: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "ERROR: SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
    SDL_GL_SetSwapInterval(target_fps > 0 ? 1 : 1);

    fprintf(stderr, "OpenGL %s | %s (%dx%d)\n",
            glGetString(GL_VERSION), frag_path, width, height);

    // Compile shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, fullscreen_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, full_frag);
    if (!vs || !fs) { SDL_GL_DeleteContext(gl_ctx); SDL_DestroyWindow(window); SDL_Quit(); return 1; }

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
        SDL_GL_DeleteContext(gl_ctx); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    glUseProgram(prog);

    // Load textures
    int tex_unit = 0;
    for (int t = 0; t < ntex; t++) {
        int raw = 0;
        for (int r = 0; r < nrawtex; r++)
            if (strcmp(rawtex_names[r], textures[t].name) == 0) raw = 1;
        if (!load_ppm_texture(textures[t].path, tex_unit, raw))
            fprintf(stderr, "WARNING: failed to load texture '%s'\n", textures[t].name);
        else {
            GLint loc = glGetUniformLocation(prog, textures[t].name);
            if (loc >= 0) glUniform1i(loc, tex_unit);
            tex_unit++;
        }
    }
    for (int t = 0; t < n_rgbatex; t++) {
        int raw = 0;
        for (int r = 0; r < nrawtex; r++)
            if (strcmp(rawtex_names[r], rgba_textures[t].name) == 0) raw = 1;
        if (!load_raw_rgba_texture(rgba_textures[t].path, tex_unit, raw))
            fprintf(stderr, "WARNING: failed to load RGBA texture '%s'\n", rgba_textures[t].name);
        else {
            GLint loc = glGetUniformLocation(prog, rgba_textures[t].name);
            if (loc >= 0) glUniform1i(loc, tex_unit);
            tex_unit++;
        }
    }

    // Set static uniforms
    for (int u = 0; u < nuniforms; u++) {
        GLint loc = glGetUniformLocation(prog, uniforms[u].name);
        if (loc < 0) continue;
        switch (uniforms[u].nvalues) {
            case 1: glUniform1f(loc, uniforms[u].values[0]); break;
            case 2: glUniform2f(loc, uniforms[u].values[0], uniforms[u].values[1]); break;
            case 3: glUniform3f(loc, uniforms[u].values[0], uniforms[u].values[1], uniforms[u].values[2]); break;
            case 4: glUniform4f(loc, uniforms[u].values[0], uniforms[u].values[1], uniforms[u].values[2], uniforms[u].values[3]); break;
        }
    }

    // Shadertoy auto-uniforms (only if not explicitly set)
    if (!was_uniform_set("iResolution", uniforms, nuniforms, 0)) {
        GLint loc = glGetUniformLocation(prog, "iResolution");
        if (loc >= 0) glUniform3f(loc, (float)width, (float)height, 1.0f);
    }
    if (!was_uniform_set("iFrameRate", uniforms, nuniforms, 0)) {
        GLint loc = glGetUniformLocation(prog, "iFrameRate");
        if (loc >= 0) glUniform1f(loc, 30.0f);
    }

    // iTime location (updated each frame)
    GLint iTime_loc = glGetUniformLocation(prog, time_uniform);
    if (iTime_loc < 0) {
        // Try "iTime" as fallback
        iTime_loc = glGetUniformLocation(prog, "iTime");
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // ── Render loop ─────────────────────────────────────────────────
    Uint32 start_ticks = SDL_GetTicks();
    int frame_count = 0;
    int running = 1;
    int frame_interval = target_fps > 0 ? 1000 / target_fps : 0;

    fprintf(stderr, "Live preview running. Press Esc or close window to exit.\n");

    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
            else if (event.type == SDL_WINDOWEVENT &&
                     event.window.event == SDL_WINDOWEVENT_RESIZED) {
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
            }
        }

        // Update iTime
        float elapsed = (frame_start - start_ticks) / 1000.0f;
        if (iTime_loc >= 0) {
            glUniform1f(iTime_loc, elapsed);
        }
        // Also update iTimeDelta
        GLint iTimeDelta_loc = glGetUniformLocation(prog, "iTimeDelta");
        if (iTimeDelta_loc >= 0) {
            glUniform1f(iTimeDelta_loc, frame_interval > 0 ? frame_interval / 1000.0f : 1.0f / 60.0f);
        }
        // iFrame
        GLint iFrame_loc = glGetUniformLocation(prog, "iFrame");
        if (iFrame_loc >= 0) {
            glUniform1i(iFrame_loc, frame_count);
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        SDL_GL_SwapWindow(window);

        frame_count++;

        // FPS limiting
        if (frame_interval > 0) {
            Uint32 frame_end = SDL_GetTicks();
            int frame_time = frame_end - frame_start;
            if (frame_time < frame_interval) {
                SDL_Delay(frame_interval - frame_time);
            }
        }

        // Update window title every 60 frames
        if (frame_count % 60 == 0) {
            Uint32 now = SDL_GetTicks();
            float dt = (now - start_ticks) / 1000.0f;
            if (dt > 0) {
                char title[256];
                snprintf(title, sizeof(title), "GLSL Live | %.0f fps | t=%.2fs",
                         frame_count / dt, elapsed);
                SDL_SetWindowTitle(window, title);
            }
        }
    }

    float total_time = (SDL_GetTicks() - start_ticks) / 1000.0f;
    fprintf(stderr, "%d frames in %.1fs (%.0f fps)\n",
            frame_count, total_time, frame_count / (total_time > 0 ? total_time : 0.001f));

    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteShader(fs);
    glDeleteShader(vs);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
