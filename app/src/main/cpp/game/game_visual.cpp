#include "game.h"
#include "battle_royale.h"
#include <cmath>
#include <algorithm>
namespace zz {
namespace {
constexpr int IT_SHOTGUN = 4;
VoxelItem weaponVisual(int id) {
    switch (id) {
    case 1:
        return VoxelItem::Pistol;
    case 2:
        return VoxelItem::Smg;
    case 4:
        return VoxelItem::Shotgun;
    case 5:
        return VoxelItem::Sniper;
    case 6:
        return VoxelItem::Knife;
    default:
        return VoxelItem::Rifle;
    }
}

} // namespace
void Game::updateActor(Actor &actor, bool local, const InputState &input, float dt) {
    const auto &v = actor.state;
    uint32_t look = uint32_t(v[2]) | (uint32_t(v[30]) << 16);
    auto *asset = character(look);
    HeroMotion motion;
    motion.velocity = local ? visualPlayer_.velocity : Vec3(v[27], v[28], v[29]);
    motion.yaw = local ? camYaw_ : v[7];
    motion.aim = local && input.aim ? 1.f : 0.f;
    motion.sprintMode = local ? (sprinting_ ? 1 : -1) : ((int(v[3]) & 32) ? 1 : -1);
    motion.alive = (int(v[3]) & 1) != 0;
    motion.crouched = local ? crouching_ : (int(v[3]) & 16) != 0;
    motion.armed = v[12] > 0;
    motion.knife = v[12] == br::KNIFE || actor.meleeTime >= 0;
    motion.melee = actor.meleeTime >= 0;
    motion.shooting = v[22] > .01f || (local && firingNow_);
    Vec3 pos = local ? visualPlayer_.position : actor.position;
    motion.inWater = world_.isWaterAt(pos + Vec3(0, .5f, 0));
    motion.onGround = local ? visualPlayer_.onGround
                            : CollisionQuery{*this}.boxCollides(pos + Vec3(-.2999f, -.05f, -.2999f),
                                                                pos + Vec3(.2999f, .001f, .2999f));
    if (int(v[11]) != br::LANDED)
        motion.onGround = int(v[11]) == br::IN_PLANE;
    motion.utility = local && (throwAiming_ || actor.throwTime >= 0);
    motion.reloading = v[14] > 0;
    animateHero(asset->model, actor.anim, actor.presentation, motion, dt);
    actor.deathTime = motion.alive ? 0 : actor.deathTime + dt;
    actor.kick = std::max(0.f, actor.kick - dt * (v[12] == br::SHOTGUN ? 4.f : 7.f));
    actor.swapTime = std::max(0.f, actor.swapTime - dt);
    actor.hitFlash = std::max(0.f, actor.hitFlash - dt);
    actor.barTimer = std::max(0.f, actor.barTimer - dt);
    if (actor.meleeTime >= 0) {
        actor.meleeTime += dt;
        if (actor.meleeTime > .38f)
            actor.meleeTime = -1;
    }
    if (actor.throwTime >= 0) {
        actor.throwTime += dt;
        if (actor.throwTime > .62f)
            actor.throwTime = -1;
    }
}
void Game::poseActor(Actor &actor, bool local, Pose &pose) {
    gunHold_.valid = false;
    if ((int(actor.state[3]) & 1) == 0)
        return;
    uint32_t look = uint32_t(actor.state[2]) | (uint32_t(actor.state[30]) << 16);
    const CharacterModel &m = character(look)->model;
    const bool playing = true, armed = actor.state[12] > 0 || actor.meleeTime >= 0 ||
                                       (local && this->throwAiming_) || actor.throwTime >= 0;
    const Vec3 actorVel = local ? visualPlayer_.velocity : Vec3(actor.state[27], actor.state[28], actor.state[29]);
    const float mantleTime_ = local ? motion_.mantle.time : -1.f;
    const float mantleDuration_ = local ? motion_.mantle.duration : 0.f;
    const float crouchBlend_ = actor.presentation.crouchBlend, airBlend_ = actor.presentation.airBlend;
    const float armedBlend_ = actor.presentation.armedBlend;
    const float gunAim_ = actor.presentation.gunAim;
    const float actorKick = local ? std::max(actor.kick, this->weaponKick_) : actor.kick;
    const float swapTimer_ = actor.swapTime;
    const float reloadTimer_ = actor.state[14], reloadDuration_ = br::weaponSpec(int(actor.state[12])).reload;
    const float meleeTime_ = actor.meleeTime, throwAnimTime_ = actor.throwTime;
    const bool throwAiming_ = local && this->throwAiming_;
    const float camPitch_ = local ? this->camPitch_ : actor.state[8];
    const bool grounded = local ? visualPlayer_.onGround : actor.presentation.airBlend < .1f;
    const int currentClip = actor.anim.clip();
    const bool authoredCrouch = currentClip == m.clipIndex("crouch-idle") ||
                                currentClip == m.clipIndex("crouch-walk") ||
                                currentClip == m.clipIndex("crouch-back");
    ikMove_ = clampf(lengthXZ(actorVel) / 6.0f, 0.0f, 1.0f);
    ikFalling_ = actorVel.y < -2.0f;
    if (playing && !authoredCrouch)
        poseCrouch(pose, m, crouchBlend_, clampf(lengthXZ(actorVel) / 2.0f, 0.0f, 1.0f));
    if (playing && mantleTime_ >= 0) {
        // Escalando: os dois braços se esticam para a borda e puxam o corpo.
        float k = clampf(mantleTime_ / std::max(0.05f, mantleDuration_), 0.0f, 1.0f);
        float reach = lerpf(-kPi * 0.95f, -kPi * 0.35f, k);
        int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left");
        if (armR >= 0)
            pose.r[armR] = Quat::axisAngle({0, 0, 1}, -0.15f) * Quat::axisAngle({1, 0, 0}, reach);
        if (armL >= 0)
            pose.r[armL] = Quat::axisAngle({0, 0, 1}, 0.15f) * Quat::axisAngle({1, 0, 0}, reach);
        return;
    }
    const float armedW = playing ? armedBlend_ : 1.0f;
    const bool authoredJump = playing && !grounded && currentClip == m.clipIndex("jump");
    if (playing && !authoredJump)
        poseAir(pose, m, airBlend_, !armed || armedW < 0.5f);
    if (int(actor.state[11]) == br::CHUTE) {
        int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left");
        if (armR >= 0)
            pose.r[armR] = Quat::axisAngle({0, 0, 1}, -0.22f) * Quat::axisAngle({1, 0, 0}, -2.45f);
        if (armL >= 0)
            pose.r[armL] = Quat::axisAngle({0, 0, 1}, 0.22f) * Quat::axisAngle({1, 0, 0}, -2.45f);
        return;
    }
    if (!armed)
        return;
    if (playing && (throwAiming_ || throwAnimTime_ >= 0.0f)) {
        const int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left"),
                  torso = m.nodeIndex("torso");
        if (throwAiming_) {
            if (armR >= 0)
                pose.r[armR] = Quat::axisAngle({0, 0, 1}, -0.28f) * Quat::axisAngle({1, 0, 0}, -2.35f);
            if (armL >= 0)
                pose.r[armL] = Quat::axisAngle({0, 1, 0}, -0.42f) * Quat::axisAngle({1, 0, 0}, -1.55f);
            if (torso >= 0)
                pose.r[torso] = pose.r[torso] * Quat::axisAngle({0, 1, 0}, 0.08f);
        } else {
            const float t = throwAnimTime_;
            const float release = smoothstepf(0.12f, 0.42f, t);
            const float follow = smoothstepf(0.38f, 0.62f, t);
            const float armAngle = lerpf(-2.35f, -0.42f, release) + 0.1f * follow;
            if (armR >= 0)
                pose.r[armR] = Quat::axisAngle({0, 0, 1}, lerpf(-0.28f, -0.04f, release)) *
                               Quat::axisAngle({1, 0, 0}, armAngle);
            if (armL >= 0)
                pose.r[armL] = Quat::axisAngle({0, 1, 0}, -0.4f + 0.28f * follow) *
                               Quat::axisAngle({1, 0, 0}, -1.45f + 0.55f * follow);
            if (torso >= 0)
                pose.r[torso] =
                    pose.r[torso] * Quat::axisAngle({0, 1, 0}, 0.22f * (1.0f - release) - 0.12f * follow);
        }
        return;
    }
    if (armedW <= 0.01f)
        return;
    float pitch = playing ? camPitch_ : -0.3f;
    const int weapon = playing ? int(actor.state[12]) : br::RIFLE;
    // Herói voxel: mira, coice, troca e recarga movem a arma; as mãos seguem pela IK (holdGun).
    const bool humanoid = m.nodeIndex("RightForeArm") >= 0;
    ikAim_ = playing ? gunAim_ : 0.0f;
    ikTwist_ = currentClip == m.clipIndex("aim-idle") ? 0.0f : 1.0f;
    ikKick_ = playing ? actorKick : 0.0f;
    ikDip_ = playing && swapTimer_ > 0 ? std::sin(clampf(swapTimer_ / 0.35f, 0.0f, 1.0f) * kPi) * 0.6f : 0.0f;
    ikReload_ = playing && reloadTimer_ > 0 ? 1.0f - reloadTimer_ / std::max(0.01f, reloadDuration_) : -1.0f;
    if (!(humanoid && (meleeTime_ >= 0 || weapon == br::KNIFE)))
        poseArmedBlend(pose, m, pitch, playing, 0.45f * (playing ? crouchBlend_ : 0.0f), armedW, weapon);
    if (!playing)
        return;
    int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left");
    if (reloadTimer_ > 0 && !humanoid) {
        // Recarga: a arma desce e gira, a mão esquerda vai ao pente e volta.
        float k = 1.0f - reloadTimer_ / std::max(0.01f, reloadDuration_);
        float dip = std::sin(k * kPi);
        if (armR >= 0)
            pose.r[armR] = Quat::axisAngle({0, 0, 1}, -0.35f * dip) *
                           Quat::axisAngle({1, 0, 0}, -kPi * 0.5f - pitch * 0.85f + 0.75f * dip);
        if (armL >= 0)
            pose.r[armL] = Quat::axisAngle({0, 1, 0}, -0.3f) *
                           Quat::axisAngle({1, 0, 0}, -kPi * 0.25f - 0.5f * std::sin(k * kTwoPi));
    }
    if (meleeTime_ >= 0 && armR >= 0) {
        // Golpe de faca: o braço sobe e corta para a frente.
        float wind = 1.0f - smoothstepf(0.0f, 0.14f, meleeTime_);
        float slash = smoothstepf(0.08f, 0.25f, meleeTime_);
        float follow = smoothstepf(0.25f, 0.38f, meleeTime_);
        pose.r[armR] = Quat::axisAngle({0, 1, 0}, 0.42f - 1.1f * slash) *
                       Quat::axisAngle({1, 0, 0}, -2.65f * wind - 0.55f * slash + 0.25f * follow);
        if (armL >= 0)
            pose.r[armL] =
                Quat::axisAngle({0, 1, 0}, 0.35f * slash) * Quat::axisAngle({1, 0, 0}, -1.1f + 0.4f * wind);
    }
}

void Game::addCharacter(CharacterAsset *asset, const Vec3 &pos, float yaw, float scale, const Pose &pose,
                        const Vec3 &tint, float flash, const Vec3 &scaleXYZ, float roll) {
    if (!asset)
        return;
    if (scene_.characterCount >= scene_.characters.size())
        scene_.characters.emplace_back();
    CharacterDraw &draw = scene_.characters[scene_.characterCount++];
    draw.asset = asset;
    float s = scale * 1.8f / std::max(0.1f, asset->model.bindHeight());
    draw.world = Mat4::translation(pos) * Mat4::rotationY(modelRotationFromYaw(yaw)) * Mat4::rotationZ(roll) *
                 Mat4::scale({s * scaleXYZ.x, s * scaleXYZ.y, s * scaleXYZ.z}) *
                 Mat4::translation({0, -asset->model.bindMinY(), 0});
    asset->model.globals(pose, draw.nodes);
    draw.tint = tint;
    draw.flash = flash;
    draw.flashColor = {1.0f, 0.25f, 0.2f};
    draw.alpha = 1.0f;
}
void Game::poseCrouch(Pose &pose, const CharacterModel &m, float blend, float moving) {
    if (blend <= 0.01f)
        return;
    const int legL = m.nodeIndex("leg-left"), legR = m.nodeIndex("leg-right"), torso = m.nodeIndex("torso"),
              head = m.nodeIndex("head");
    const float px = kBlockyPixel;
    if (torso >= 0)
        pose.r[torso] = slerp(pose.r[torso], pose.r[torso] * Quat::axisAngle({1, 0, 0}, 0.45f), blend);
    if (head >= 0)
        pose.r[head] = slerp(pose.r[head], pose.r[head] * Quat::axisAngle({1, 0, 0}, -0.4f), blend);
    for (int leg : {legL, legR}) {
        if (leg < 0)
            continue;
        // Passada menor e pernas recuadas sob o corpo inclinado.
        pose.r[leg] = slerp(pose.r[leg], Quat(), blend * (0.45f - 0.15f * moving));
        pose.t[leg].z -= 2.0f * px * blend;
    }
}
void Game::poseAir(Pose &pose, const CharacterModel &m, float blend, bool armsFree) {
    if (blend <= 0.01f)
        return;
    const int legL = m.nodeIndex("leg-left"), legR = m.nodeIndex("leg-right"), torso = m.nodeIndex("torso");
    const int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left");
    const bool falling = ikFalling_;
    if (legR >= 0)
        pose.r[legR] = slerp(pose.r[legR], Quat::axisAngle({1, 0, 0}, falling ? -0.25f : -0.75f), blend);
    if (legL >= 0)
        pose.r[legL] = slerp(pose.r[legL], Quat::axisAngle({1, 0, 0}, falling ? 0.2f : 0.4f), blend);
    if (torso >= 0)
        pose.r[torso] = slerp(pose.r[torso], pose.r[torso] * Quat::axisAngle({1, 0, 0}, 0.1f), blend);
    if (armsFree) {
        float up = falling ? -1.6f : -0.5f;
        if (armR >= 0)
            pose.r[armR] = slerp(pose.r[armR],
                                 Quat::axisAngle({0, 0, 1}, -0.55f) * Quat::axisAngle({1, 0, 0}, up), blend);
        if (armL >= 0)
            pose.r[armL] = slerp(pose.r[armL],
                                 Quat::axisAngle({0, 0, 1}, 0.55f) * Quat::axisAngle({1, 0, 0}, up), blend);
    }
}
void Game::poseArmedBlend(Pose &pose, const CharacterModel &m, float pitch, bool moveHead, float lean,
                          float weight, int weapon) {
    if (m.nodeIndex("RightForeArm") >= 0) {
        holdGun(pose, m, pitch, moveHead, weight, weapon);
        return;
    }
    int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left"), head = m.nodeIndex("head");
    float armPitch = pitch * 0.85f + lean;
    Quat aimR = Quat::axisAngle({0, 1, 0}, 0.08f) * Quat::axisAngle({1, 0, 0}, -kPi * 0.5f - armPitch);
    Quat aimL = Quat::axisAngle({0, 1, 0}, -0.5f) * Quat::axisAngle({1, 0, 0}, -kPi * 0.5f - armPitch + 0.1f);
    float move = ikMove_;
    float sway = std::sin(time_ * 9.0f) * move * 0.035f;
    float kick = ikKick_ * (weapon == IT_SHOTGUN ? 0.14f : 0.1f);
    aimR = Quat::axisAngle({0, 0, 1}, sway) * Quat::axisAngle({1, 0, 0}, kick) * aimR;
    aimL = Quat::axisAngle({0, 0, 1}, -sway * 0.55f) * aimL;
    if (armR >= 0)
        pose.r[armR] = slerp(pose.r[armR], aimR, weight);
    if (armL >= 0)
        pose.r[armL] = slerp(pose.r[armL], aimL, weight);
    if (head >= 0 && moveHead)
        pose.r[head] = slerp(pose.r[head], Quat::axisAngle({1, 0, 0}, -pitch * 0.5f - lean), weight);
}
void Game::addWeaponOnBack(const CharacterDraw &draw, int torso, int weapon) {
    const float px = kBlockyPixel;
    Mat4 frame = draw.world * draw.nodes[static_cast<size_t>(torso)] *
                 Mat4::translation({0.0f, 6.5f * px, -2.7f * px}) * Mat4::rotationZ(0.62f) *
                 Mat4::rotationX(-kPi * 0.5f) * Mat4::translation({0.0f, 0.0f, -0.35f});
    addWeaponParts(frame, weapon, nullptr);
}
namespace {

Quat conjugate(const Quat &q) { return {-q.x, -q.y, -q.z, q.w}; }
Vec3 rotateVec(const Quat &q, const Vec3 &v) { return Mat4::fromQuat(q).transformDir(v); }

// Quaternion da base ortonormal com colunas x, y, z.
Quat quatFromBasis(const Vec3 &x, const Vec3 &y, const Vec3 &z) {
    const float tr = x.x + y.y + z.z;
    Quat q;
    if (tr > 0) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    } else if (x.x > y.y && x.x > z.z) {
        const float s = std::sqrt(1.0f + x.x - y.y - z.z) * 2.0f;
        q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (y.y > z.z) {
        const float s = std::sqrt(1.0f + y.y - x.x - z.z) * 2.0f;
        q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1.0f + z.z - x.x - y.y) * 2.0f;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
    }
    return normalize(q);
}

// Rotação no espaço do modelo de um osso do braço (repouso apontando para baixo, frente em +Z) até `dir`,
// com a frente do osso virada para `front`.
Quat segmentRotation(const Vec3 &dir, const Vec3 &front) {
    const Vec3 d = normalize(dir);
    const Vec3 u = normalize(front - d * dot(front, d));
    const Vec3 w = cross(d, u);
    return quatFromBasis(w * -1.0f, d * -1.0f, u);
}

Quat globalRotation(const Pose &pose, const CharacterModel &m, int node) {
    Quat q;
    for (int n = node; n >= 0; n = m.nodes()[static_cast<size_t>(n)].parent)
        q = pose.r[static_cast<size_t>(n)] * q;
    return q;
}

// Mão em repouso: dedos para baixo (-Y), palma para o corpo, polegar para a frente (+Z). Na moldura da arma
// (z = cano, y = cima, x = lado esquerdo): a direita fecha na empunhadura com o polegar por cima; a esquerda
// segura o guarda-mão por baixo, dedos para o lado direito e polegar ao longo do cano.
const Quat kHandOnGrip = quatFromBasis({1, 0, 0}, {0, 0, -1}, {0, 1, 0});
const Quat kHandOnForegrip = quatFromBasis({0, -1, 0}, {1, 0, 0}, {0, 0, 1});

} // namespace

