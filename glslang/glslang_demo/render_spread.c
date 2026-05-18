//
// Offscreen renderer for fragment shaders — GLX + OpenGL, outputs PPM
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

static const char* vertex_shader_src =
    "#version 330 core\n"
    "const vec2 pos[3] = vec2[3](\n"
    "    vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));\n"
    "void main() {\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        return 0;
    }
    return s;
}

typedef GLXContext (*glXCreateContextAttribsARBProc)(
    Display*, GLXFBConfig, GLXContext, Bool, const int*);

int main(int argc, char** argv) {
    const char* frag_path = "glslang_demo/spread.frag";
    int w = WIN_W, h = WIN_H;
    float time_val = 1.5f;
    const char* out_path = NULL;

    if (argc > 1) frag_path = argv[1];
    if (argc > 2) w = atoi(argv[2]);
    if (argc > 3) h = atoi(argv[3]);
    if (argc > 4) time_val = atof(argv[4]);
    if (argc > 5) out_path = argv[5];

    // Read fragment shader
    char* frag_src = read_file(frag_path);
    if (!frag_src) {
        fprintf(stderr, "ERROR: cannot read %s\n", frag_path);
        return 1;
    }

    char full_frag[32768];
    snprintf(full_frag, sizeof(full_frag),
        "#version 330 core\n"
        "out vec4 outColor;\n"
        "%s\n"
        "void main() {\n"
        "    vec4 c;\n"
        "    mainImage(c, gl_FragCoord.xy);\n"
        "    outColor = c;\n"
        "}\n",
        frag_src);
    free(frag_src);

    // X11 display
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ERROR: cannot open X display\n");
        return 1;
    }

    // GLXFBConfig
    int fb_attr[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 0,
        GLX_STENCIL_SIZE, 0,
        GLX_DOUBLEBUFFER, False,
        None
    };

    int n_fb;
    GLXFBConfig* fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), fb_attr, &n_fb);
    if (!fbc || n_fb == 0) {
        fprintf(stderr, "ERROR: no matching GLXFBConfig\n");
        return 1;
    }

    // Get the createContextAttribs function
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddress(
            (const GLubyte*)"glXCreateContextAttribsARB");

    int ctx_attr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXContext ctx = glXCreateContextAttribsARB(dpy, fbc[0], NULL, True, ctx_attr);
    if (!ctx) {
        fprintf(stderr, "ERROR: cannot create GLX context\n");
        return 1;
    }

    // Pbuffer for offscreen rendering
    int pb_attr[] = {
        GLX_PBUFFER_WIDTH, w,
        GLX_PBUFFER_HEIGHT, h,
        None
    };

    GLXPbuffer pbuf = glXCreatePbuffer(dpy, fbc[0], pb_attr);
    if (!pbuf) {
        fprintf(stderr, "ERROR: cannot create pbuffer\n");
        return 1;
    }

    glXMakeContextCurrent(dpy, pbuf, pbuf, ctx);

    fprintf(stderr, "OpenGL %s | %s (%dx%d) t=%.2f\n",
            glGetString(GL_VERSION), frag_path, w, h, time_val);

    // Compile shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, full_frag);
    if (!vs || !fs) { return 1; }

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
        return 1;
    }
    glUseProgram(prog);

    // VAO required for core profile
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Set uniforms
    GLint loc;
    loc = glGetUniformLocation(prog, "iResolution");
    if (loc >= 0) glUniform3f(loc, (float)w, (float)h, 1.0f);

    float fps = 30.0f;
    int frame_num = (int)(time_val * fps);

    loc = glGetUniformLocation(prog, "iTime");
    if (loc >= 0) glUniform1f(loc, time_val);

    loc = glGetUniformLocation(prog, "iTimeDelta");
    if (loc >= 0) glUniform1f(loc, 1.0f / fps);

    loc = glGetUniformLocation(prog, "iFrameRate");
    if (loc >= 0) glUniform1f(loc, fps);

    loc = glGetUniformLocation(prog, "iFrame");
    if (loc >= 0) glUniform1i(loc, frame_num);

    loc = glGetUniformLocation(prog, "iMouse");
    if (loc >= 0) glUniform4f(loc, 0.0f, 0.0f, 0.0f, 0.0f);

    loc = glGetUniformLocation(prog, "iSampleRate");
    if (loc >= 0) glUniform1f(loc, 44100.0f);

    loc = glGetUniformLocation(prog, "iDate");
    if (loc >= 0) glUniform4f(loc, 2026.0f, 4.0f, 29.0f, 0.0f);

    // Render
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    // Read pixels
    unsigned char* pixels = malloc(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Write PPM (P6 binary format)
    char default_out[512];
    if (!out_path) {
        snprintf(default_out, sizeof(default_out), "glslang_demo/spread_output.ppm");
        out_path = default_out;
    }
    FILE* out = fopen(out_path, "wb");
    if (out) {
        fprintf(out, "P6\n%d %d\n255\n", w, h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int idx = (y * w + x) * 4;
                fputc(pixels[idx + 0], out);
                fputc(pixels[idx + 1], out);
                fputc(pixels[idx + 2], out);
            }
        }
        fclose(out);
        printf("%s\n", out_path);
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
