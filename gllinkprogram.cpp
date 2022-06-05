#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, "gllinkProgram", __VA_ARGS__))
#define LOGW(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_WARN, "gllinkProgram", __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "gllinkProgram", __VA_ARGS__))

const char *vertexCode = R"(
#version 310 es

layout(location = 0) in vec4 vertex_pos;
layout(location = 1) in vec2 vertex_tex0;
layout(location = 0) out vec2 input_TexCoord;

void main()
{
    gl_Position = vertex_pos;
    input_TexCoord = vertex_tex0;
}
)";

const char *fragCode = R"(
#version 310 es

precision mediump float;
precision highp int;

layout(std140) uniform ColorTransformBlock
{
mat4 yuvCoef;
} colorTransform;

layout(binding = 0) uniform highp sampler2D YTex;

layout(location = 0) in highp vec2 input_TexCoord;
layout(location = 0) out highp vec4 _entryPointOutput;

mat4 dummy(mat4 wrap) { return wrap; }

void main()
{
_entryPointOutput = vec4(texture(YTex, input_TexCoord).x) * dummy(colorTransform.yuvCoef);
}
)";

class GLContext {
private:
  EGLContext context_ = EGL_NO_CONTEXT;
  EGLConfig config_;

  GLContext(GLContext const &);
  void operator=(GLContext const &);

public:
  GLContext() {}

  ~GLContext() {
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (context_ != EGL_NO_CONTEXT) {
      eglDestroyContext(display, context_);
    }
    eglTerminate(display);
  }

  bool init() {
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    const EGLint attribs[] = {EGL_RENDERABLE_TYPE,
                              EGL_OPENGL_ES3_BIT,
                              EGL_BLUE_SIZE,
                              8,
                              EGL_GREEN_SIZE,
                              8,
                              EGL_RED_SIZE,
                              8,
                              EGL_ALPHA_SIZE,
                              8,
                              EGL_NONE};

    EGLint num_configs;
    EGLConfig config;
    eglChooseConfig(display, attribs, &config, 1, &num_configs);

    if (!num_configs) {
      LOGE("Unable to retrieve EGL config");
      return false;
    }

    const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                      EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};

    context_ = eglCreateContext(display, config, nullptr, context_attribs);

    if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) ==
        EGL_FALSE) {
      LOGE("Unable to eglMakeCurrent");
      return false;
    }

    return true;
  }
};

static bool compileShader(GLuint *shader, const GLenum type,
                          const GLchar *source, const int32_t iSize) {
  if (source == NULL || iSize <= 0)
    return false;

  *shader = glCreateShader(type);
  glShaderSource(*shader, 1, &source, &iSize);

  glCompileShader(*shader);

  GLint logLength;
  glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetShaderInfoLog(*shader, logLength, &logLength, log);
    LOGI("Shader compile log:\n%s", log);
    free(log);
  }

  GLint status;
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    glDeleteShader(*shader);
    LOGE("Failed to compile shader");
    return false;
  }

  return true;
}

static bool linkProgram(const GLuint prog) {
  GLint status;

  glLinkProgram(prog);

  GLint logLength;
  glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetProgramInfoLog(prog, logLength, &logLength, log);
    LOGE("Program link log:\n%s", log);
    free(log);
  }

  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (status == 0) {
    LOGE("Program link failed\n");
    return false;
  }

  return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_gllinkprogram_MainActivity_linkProgram(JNIEnv *env,
                                                        jobject thiz) {
  GLContext context;
  if (!context.init()) {
    return false;
  }

  int size = strlen(vertexCode);
  GLuint vertextShader;
  if (!compileShader(&vertextShader, GL_VERTEX_SHADER, vertexCode, size)) {
    return false;
  }

  int size2 = strlen(fragCode);
  GLuint fragShader;
  if (!compileShader(&fragShader, GL_FRAGMENT_SHADER, fragCode, size2)) {
    return false;
  }

  // Create shader program
  GLuint program = glCreateProgram();

  glAttachShader(program, vertextShader);
  glAttachShader(program, fragShader);

  bool ret = linkProgram(program);

  glDeleteShader(vertextShader);
  glDeleteShader(fragShader);
  glDeleteProgram(program);

  return ret;
}
