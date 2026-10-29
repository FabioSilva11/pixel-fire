#include "gl_util.h"

#include <algorithm>
#include <vector>

#include "../core/log.h"

namespace zz {

namespace {
GLuint compileShader(GLenum type, const char* source, const char* label) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(std::max(1, len)));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        LOGE("Shader %s (%s) falhou: %s", label, type == GL_VERTEX_SHADER ? "vs" : "fs", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}  // namespace

GLuint compileProgram(const char* vertexSource, const char* fragmentSource, const char* label) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource, label);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource, label);
    if (!vs || !fs) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(std::max(1, len)));
        glGetProgramInfoLog(program, len, nullptr, log.data());
        LOGE("Link %s falhou: %s", label, log.data());
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

}  // namespace zz
