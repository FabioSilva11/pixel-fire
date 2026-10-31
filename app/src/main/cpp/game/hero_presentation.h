#pragma once
#include "../render/character.h"
namespace zz {
struct HeroMotion {
    Vec3 velocity;
    float yaw = 0, aim = 0;
    int sprintMode = 0; // 0 = auto threshold, 1 = explicit sprint, -1 = explicit walk
    bool reloading = false, onGround = true, inWater = false, crouched = false, shooting = false,
         melee = false, armed = true, alive = true, knife = false, utility = false;
    int flight = 3; // 0 = IN_PLANE, 1 = FREEFALL, 2 = CHUTE, 3 = LANDED
};
struct HeroPresentation {
    float yaw = 0, lean = 0, gunAim = 0, aimHold = 0, armedBlend = 1, airBlend = 0, crouchBlend = 0;
    float freefallPitch = 0, chutePitch = 0;
    float flightTime = 0, chuteDeployTime = 0;
    bool initialized = false;
};
// Original hero locomotion and blends from main, independent of match authority.
inline int heroLocomotionClip(const CharacterModel &model, const HeroMotion &in, float yaw, bool sprinting) {
    float speed = lengthXZ(in.velocity);
    if (speed <= .5f)
        return model.clipIndex(in.crouched ? "crouch-idle" : "idle");
    float forward = dot(in.velocity, forwardFromYaw(yaw));
    float side = dot(in.velocity, rightFromYaw(yaw));
    if (sprinting)
        return model.clipIndex(in.knife ? "knife-sprint" : (forward < -.4f ? "run-back" : "sprint"));
    if (std::fabs(side) > std::fabs(forward) * 1.15f)
        return model.clipIndex(side < 0 ? "strafe-left" : "strafe-right");
    if (forward < -.4f)
        return model.clipIndex(in.knife ? "knife-back" : (in.crouched ? "crouch-back" : "walk-back"));
    return model.clipIndex(in.knife ? "knife-walk" : (in.crouched ? "crouch-walk" : "walk"));
}
inline void animateHero(const CharacterModel &model, Animator &anim, HeroPresentation &state,
                        const HeroMotion &in, float dt) {
    const float horizontal = lengthXZ(in.velocity);
    const bool inFlight = in.flight == 1 || in.flight == 2;
    if (in.flight == 1) { // FREEFALL
        state.flightTime += dt;
        state.chuteDeployTime = 0.f;
        // Postura horizontal de paraquedista: corpo inclina ~78 graus (1.36 rad)
        const float targetPitch = 1.36f;
        state.freefallPitch = lerpf(state.freefallPitch, targetPitch, damp(4.8f, dt));
    } else if (in.flight == 2) { // CHUTE
        state.flightTime += dt;
        state.chuteDeployTime += dt;
        // Transição suave para a postura vertical suspensa
        state.freefallPitch = lerpf(state.freefallPitch, 0.f, damp(5.5f, dt));
    } else { // LANDED or IN_PLANE
        state.freefallPitch = lerpf(state.freefallPitch, 0.f, damp(12.f, dt));
        state.flightTime = 0.f;
        state.chuteDeployTime = 0.f;
    }

    state.aimHold = in.shooting ? 1.f : std::max(0.f, state.aimHold - dt);
    state.gunAim = lerpf(state.gunAim, (in.aim > .3f || state.aimHold > 0) ? 1.f : 0.f, damp(10, dt));
    const bool combat = in.aim > .3f || in.shooting || in.melee || in.utility;
    float targetYaw;
    if (inFlight) {
        // No ar, orienta o corpo suavemente na direção de deslocamento se estiver se movendo, ou na mira
        targetYaw = horizontal > 0.8f ? yawFromDirection(in.velocity.x, in.velocity.z) : in.yaw;
    } else {
        targetYaw = combat || horizontal <= .6f ? in.yaw : yawFromDirection(in.velocity.x, in.velocity.z);
    }
    if (!state.initialized) {
        state.yaw = targetYaw;
        state.initialized = true;
    }
    // Stationary heroes retain their last facing until aiming or moving again.
    if (!combat && !inFlight && horizontal <= .6f)
        targetYaw = state.yaw;
    float previous = state.yaw;
    state.yaw = lerpAngle(state.yaw, targetYaw, damp(inFlight ? 9.f : (combat ? 22.f : 13.f), dt));
    float turnRate = wrapAngle(state.yaw - previous) / std::max(dt, 1e-3f);

    // Inclinação lateral (banking) nas curvas
    float leanTarget = 0.f;
    if (in.flight == 1) {
        leanTarget = clampf(turnRate * -0.25f, -0.45f, 0.45f);
    } else if (in.flight == 2) {
        leanTarget = clampf(turnRate * -0.22f, -0.35f, 0.35f);
    } else if (in.onGround) {
        leanTarget = clampf(turnRate * horizontal * .012f, -.16f, .16f) * (in.crouched ? .3f : 1.f);
    }
    state.lean = lerpf(state.lean, leanTarget, damp(8, dt));

    bool swimming = in.inWater && !in.onGround && model.clipIndex("swim") >= 0;
    bool sprinting = !in.crouched && !combat && !inFlight &&
                     (in.sprintMode != 0 ? (in.sprintMode > 0 && horizontal > 3.5f) : (horizontal > 7.2f));
    bool holster = inFlight || (((sprinting || in.utility) && !in.reloading && !in.inWater) || swimming);
    state.armedBlend = lerpf(state.armedBlend, holster ? 0.f : 1.f, damp(holster ? 7.f : 16.f, dt));
    state.airBlend =
        lerpf(state.airBlend, (!in.onGround && !in.inWater && !inFlight) ? 1.f : 0.f, damp(in.onGround ? 16.f : 7.f, dt));
    state.crouchBlend = lerpf(state.crouchBlend, in.crouched ? 1.f : 0.f, damp(12, dt));
    if (!in.alive) {
        anim.play(model.clipIndex("die"), false, .1f);
        anim.update(dt);
        return;
    }
    int jump = model.clipIndex("jump");
    bool jumping = !in.onGround && !in.inWater && !inFlight && jump >= 0;
    int clip = jumping ? jump : heroLocomotionClip(model, in, state.yaw, sprinting);
    if (inFlight)
        clip = model.clipIndex("idle"); // Postura base neutra para posing procedural de paraquedismo
    bool idle = clip == model.clipIndex("idle") || clip == model.clipIndex("crouch-idle");
    float speed = jumping || idle
                      ? 1.f
                      : (in.crouched ? std::max(.5f, horizontal / 2.2f)
                                     : (sprinting ? horizontal / 6.f : std::max(.6f, horizontal / 3.6f)));
    int aimIdle = model.clipIndex("aim-idle");
    if (swimming) {
        clip = model.clipIndex("swim");
        speed = 1;
    } else if (clip == model.clipIndex("idle") && aimIdle >= 0 && state.gunAim > .5f &&
               state.armedBlend > .5f && !in.knife && in.armed) {
        clip = aimIdle;
        speed = 1;
    }
    if (clip < 0)
        clip = model.clipIndex("idle");
    anim.play(clip, !jumping, jumping ? .1f : (sprinting ? .18f : .2f), speed);
    anim.update(dt);
}
} // namespace zz
