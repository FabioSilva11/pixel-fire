#pragma once

#include <GLES3/gl3.h>

#include <string>

namespace zz {

GLuint compileProgram(const char* vertexSource, const char* fragmentSource, const char* label);

// Programa GLSL com cache dos locais de uniformes. A chave é o ENDEREÇO do literal: passe sempre literais
// ("u_viewProj"), nunca um buffer dinâmico. Literal duplicado em outro ponto só ocupa outra entrada (correto).
struct ShaderProgram {
    GLuint id = 0;

    GLint uniform(const char* name) const {
        if (cachedId_ != id || cachedEpoch_ != epoch()) {  // programa recriado (onSurfaceCreated)
            count_ = 0;
            cachedId_ = id;
            cachedEpoch_ = epoch();
        }
        for (int i = 0; i < count_; ++i)
            if (keys_[i] == name) return locs_[i];
        const GLint loc = glGetUniformLocation(id, name);
        if (count_ < kMax) {
            keys_[count_] = name;
            locs_[count_] = loc;
            ++count_;
        }
        return loc;
    }

    // Chame ao recriar os programas (os ids de GL podem se repetir entre contextos).
    static void invalidateAll() { ++epoch(); }

private:
    static unsigned& epoch() {
        static unsigned e = 1;
        return e;
    }
    static constexpr int kMax = 64;
    mutable const char* keys_[kMax] = {};
    mutable GLint locs_[kMax] = {};
    mutable int count_ = 0;
    mutable GLuint cachedId_ = 0;
    mutable unsigned cachedEpoch_ = 0;
};

}  // namespace zz
