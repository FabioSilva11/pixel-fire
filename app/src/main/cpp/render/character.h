// Personagens de blocos (estilo Minecraft): hierarquia rígida de partes + animações TRS.
// O modelo é montado em código (ver blocky.cpp); este arquivo cuida da pose e das animações.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../core/math.h"

namespace zz {

struct ModelNode {
    std::string name;
    int parent = -1;
    int mesh = -1;
    Vec3 translation;
    Quat rotation;
    Vec3 scale{1, 1, 1};
};

struct ModelMesh {
    std::vector<float> vertices; // pos(3) normal(3) uv(2)
    std::vector<uint16_t> indices;
};

struct AnimChannel {
    int node = -1;
    int path = 0; // 0 = translação, 1 = rotação, 2 = escala
    bool step = false;
    bool cubic = false;
    std::vector<float> times;
    std::vector<float> values;
};

struct AnimClip {
    std::string name;
    float duration = 0;
    std::vector<AnimChannel> channels;
};

struct Pose {
    std::vector<Vec3> t;
    std::vector<Quat> r;
    std::vector<Vec3> s;
};

class CharacterModel {
  public:
    // Monta o modelo a partir de nós, malhas e clipes gerados em código. `height` > 0 fixa a altura de
    // referência (chapéus e cabelos altos não podem encolher o personagem).
    void build(std::vector<ModelNode> nodes, std::vector<ModelMesh> meshes, std::vector<AnimClip> clips,
               float height = 0.0f);

    const std::vector<ModelNode> &nodes() const { return nodes_; }
    const std::vector<ModelMesh> &meshes() const { return meshes_; }
    const std::vector<AnimClip> &clips() const { return clips_; }
    int clipIndex(const std::string &name) const;
    int nodeIndex(const std::string &name) const;
    // Outro nome para um nó (os nomes antigos das partes apontam para os ossos do esqueleto humano).
    void addAlias(const std::string &alias, int node) { aliases_.emplace_back(alias, node); }
    float bindHeight() const { return bindHeight_; }
    float bindMinY() const { return bindMinY_; }

    void bindPose(Pose &out) const;
    // Amostra o clipe por cima da pose recebida (normalmente a pose de bind).
    void sample(int clip, float time, bool loop, Pose &pose) const;
    // Standalone presentation (lobby) starts from the skeleton's complete rest pose.
    void sampleBind(int clip, float time, bool loop, Pose &pose) const {
        bindPose(pose);
        sample(clip, time, loop, pose);
    }
    void globals(const Pose &pose, std::vector<Mat4> &out) const;

  private:
    std::vector<ModelNode> nodes_;
    std::vector<std::pair<std::string, int>> aliases_;
    std::vector<int> order_; // pais antes dos filhos
    std::vector<ModelMesh> meshes_;
    std::vector<AnimClip> clips_;
    float bindHeight_ = 1.0f;
    float bindMinY_ = 0.0f;
};

void blendPose(const Pose &a, const Pose &b, float weightB, Pose &out);

// Controla clipe atual, transição suave e tempo de um personagem.
class Animator {
  public:
    void play(int clip, bool loop = true, float fade = 0.15f, float speed = 1.0f);
    void update(float dt);
    void evaluate(const CharacterModel &model, Pose &out);
    int clip() const { return clip_; }
    float time() const { return time_; }
    bool finished(const CharacterModel &model) const;
    void setSpeed(float s) { speed_ = s; }

  private:
    int clip_ = -1;
    int prevClip_ = -1;
    float time_ = 0, prevTime_ = 0;
    float fade_ = 0, fadeDuration_ = 0;
    bool loop_ = true, prevLoop_ = true;
    float speed_ = 1.0f, prevSpeed_ = 1.0f;
    Pose scratchA_, scratchB_;
    Pose displayedPose_, fadeSourcePose_;
    bool fadeFromPose_ = false;
};

} // namespace zz
