#include "blocky.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "../core/math.h"
#include "../core/random.h"
#include "renderer.h"
#include "voxel_humanoid.h"

namespace zz {

namespace {

// ----------------------------------------------------------------------------- animações procedurais
// Os clipes descrevem as sete partes antigas (raiz, tronco, cabeça, braços e pernas); no esqueleto humano
// elas caem em root, Spine, Head, RightArm/LeftArm e RightUpLeg/LeftUpLeg, e joelhos, cotovelos e pés
// acompanham o balanço de pernas e braços.

enum NodeId { N_ROOT = 0, N_TORSO, N_HEAD, N_ARM_R, N_ARM_L, N_LEG_R, N_LEG_L, N_COUNT };

struct KeyPose {
    Quat r[N_COUNT];
    Vec3 rootT;
};

Quat rx(float a) { return Quat::axisAngle({1, 0, 0}, a); }
Quat ry(float a) { return Quat::axisAngle({0, 1, 0}, a); }
Quat rz(float a) { return Quat::axisAngle({0, 0, 1}, a); }

// Ângulo do balanço em X de uma rotação dominada por rx (frente negativa, trás positiva).
float swing(const Quat& q) { return 2.0f * std::atan2(q.x, q.w); }

AnimClip makeClip(const HumanoidRig& rig, const char* name, float duration, const std::function<void(float, KeyPose&)>& fn) {
    AnimClip clip;
    clip.name = name;
    clip.duration = duration;
    const int keys = std::max(2, static_cast<int>(duration * 24.0f) + 1);
    std::vector<float> times(static_cast<size_t>(keys));
    std::vector<KeyPose> poses(static_cast<size_t>(keys));
    for (int k = 0; k < keys; ++k) {
        times[static_cast<size_t>(k)] = duration * k / (keys - 1);
        fn(times[static_cast<size_t>(k)], poses[static_cast<size_t>(k)]);
    }
    auto channel = [&](int node, const std::function<Quat(int)>& value) {
        if (node < 0) return;
        AnimChannel c;
        c.node = node;
        c.path = 1;
        c.times = times;
        for (int k = 0; k < keys; ++k) {
            const Quat q = value(k);
            c.values.insert(c.values.end(), {q.x, q.y, q.z, q.w});
        }
        clip.channels.push_back(std::move(c));
    };
    const int parts[N_COUNT] = {rig.root, rig.torso, rig.head, rig.armR, rig.armL, rig.legR, rig.legL};
    for (int n = 0; n < N_COUNT; ++n) channel(parts[n], [&](int k) { return poses[static_cast<size_t>(k)].r[n]; });
    if (rig.root >= 0) {
        AnimChannel t;
        t.node = rig.root;
        t.path = 0;
        t.times = times;
        for (const KeyPose& p : poses) t.values.insert(t.values.end(), {p.rootT.x, p.rootT.y, p.rootT.z});
        clip.channels.push_back(std::move(t));
    }
    // Joelho dobra quando a perna avança (fase de balanço), mais ainda agachado; o pé compensa para ficar plano.
    auto knee = [&](int leg, int k) {
        const int a = std::max(0, k - 1), b = std::min(keys - 1, k + 1);
        const float dt = times[static_cast<size_t>(b)] - times[static_cast<size_t>(a)];
        const float rate = dt > 0 ? (swing(poses[static_cast<size_t>(b)].r[leg]) - swing(poses[static_cast<size_t>(a)].r[leg])) / dt : 0.0f;
        float bend = 0.06f + 1.7f * std::max(0.0f, -rate) * duration / kTwoPi;
        const float drop = -poses[static_cast<size_t>(k)].rootT.y / kBlockyPixel;
        if (drop > 0) bend += std::min(0.6f, drop * 0.6f);
        return clampf(bend, 0.0f, 1.35f);
    };
    auto elbow = [&](int arm, int k) { return clampf(0.12f + 0.55f * std::max(0.0f, -swing(poses[static_cast<size_t>(k)].r[arm])), 0.0f, 1.3f); };
    channel(rig.kneeR, [&](int k) { return rx(knee(N_LEG_R, k)); });
    channel(rig.kneeL, [&](int k) { return rx(knee(N_LEG_L, k)); });
    channel(rig.footR, [&](int k) { return rx(-0.5f * (swing(poses[static_cast<size_t>(k)].r[N_LEG_R]) + knee(N_LEG_R, k))); });
    channel(rig.footL, [&](int k) { return rx(-0.5f * (swing(poses[static_cast<size_t>(k)].r[N_LEG_L]) + knee(N_LEG_L, k))); });
    channel(rig.foreR, [&](int k) { return rx(-elbow(N_ARM_R, k)); });
    channel(rig.foreL, [&](int k) { return rx(-elbow(N_ARM_L, k)); });
    return clip;
}

std::vector<AnimClip> buildClips(const HumanoidRig& rig) {
    const float P = kBlockyPixel;
    std::vector<AnimClip> clips;
    // Parado: respiração e braços balançando de leve.
    clips.push_back(makeClip(rig, "idle", 3.2f, [](float t, KeyPose& p) {
        float b = std::sin(t / 3.2f * kTwoPi);
        p.r[N_ARM_R] = rz(-0.05f - 0.025f * b) * rx(0.04f * b);
        p.r[N_ARM_L] = rz(0.05f + 0.025f * b) * rx(-0.04f * b);
        p.r[N_HEAD] = rx(0.025f * std::sin(t / 3.2f * kPi * 2.0f + 1.0f));
    }));
    // Andar: pernas e braços alternados, tronco girando um pouco.
    clips.push_back(makeClip(rig, "walk", 1.0f, [P](float t, KeyPose& p) {
        float s = std::sin(t * kTwoPi);
        p.r[N_LEG_R] = rx(0.62f * s);
        p.r[N_LEG_L] = rx(-0.62f * s);
        p.r[N_ARM_R] = rx(-0.55f * s) * rz(-0.04f);
        p.r[N_ARM_L] = rx(0.55f * s) * rz(0.04f);
        p.r[N_TORSO] = ry(0.07f * s);
        p.r[N_HEAD] = ry(-0.05f * s);
        p.rootT = {0, -0.35f * P * std::fabs(s), 0};
    }));
    // Recuar mantendo a mira: passada invertida e tronco ligeiramente para trás.
    clips.push_back(makeClip(rig, "walk-back", 1.0f, [P](float t, KeyPose& p) {
        float s = std::sin(t * kTwoPi);
        p.r[N_LEG_R] = rx(-0.48f * s);
        p.r[N_LEG_L] = rx(0.48f * s);
        p.r[N_ARM_R] = rx(0.2f * s) * rz(-0.03f);
        p.r[N_ARM_L] = rx(-0.2f * s) * rz(0.03f);
        p.r[N_TORSO] = rx(-0.08f) * ry(-0.05f * s);
        p.r[N_HEAD] = rx(0.04f) * ry(0.04f * s);
        p.rootT = {0, -0.22f * P * std::fabs(s), 0};
    }));
    // Variações procedurais para recuar correndo e deslocar-se agachado.
    clips.push_back(makeClip(rig, "run-back", 0.58f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 0.58f * kTwoPi);
        p.r[N_LEG_R] = rx(-0.92f * s + 0.08f);
        p.r[N_LEG_L] = rx(0.92f * s + 0.08f);
        p.r[N_ARM_R] = rx(0.8f * s - 0.1f) * rz(-0.1f);
        p.r[N_ARM_L] = rx(-0.8f * s - 0.1f) * rz(0.1f);
        p.r[N_TORSO] = rx(-0.12f) * ry(-0.08f * s);
        p.r[N_HEAD] = rx(-0.16f) * ry(0.06f * s);
        p.rootT = {0, P * (0.7f * std::fabs(std::cos(t / 0.58f * kTwoPi)) - 0.4f), 0};
    }));
    clips.push_back(makeClip(rig, "crouch-walk", 1.1f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 1.1f * kTwoPi);
        p.r[N_LEG_R] = rx(0.34f * s + 0.2f);
        p.r[N_LEG_L] = rx(-0.34f * s + 0.2f);
        p.r[N_ARM_R] = rx(-0.28f * s - 0.1f);
        p.r[N_ARM_L] = rx(0.28f * s - 0.1f);
        p.r[N_TORSO] = rx(0.12f) * ry(0.04f * s);
        p.r[N_HEAD] = rx(-0.08f) * ry(-0.03f * s);
        p.rootT = {0, -0.45f * P - 0.12f * P * std::fabs(s), 0};
    }));
    clips.push_back(makeClip(rig, "crouch-back", 1.1f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 1.1f * kTwoPi);
        p.r[N_LEG_R] = rx(-0.3f * s + 0.2f);
        p.r[N_LEG_L] = rx(0.3f * s + 0.2f);
        p.r[N_ARM_R] = rx(0.1f * s - 0.08f);
        p.r[N_ARM_L] = rx(-0.1f * s - 0.08f);
        p.r[N_TORSO] = rx(-0.08f) * ry(-0.04f * s);
        p.r[N_HEAD] = rx(-0.06f) * ry(0.03f * s);
        p.rootT = {0, -0.45f * P - 0.1f * P * std::fabs(s), 0};
    }));
    // Faca à frente: passada compacta e mão de ataque sempre pronta.
    clips.push_back(makeClip(rig, "knife-walk", 0.8f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 0.8f * kTwoPi);
        p.r[N_LEG_R] = rx(0.5f * s);
        p.r[N_LEG_L] = rx(-0.5f * s);
        p.r[N_ARM_R] = rz(-0.3f) * rx(-1.0f + 0.08f * s);
        p.r[N_ARM_L] = rz(0.08f) * rx(0.35f * s - 0.18f);
        p.r[N_TORSO] = ry(0.08f * s);
        p.rootT = {0, -0.25f * P * std::fabs(s), 0};
    }));
    clips.push_back(makeClip(rig, "knife-back", 0.8f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 0.8f * kTwoPi);
        p.r[N_LEG_R] = rx(-0.42f * s);
        p.r[N_LEG_L] = rx(0.42f * s);
        p.r[N_ARM_R] = rz(-0.3f) * rx(-0.95f + 0.06f * s);
        p.r[N_ARM_L] = rz(0.08f) * rx(-0.25f * s - 0.18f);
        p.r[N_TORSO] = rx(-0.06f) * ry(-0.05f * s);
        p.rootT = {0, -0.2f * P * std::fabs(s), 0};
    }));
    clips.push_back(makeClip(rig, "knife-sprint", 0.62f, [P](float t, KeyPose& p) {
        float s = std::sin(t / 0.62f * kTwoPi);
        p.r[N_LEG_R] = rx(0.86f * s - 0.1f);
        p.r[N_LEG_L] = rx(-0.86f * s - 0.1f);
        p.r[N_ARM_R] = rz(-0.36f) * rx(-0.9f + 0.1f * s);
        p.r[N_ARM_L] = rx(0.65f * s - 0.25f);
        p.r[N_TORSO] = rx(0.2f) * ry(0.1f * s);
        p.r[N_HEAD] = rx(-0.16f);
        p.rootT = {0, P * (0.8f * std::fabs(std::cos(t / 0.62f * kTwoPi)) - 0.4f), 0};
    }));
    // Passos laterais curtos: quadril e ombros compensam sem girar o personagem.
    for (int side : {-1, 1}) {
        const char* name = side < 0 ? "strafe-left" : "strafe-right";
        clips.push_back(makeClip(rig, name, 0.9f, [P, side](float t, KeyPose& p) {
            float ph = t / 0.9f * kTwoPi;
            float s = std::sin(ph);
            float liftR = std::max(0.0f, s), liftL = std::max(0.0f, -s);
            p.r[N_LEG_R] = rx(0.34f * liftR) * ry(side * 0.12f);
            p.r[N_LEG_L] = rx(0.34f * liftL) * ry(-side * 0.12f);
            p.r[N_ARM_R] = rx(-0.18f * s) * rz(-side * 0.05f);
            p.r[N_ARM_L] = rx(0.18f * s) * rz(side * 0.05f);
            p.r[N_TORSO] = rz(-side * 0.1f * s) * ry(side * 0.06f);
            p.r[N_HEAD] = rz(side * 0.025f * s);
            p.rootT = {side * 0.12f * P * s, 0.12f * P * std::fabs(s), 0};
        }));
    }
    // Correr: passadas largas, tronco inclinado para a frente, braços fortes e quique do corpo.
    clips.push_back(makeClip(rig, "sprint", 0.62f, [P](float t, KeyPose& p) {
        float ph = t / 0.62f * kTwoPi;
        float s = std::sin(ph);
        p.r[N_LEG_R] = rx(0.98f * s - 0.12f);
        p.r[N_LEG_L] = rx(-0.98f * s - 0.12f);
        p.r[N_ARM_R] = rx(-1.15f * s - 0.3f) * rz(-0.14f);
        p.r[N_ARM_L] = rx(1.15f * s - 0.3f) * rz(0.14f);
        p.r[N_TORSO] = rx(0.3f) * ry(0.12f * s);
        p.r[N_HEAD] = rx(-0.24f) * ry(-0.08f * s);
        p.rootT = {0, P * (1.1f * std::fabs(std::cos(ph)) - 0.6f), 0};
    }));
    // Morrer (substituído pela queda do Mixamo quando o arquivo está no APK): cai de costas com os braços abertos.
    clips.push_back(makeClip(rig, "die", 1.1f, [P](float t, KeyPose& p) {
        float k = smoothstepf(0.0f, 0.62f, t);
        float lift = std::sin(std::min(1.0f, t / 0.62f) * kPi) * 1.2f * P;
        p.r[N_ROOT] = rx(-1.48f * k);
        p.r[N_ARM_R] = rz(-0.9f * k) * rx(-0.6f * k);
        p.r[N_ARM_L] = rz(0.9f * k) * rx(-0.6f * k);
        p.r[N_LEG_R] = rx(-0.25f * k) * rz(-0.12f * k);
        p.r[N_LEG_L] = rx(0.1f * k) * rz(0.12f * k);
        p.r[N_HEAD] = rx(0.35f * k);
        p.rootT = {0, lift, 0};
    }));
    // Golpe dos zumbis: tronco gira e o braço desce num tapa.
    clips.push_back(makeClip(rig, "attack-melee-right", 0.6f, [](float t, KeyPose& p) {
        float k = t / 0.6f;
        float wind = smoothstepf(0.0f, 0.35f, k), strike = smoothstepf(0.35f, 0.6f, k), back = smoothstepf(0.7f, 1.0f, k);
        p.r[N_ARM_R] = rz(-0.2f) * rx(lerpf(-2.6f * wind, -0.5f, strike) * (1.0f - back));
        p.r[N_ARM_L] = rx(-0.4f);
        p.r[N_TORSO] = ry((-0.35f * wind + 0.6f * strike) * (1.0f - back));
    }));
    // Gestos do lobby.
    clips.push_back(makeClip(rig, "emote-yes", 1.2f, [](float t, KeyPose& p) {
        float env = std::sin(std::min(1.0f, t / 1.2f) * kPi);
        p.r[N_HEAD] = rx(0.32f * std::sin(t / 0.3f * kPi) * env);
        p.r[N_ARM_R] = rz(-0.35f * env) * rx(-2.7f * env + 0.25f * std::sin(t * 18.0f) * env);
        p.r[N_ARM_L] = rz(0.05f);
    }));
    clips.push_back(makeClip(rig, "interact-right", 0.9f, [](float t, KeyPose& p) {
        float env = std::sin(std::min(1.0f, t / 0.9f) * kPi);
        p.r[N_ARM_R] = rx(-1.35f * env);
        p.r[N_HEAD] = rx(0.2f * env);
    }));
    clips.push_back(makeClip(rig, "pick-up", 1.0f, [](float t, KeyPose& p) {
        float env = std::sin(std::min(1.0f, t) * kPi);
        p.r[N_TORSO] = rx(0.75f * env);
        p.r[N_HEAD] = rx(-0.2f * env);
        p.r[N_ARM_R] = rx(-0.7f * env);
        p.r[N_ARM_L] = rx(-0.7f * env);
    }));
    // Acenar: o braço direito sobe ao lado da cabeça e a mão balança algumas vezes.
    clips.push_back(makeClip(rig, "wave", 2.8f, [](float t, KeyPose& p) {
        float up = smoothstepf(0.0f, 0.35f, t) * (1.0f - smoothstepf(2.35f, 2.8f, t));
        float wave = std::sin((t - 0.35f) * kTwoPi * 2.2f) * smoothstepf(0.2f, 0.5f, t);
        p.r[N_ARM_R] = rz((-2.5f + 0.4f * wave) * up) * rx(-0.15f * up);
        p.r[N_ARM_L] = rz(0.06f);
        p.r[N_TORSO] = rz(0.05f * up);
        p.r[N_HEAD] = rz(-0.1f * up) * ry(0.08f * wave * up);
    }));
    // Dançar (substituída pela dança do Mixamo quando o arquivo está no APK).
    clips.push_back(makeClip(rig, "dance", 6.4f, [P](float t, KeyPose& p) {
        float env = smoothstepf(0.0f, 0.3f, t) * (1.0f - smoothstepf(6.0f, 6.4f, t));
        float hop = std::fabs(std::sin(t / 0.4f * kPi));
        float s = std::sin(t / 0.8f * kTwoPi);
        float m = smoothstepf(3.0f, 3.4f, t);
        p.r[N_ARM_R] = rz(lerpf(-0.25f, -2.7f + 0.35f * s, m) * env) * rx(lerpf(-1.2f - 0.8f * s, 0.0f, m) * env);
        p.r[N_ARM_L] = rz(lerpf(0.25f, 2.7f + 0.35f * s, m) * env) * rx(lerpf(-1.2f + 0.8f * s, 0.0f, m) * env);
        p.r[N_LEG_R] = rx(lerpf(-0.45f * std::max(0.0f, s), 0.12f * s, m) * env);
        p.r[N_LEG_L] = rx(lerpf(-0.45f * std::max(0.0f, -s), -0.12f * s, m) * env);
        p.r[N_TORSO] = ry(lerpf(0.25f * s, 0.0f, m) * env) * rz(lerpf(0.0f, 0.12f * s, m) * env);
        p.r[N_HEAD] = rx(0.12f * hop * env) * ry(lerpf(-0.2f * s, 0.0f, m) * env);
        p.rootT = {lerpf(0.0f, 0.9f * P * s, m) * env, 0.6f * P * hop * env, 0};
    }));
    return clips;
}

}  // namespace