// Herói voxel com as duas mãos na arma (como na página de teste tools/personagem-voxel): a arma fica presa ao
// peito — pronta, atravessada no corpo com o cano para baixo e para a esquerda, ou com a coronha no ombro
// apontando para onde a câmera mira — e os braços alcançam empunhadura e guarda-mão por IK de dois ossos.
void Game::holdGun(Pose &pose, const CharacterModel &m, float pitch, bool moveHead, float weight,
                   int weapon) {
    gunHold_.valid = false;
    const int spine = m.nodeIndex("Spine"), spine1 = m.nodeIndex("Spine1"), spine2 = m.nodeIndex("Spine2");
    const int neck = m.nodeIndex("Neck"), head = m.nodeIndex("Head");
    const int armR = m.nodeIndex("RightArm"), foreR = m.nodeIndex("RightForeArm"),
              handR = m.nodeIndex("RightHand");
    const int armL = m.nodeIndex("LeftArm"), foreL = m.nodeIndex("LeftForeArm"),
              handL = m.nodeIndex("LeftHand");
    if (weight <= 0.01f || spine2 < 0 || handR < 0 || handL < 0 || foreR < 0 || foreL < 0 || armR < 0 ||
        armL < 0)
        return;
    const float aim = clampf(ikAim_, 0.0f, 1.0f);
    const float twist = aim * ikTwist_;
    const float px = kBlockyPixel;
    const VoxelItem item = weaponVisual(weapon);
    auto ry = [](float a) { return Quat::axisAngle({0, 1, 0}, a); };
    auto rx = [](float a) { return Quat::axisAngle({1, 0, 0}, a); };
    auto rz = [](float a) { return Quat::axisAngle({0, 0, 1}, a); };
    // Mirando: tronco de lado (ombro esquerdo à frente) e a cabeça volta para olhar pela alça de mira.
    if (twist > 0.01f) {
        if (spine >= 0)
            pose.r[static_cast<size_t>(spine)] = pose.r[static_cast<size_t>(spine)] * ry(-0.30f * twist);
        if (spine1 >= 0)
            pose.r[static_cast<size_t>(spine1)] = pose.r[static_cast<size_t>(spine1)] * ry(-0.22f * twist);
        pose.r[static_cast<size_t>(spine2)] = pose.r[static_cast<size_t>(spine2)] * ry(-0.24f * twist);
        if (neck >= 0)
            pose.r[static_cast<size_t>(neck)] =
                pose.r[static_cast<size_t>(neck)] * ry(0.34f * twist) * rx(0.1f * twist);
        if (head >= 0)
            pose.r[static_cast<size_t>(head)] =
                pose.r[static_cast<size_t>(head)] * ry(0.4f * twist) * rx(0.18f * twist);
    }
    if (moveHead && head >= 0)
        pose.r[static_cast<size_t>(head)] = pose.r[static_cast<size_t>(head)] * rx(-pitch * 0.5f);

    m.globals(pose, ikGlobals_);
    const Vec3 chest = ikGlobals_[static_cast<size_t>(spine2)].transformPoint({0, 0, 0});
    const Quat chestQ = globalRotation(pose, m, spine2);
    auto gunFrame = [&](const Vec3 &dir, const Vec3 &upHint, const Vec3 &anchor, const Vec3 &local, Quat &q,
                        Vec3 &origin) {
        const Vec3 z = normalize(dir);
        const Vec3 y = normalize(upHint - z * dot(upHint, z));
        q = quatFromBasis(cross(y, z), y, z);
        origin = anchor - rotateVec(q, local);
    };
    Quat readyQ, aimQ;
    Vec3 readyO, aimO;
    gunFrame(rotateVec(chestQ, {0.9f, -0.36f, 0.25f}), rotateVec(chestQ, {0.0f, 1.0f, 0.55f}),
             chest + rotateVec(chestQ, Vec3(-2.0f, -3.5f, 3.4f) * px), voxelGrip(item), readyQ, readyO);
    const float p = clampf(pitch, -1.2f, 1.2f);
    gunFrame({0.0f, std::sin(p), std::cos(p)}, {0.0f, std::cos(p), -std::sin(p)},
             chest + rotateVec(chestQ, Vec3(-1.7f, 2.7f, 0.3f) * px), voxelButt(item), aimQ, aimO);
    Quat gq = slerp(readyQ, aimQ, aim);
    Vec3 go = lerp(readyO, aimO, aim);
    const Vec3 gx = rotateVec(gq, {1, 0, 0}), gz = rotateVec(gq, {0, 0, 1});
    // Coice (o cano sobe e a arma recua), troca de arma (desce) e recarga (inclina e baixa).
    const float kick = ikKick_ * (weapon == IT_SHOTGUN ? 0.16f : 0.1f);
    const float reload = ikReload_ >= 0 ? std::sin(clampf(ikReload_, 0.0f, 1.0f) * kPi) : 0.0f;
    gq = Quat::axisAngle(gz, -0.55f * reload) * Quat::axisAngle(gx, ikDip_ + 0.35f * reload - kick) * gq;
    go = go - gz * (kick * 0.3f) - Vec3(0.0f, 1.0f, 0.0f) * (reload * 1.4f * px);
    const Mat4 gunM = Mat4::trs(go, gq, {1, 1, 1});
    const Vec3 gripW = gunM.transformPoint(voxelGrip(item));
    Vec3 foreW = gunM.transformPoint(voxelForegrip(item));
    if (ikReload_ >= 0) {
        // A mão esquerda vai ao carregador (entre a empunhadura e o guarda-mão, embaixo) e volta.
        const Vec3 mag =
            gunM.transformPoint(lerp(voxelGrip(item), voxelForegrip(item), 0.55f) + Vec3(0.0f, -0.12f, 0.0f));
        foreW = lerp(foreW, mag, std::max(0.0f, std::sin(clampf(ikReload_, 0.0f, 1.0f) * kTwoPi)));
    }

    auto solve = [&](int arm, int fore, int hand, const Vec3 &contact, const Quat &handQ, const Vec3 &palm,
                     const Vec3 &poleModel) {
        const Quat orig[3] = {pose.r[static_cast<size_t>(arm)], pose.r[static_cast<size_t>(fore)],
                              pose.r[static_cast<size_t>(hand)]};
        const Vec3 wrist = contact - rotateVec(handQ, palm);
        const Vec3 S = ikGlobals_[static_cast<size_t>(arm)].transformPoint({0, 0, 0});
        const float a = length(pose.t[static_cast<size_t>(fore)]),
                    b = length(pose.t[static_cast<size_t>(hand)]);
        const Vec3 toT = wrist - S;
        const float dist = clampf(length(toT), 0.01f, a + b - 0.001f);
        const Vec3 dir = normalize(toT);
        const float cosA = clampf((a * a + dist * dist - b * b) / (2.0f * a * dist), -1.0f, 1.0f);
        const Vec3 pole = rotateVec(chestQ, poleModel);
        const Vec3 pn = normalize(pole - dir * dot(pole, dir));
        const Vec3 E = S + dir * (a * cosA) + pn * (a * std::sqrt(std::max(0.0f, 1.0f - cosA * cosA)));
        const Vec3 thumb = rotateVec(handQ, {0, 0, 1});
        const Quat parentQ = globalRotation(pose, m, m.nodes()[static_cast<size_t>(arm)].parent);
        const Quat armW = segmentRotation(E - S, thumb);
        const Quat foreW2 = segmentRotation(wrist - E, thumb);
        pose.r[static_cast<size_t>(arm)] = slerp(orig[0], conjugate(parentQ) * armW, weight);
        pose.r[static_cast<size_t>(fore)] = slerp(orig[1], conjugate(armW) * foreW2, weight);
        pose.r[static_cast<size_t>(hand)] = slerp(orig[2], conjugate(foreW2) * handQ, weight);
    };
    solve(armR, foreR, handR, gripW, gq * kHandOnGrip, Vec3(0.95f, -1.55f, 0.0f) * px,
          lerp(Vec3(-1.0f, -0.9f, -0.6f), Vec3(-1.0f, -0.7f, -0.4f), aim));
    solve(armL, foreL, handL, foreW, gq * kHandOnForegrip, Vec3(-0.75f, -1.45f, 0.1f) * px,
          lerp(Vec3(1.0f, -1.0f, -0.3f), Vec3(0.3f, -1.0f, -0.2f), aim));

    // Dedos fechados na empunhadura (o indicador direito fica no gatilho) e no guarda-mão.
    static const char *kFingerNames[] = {"Index", "Middle", "Ring", "Pinky"};
    for (int side = 0; side < 2; ++side) {
        const std::string hand = side == 0 ? "RightHand" : "LeftHand";
        const float sign = side == 0 ? 1.0f : -1.0f;
        for (int f = 0; f < 4; ++f) {
            const bool trigger = side == 0 && f == 0;
            const float amount[3] = {trigger ? 0.35f : 1.0f, trigger ? 0.55f : 1.15f,
                                     trigger ? 0.35f : 0.75f};
            for (int i = 0; i < 3; ++i) {
                const int n = m.nodeIndex(hand + kFingerNames[f] + std::to_string(i + 1));
                if (n >= 0)
                    pose.r[static_cast<size_t>(n)] =
                        slerp(pose.r[static_cast<size_t>(n)], rz(sign * amount[i]), weight);
            }
        }
        const float thumb[3] = {0.45f, 0.4f, 0.35f};
        for (int i = 0; i < 3; ++i) {
            const int n = m.nodeIndex(hand + "Thumb" + std::to_string(i + 1));
            if (n >= 0)
                pose.r[static_cast<size_t>(n)] =
                    slerp(pose.r[static_cast<size_t>(n)], ry(sign * thumb[i]), weight);
        }
    }
    gunHold_.frame = gunM;
    gunHold_.valid = weight > 0.5f;
}
void Game::addWeaponModel(const CharacterDraw &draw, int armR, int weapon, Vec3 *muzzle) {
    // Ao trocar de arma ela desce e volta.
    float dip = 0.f;
    Mat4 frame = draw.world * draw.nodes[static_cast<size_t>(armR)] * Mat4::translation({0.0f, -0.9f, 0.0f}) *
                 Mat4::rotationX(kPi * 0.5f + dip);
    // Herói voxel: a arma já está posicionada pela pose (as duas mãos foram presas a ela por IK).
    if (gunHold_.valid)
        frame = draw.world * gunHold_.frame;
    gunHold_.valid = false;
    addWeaponParts(frame, weapon, muzzle);
}
void Game::addWeaponParts(const Mat4 &frame, int weapon, Vec3 *muzzle) {
    const auto item = weaponVisual(weapon);
    addVoxelItem(item, frame);
    if (muzzle)
        *muzzle = frame.transformPoint(voxelMuzzle(item));
}
void Game::addVoxelItem(VoxelItem item, const Mat4 &world) {
    CharacterAsset &asset = voxelItems_[static_cast<size_t>(item)];
    if (scene_.characterCount >= scene_.characters.size())
        scene_.characters.emplace_back();
    CharacterDraw &draw = scene_.characters[scene_.characterCount++];
    draw.asset = &asset;
    draw.world = world;
    Pose bind;
    asset.model.bindPose(bind);
    asset.model.globals(bind, draw.nodes);
    draw.tint = {1, 1, 1};
    draw.flash = 0;
    draw.alpha = 1;
}
} // namespace zz
