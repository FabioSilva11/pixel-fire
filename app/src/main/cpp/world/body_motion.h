#pragma once
// Shared axis sweep and step solver used by campaign and dedicated BR simulation.
#include "world.h"
namespace zz {
namespace motion_detail {

template <class Query> bool overlaps(const Query &world, const BodyState &b, const Vec3 &p) {
    Vec3 mn(p.x - b.halfWidth, p.y, p.z - b.halfWidth);
    Vec3 mx(p.x + b.halfWidth, p.y + b.height, p.z + b.halfWidth);
    const float e = 1e-4f;
    return world.boxCollides(mn + Vec3(e, e, e), mx - Vec3(e, e, e));
}

// Move num eixo e resolve contra blocos; retorna true se colidiu.
template <class Query> bool moveAxis(const Query &world, BodyState &b, int axis, float amount) {
    if (amount == 0)
        return false;
    Vec3 p = b.position;
    p[axis] += amount;
    if (!overlaps(world, b, p)) {
        b.position = p;
        return false;
    }
    // Busca binária pela maior fração livre.
    float lo = 0, hi = 1;
    for (int i = 0; i < 10; ++i) {
        float mid = (lo + hi) * 0.5f;
        Vec3 q = b.position;
        q[axis] += amount * mid;
        if (overlaps(world, b, q))
            hi = mid;
        else
            lo = mid;
    }
    // Stop on the free side of the face; the overlap epsilon must not leave feet below a floor.
    b.position[axis] += std::copysign(std::max(0.f, std::abs(amount) * lo - 1e-4f), amount);
    return true;
}

} // namespace motion_detail

template <class Query>
void moveBodyAgainst(const Query &world, BodyState &body, const Vec3 &displacement, float stepHeight) {
    using motion_detail::moveAxis;
    using motion_detail::overlaps;
    body.hitWall = false;
    int steps = std::max(1, static_cast<int>(std::ceil(
                                std::max(std::fabs(displacement.x),
                                         std::max(std::fabs(displacement.y), std::fabs(displacement.z))) /
                                0.35f)));
    Vec3 d = displacement / static_cast<float>(steps);
    bool grounded = false;
    for (int s = 0; s < steps; ++s) {
        if (moveAxis(world, body, 1, d.y)) {
            if (d.y < 0)
                grounded = true;
            body.velocity.y = 0;
        }
        for (int axis = 0; axis <= 2; axis += 2) {
            float amount = d[axis];
            if (amount == 0)
                continue;
            Vec3 before = body.position;
            if (!moveAxis(world, body, axis, amount))
                continue;
            // Tenta subir um degrau de até stepHeight.
            bool stepped = false;
            if ((body.onGround || grounded) && stepHeight > 0) {
                Vec3 raised = before;
                raised.y += stepHeight;
                if (!overlaps(world, body, raised)) {
                    Vec3 target = raised;
                    target[axis] += amount;
                    if (!overlaps(world, body, target)) {
                        // Desce até apoiar no degrau.
                        BodyState probe = body;
                        probe.position = target;
                        moveAxis(world, probe, 1, -stepHeight);
                        body.stepOffset += probe.position.y - before.y;
                        body.position = probe.position;
                        stepped = true;
                    }
                }
            }
            if (!stepped)
                body.hitWall = true;
        }
    }
    body.onGround = grounded;
    if (!grounded && body.velocity.y <= 0) {
        // Checagem de contato com o chão (para pulo/animação).
        Vec3 probe = body.position;
        probe.y -= 0.05f;
        body.onGround = overlaps(world, body, probe);
    }
    Vec3 mid = body.position;
    mid.y += 0.6f;
    body.inWater = world.isWaterAt(mid) || world.isWaterAt(body.position + Vec3(0, 0.15f, 0));
}

} // namespace zz