Look Look::decode(uint32_t c) {
    Look l;
    l.skin = static_cast<int>(c & 7u);
    l.hairStyle = static_cast<int>((c >> 3) & 7u);
    l.hairColor = static_cast<int>((c >> 6) & 7u);
    l.eyes = static_cast<int>((c >> 9) & 3u);
    l.face = static_cast<int>((c >> 11) & 3u);
    l.top = static_cast<int>((c >> 13) & 7u);
    l.topColor = static_cast<int>((c >> 16) & 15u);
    l.pantsColor = static_cast<int>((c >> 20) & 7u);
    l.shoes = static_cast<int>((c >> 23) & 3u);
    l.hat = static_cast<int>((c >> 25) & 7u);
    l.shorts = ((c >> 28) & 1u) != 0;
    l.zombie = (c & kLookZombieBit) != 0;
    return l;
}

uint32_t Look::encode() const {
    uint32_t c = static_cast<uint32_t>(skin & 7) | (static_cast<uint32_t>(hairStyle & 7) << 3) | (static_cast<uint32_t>(hairColor & 7) << 6) |
                 (static_cast<uint32_t>(eyes & 3) << 9) | (static_cast<uint32_t>(face & 3) << 11) | (static_cast<uint32_t>(top & 7) << 13) |
                 (static_cast<uint32_t>(topColor & 15) << 16) | (static_cast<uint32_t>(pantsColor & 7) << 20) |
                 (static_cast<uint32_t>(shoes & 3) << 23) | (static_cast<uint32_t>(hat & 7) << 25) | (shorts ? 1u << 28 : 0u);
    return zombie ? (c | kLookZombieBit) : c;
}

