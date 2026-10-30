#pragma once
// The campaign's existing movement laws, shared with the authoritative dedicated simulation.
#include "../world/body_motion.h"
namespace zz::mission {
constexpr float kGravity = 28, kJumpVelocity = 8.6f, kStepHeight = 1.05f, kPlayerSpeed = 4.5f;
inline Vec3 movementWish(float mx, float my, float yaw, bool sprinting, bool crouching, float aim, bool water,
                         bool slowed, float multiplier, bool adrenaline) {
    const float mag = std::min(1.f, std::sqrt(mx * mx + my * my));
    float response = mag > 1e-3f ? std::pow(mag, 1.25f) : 0;
    if (sprinting)
        response = 1;
    const float speed = kPlayerSpeed * (sprinting ? 2.f : (mag > .9f ? 1.45f : 1.f)) *
                        (crouching ? .5f : 1.f) * lerpf(1.f, .6f, aim) * (water ? .6f : 1.f) *
                        (slowed ? .55f : 1.f) * multiplier * (adrenaline ? 1.3f : 1.f);
    Vec3 wish = rightFromYaw(yaw) * mx + forwardFromYaw(yaw) * my;
    return lengthXZ(wish) > 1e-3f ? normalize(wish) * (response * speed) : wish;
}
inline void accelerate(BodyState &body, const Vec3 &wish, float dt) {
    Vec3 hv(body.velocity.x, 0, body.velocity.z);
    float acceleration;
    if (!body.onGround && !body.inWater)
        acceleration = 3.5f;
    else if (lengthXZ(wish) < .1f)
        acceleration = 14;
    else if (dot(hv, wish) < 0)
        acceleration = 18;
    else
        acceleration = lengthXZ(wish) > lengthXZ(hv) ? 10 : 12;
    body.velocity.x = lerpf(body.velocity.x, wish.x, damp(acceleration, dt));
    body.velocity.z = lerpf(body.velocity.z, wish.z, damp(acceleration, dt));
}
template <class Query>
bool mantleDestination(const Query &world, const BodyState &body, const Vec3 &dir, Vec3 &destination) {
    if (lengthXZ(dir) < .5f)
        return false;
    const float hw = body.halfWidth - 1e-3f;
    const Vec3 probe = body.position + dir * (body.halfWidth + .35f);
    const int x = int(std::floor(probe.x)), z = int(std::floor(probe.z));
    if (x == int(std::floor(body.position.x)) && z == int(std::floor(body.position.z)))
        return false;
    const int feet = int(std::floor(body.position.y + .05f));
    int top = -1;
    for (int y = feet + 1; y <= feet + 3; ++y)
        if (world.solid(x, y - 1, z) && !world.solid(x, y, z) && !world.solid(x, y + 1, z)) {
            top = y;
            break;
        }
    if (top < 0 || float(top) - body.position.y < 1.05f)
        return false;
    Vec3 risen(body.position.x, top + .02f, body.position.z);
    if (world.boxCollides(risen + Vec3(-hw, 0, -hw), risen + Vec3(hw, 1.74f, hw)))
        return false;
    destination = {x + .5f, float(top), z + .5f};
    return !world.boxCollides(destination + Vec3(-hw, .02f, -hw), destination + Vec3(hw, 1.74f, hw));
}
struct Mantle {
    float time = -1, duration = 0;
    Vec3 from, to;
};
inline bool advanceMantle(BodyState &body, Mantle &mantle, float dt) {
    if (mantle.time < 0)
        return false;
    mantle.time += dt;
    float t = clampf(mantle.time / mantle.duration, 0, 1);
    float up = smoothstepf(0, .65f, t), across = smoothstepf(.4f, 1, t);
    body.position = {lerpf(mantle.from.x, mantle.to.x, across), lerpf(mantle.from.y, mantle.to.y + .02f, up),
                     lerpf(mantle.from.z, mantle.to.z, across)};
    body.velocity = {};
    if (t >= 1) {
        mantle.time = -1;
        body.onGround = true;
    }
    return true;
}

// State needed to replay the original movement laws at the server's fixed step.
struct GroundMotion {
    Mantle mantle;
    float coyote = 0, jumpBuffer = 0, wallPush = 0;
};
// Flight values are part of wire v1: freefall=1, chute=2, landed=3.
template <class Query>
void stepDescent(const Query &world, BodyState &body, int &flight, float mx, float mz, float yaw, bool open,
                 float altitude, float dt) {
    const float horiz = flight == 1 ? 17.f : 10.f;
    Vec3 wish = rightFromYaw(yaw) * mx + forwardFromYaw(yaw) * mz;
    if (lengthXZ(wish) > 1)
        wish = normalize(wish);
    body.velocity.x = wish.x * horiz;
    body.velocity.z = wish.z * horiz;
    if (flight == 1 && (altitude <= 30 || (open && altitude > 10)))
        flight = 2;
    body.velocity.y = flight == 2 ? -6.f : std::max(-45.f, body.velocity.y - 20 * dt);
    moveBodyAgainst(world, body, body.velocity * dt, 0);
    if (body.onGround) {
        flight = 3;
        body.velocity.y = 0;
    }
}
template <class Query>
float stepGroundMovement(const Query &world, BodyState &body, GroundMotion &motion, float mx, float my,
                         float yaw, bool sprint, bool crouch, bool jump, bool down, float dt) {
    if (!std::isfinite(dt) || dt <= 0 || dt > .1f)
        return 0;
    const float hw = body.halfWidth - .001f;
    const bool canStand =
        !world.boxCollides(body.position + Vec3(-hw, .02f, -hw), body.position + Vec3(hw, 1.75f, hw));
    if (jump && crouch && !down && canStand) {
        crouch = false;
        jump = false;
    }
    crouch = crouch || (!down && !canStand);
    body.height = down ? .55f : (crouch ? 1.2f : 1.75f);
    body.inWater =
        world.isWaterAt(body.position + Vec3(0, .6f, 0)) || world.isWaterAt(body.position + Vec3(0, .15f, 0));
    if (down)
        motion.mantle.time = -1;
    if (motion.mantle.time >= 0) {
        const Vec3 before = body.position;
        advanceMantle(body, motion.mantle, dt);
        if (world.boxCollides(body.position + Vec3(-hw, .001f, -hw),
                              body.position + Vec3(hw, body.height - .001f, hw))) {
            body.position = before;
            motion.mantle.time = -1;
            body.velocity = {};
        }
        if (motion.mantle.time < 0)
            motion.coyote = .12f;
        return 0;
    }
    if (sprint && mx * mx + my * my < .01f) {
        mx = 0;
        my = 1;
    }
    sprint = sprint && !crouch && !down;
    const Vec3 wish =
        movementWish(mx, my, yaw, sprint, crouch, 0, body.inWater, false, down ? 1.1f / 6.525f : 1.f, false);
    accelerate(body, wish, dt);
    motion.coyote = body.onGround ? .12f : std::max(0.f, motion.coyote - dt);
    motion.jumpBuffer = jump ? .15f : std::max(0.f, motion.jumpBuffer - dt);
    if (motion.jumpBuffer > 0 && !down && !crouch) {
        if (body.inWater) {
            body.velocity.y = 5;
            motion.jumpBuffer = 0;
        } else if (motion.coyote > 0 && body.velocity.y <= 1) {
            body.velocity.y = kJumpVelocity;
            motion.coyote = 0;
            motion.jumpBuffer = 0;
            body.onGround = false;
        }
    }
    if (body.inWater) {
        body.velocity.y -= 9 * dt;
        if (world.isWaterAt(body.position + Vec3(0, 1.3f, 0)))
            body.velocity.y += 17 * dt;
        body.velocity.y = clampf(body.velocity.y, -3, 5);
    } else
        body.velocity.y =
            std::max(-40.f, body.velocity.y - kGravity * (body.velocity.y < 0 ? 1.2f : 1.f) * dt);
    const float falling = body.velocity.y;
    const Vec3 before = body.position;
    body.stepOffset = 0;
    moveBodyAgainst(world, body, body.velocity * dt, down ? 0 : kStepHeight);
    const float wanted = lengthXZ(wish) * dt, moved = lengthXZ(body.position - before);
    if (!down && !crouch && !body.inWater && wanted > .004f && moved < wanted * .35f)
        motion.wallPush += dt;
    else
        motion.wallPush = std::max(0.f, motion.wallPush - 3 * dt);
    if (motion.wallPush > .22f || (motion.wallPush > .02f && jump)) {
        Vec3 dest;
        if (mantleDestination(world, body, normalize(Vec3(wish.x, 0, wish.z)), dest)) {
            motion.mantle = {0, .28f + .12f * (dest.y - body.position.y), body.position, dest};
            body.velocity = {};
            motion.wallPush = 0;
        }
    }
    return body.onGround && !body.inWater ? std::max(0.f, -falling) : 0;
}
} // namespace zz::mission