uint32_t zombieLook(int variant, uint32_t seed) {
    Rng rng(hash2(variant, 77, seed));
    Look z;
    z.zombie = true;
    z.skin = variant & 7;
    static const int styles[] = {0, 1, 1, 2, 4, 6};
    z.hairStyle = styles[rng.rangeInt(0, 5)];
    z.hairColor = rng.chance(0.5f) ? 0 : rng.rangeInt(1, 4);
    static const int tops[] = {0, 0, 1, 2, 3, 4, 7};
    z.top = tops[rng.rangeInt(0, 6)];
    z.topColor = rng.rangeInt(0, 14);
    z.pantsColor = rng.rangeInt(0, 6);
    z.shoes = rng.rangeInt(0, 3);
    z.shorts = rng.chance(0.15f);
    z.face = rng.chance(0.2f) ? 1 : 0;
    return z.encode();
}

void buildBlockyCharacter(uint32_t code, CharacterAsset& out) {
    const Look l = Look::decode(code);
    std::vector<ModelNode> nodes;
    std::vector<ModelMesh> meshes;
    HumanoidRig rig;
    buildVoxelHumanoid(l, code, nodes, meshes, out.image, rig);
    std::vector<AnimClip> clips = buildClips(rig);
    if (!l.zombie) addHeroClips(nodes, clips);
    addMixamoClips(nodes, l.zombie, clips);
    out.model.build(std::move(nodes), std::move(meshes), std::move(clips), 32.0f * kBlockyPixel);
    addHumanoidAliases(out.model);
    out.pixelArt = true;
}

}  // namespace zz
