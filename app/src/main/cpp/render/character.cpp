#include "character.h"

#include <algorithm>
#include <cmath>

namespace zz {

void CharacterModel::build(std::vector<ModelNode> nodes, std::vector<ModelMesh> meshes, std::vector<AnimClip> clips, float height) {
    nodes_ = std::move(nodes);
    meshes_ = std::move(meshes);
    clips_ = std::move(clips);
    aliases_.clear();
    // Ordem topológica: pais antes dos filhos.
    order_.clear();
    std::vector<int> depth(nodes_.size(), 0);
    for (size_t i = 0; i < nodes_.size(); ++i) {
        int d = 0, p = nodes_[i].parent;
        while (p >= 0 && d < 64) { ++d; p = nodes_[p].parent; }
        depth[i] = d;
    }
    for (int d = 0; d < 64; ++d)
        for (size_t i = 0; i < nodes_.size(); ++i)
            if (depth[i] == d) order_.push_back(static_cast<int>(i));

    // Altura na pose de bind.
    Pose pose;
    bindPose(pose);
    std::vector<Mat4> g;
    globals(pose, g);
    float minY = 1e9f, maxY = -1e9f;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].mesh < 0 || nodes_[i].mesh >= static_cast<int>(meshes_.size())) continue;
        const auto& verts = meshes_[static_cast<size_t>(nodes_[i].mesh)].vertices;
        for (size_t v = 0; v + 8 <= verts.size(); v += 8) {
            Vec3 p = g[i].transformPoint({verts[v], verts[v + 1], verts[v + 2]});
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    if (maxY > minY) {
        bindHeight_ = maxY - minY;
        bindMinY_ = minY;
    }
    if (height > 0) {
        bindHeight_ = height;
        bindMinY_ = 0;
    }
}

int CharacterModel::clipIndex(const std::string& name) const {
    for (size_t i = 0; i < clips_.size(); ++i)
        if (clips_[i].name == name) return static_cast<int>(i);
    return -1;
}

int CharacterModel::nodeIndex(const std::string& name) const {
    for (size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i].name == name) return static_cast<int>(i);
    for (const auto& a : aliases_)
        if (a.first == name) return a.second;
    return -1;
}

void CharacterModel::bindPose(Pose& out) const {
    out.t.resize(nodes_.size());
    out.r.resize(nodes_.size());
    out.s.resize(nodes_.size());
    for (size_t i = 0; i < nodes_.size(); ++i) {
        out.t[i] = nodes_[i].translation;
        out.r[i] = nodes_[i].rotation;
        out.s[i] = nodes_[i].scale;
    }
}

void CharacterModel::sample(int clipIndex, float time, bool loop, Pose& pose) const {
    if (clipIndex < 0 || clipIndex >= static_cast<int>(clips_.size())) return;
    const AnimClip& clip = clips_[clipIndex];
    float t = time;
    if (clip.duration > 0) {
        if (loop) t = std::fmod(time, clip.duration);
        else t = std::min(time, clip.duration);
    }
    for (const auto& ch : clip.channels) {
        if (ch.node < 0 || ch.node >= static_cast<int>(nodes_.size()) || ch.times.empty()) continue;
        const int comps = ch.path == 1 ? 4 : 3;
        size_t n = ch.times.size();
        if (ch.values.size() < n * comps) continue;
        size_t k1 = 0;
        while (k1 < n && ch.times[k1] < t) ++k1;
        size_t k0 = k1 == 0 ? 0 : k1 - 1;
        if (k1 >= n) k1 = n - 1;
        float f = 0;
        if (k1 != k0 && !ch.step) {
            float span = ch.times[k1] - ch.times[k0];
            f = span > 1e-6f ? clampf((t - ch.times[k0]) / span, 0, 1) : 0;
        }
        const float* a = &ch.values[k0 * comps];
        const float* b = &ch.values[k1 * comps];
        if (ch.path == 1) {
            pose.r[ch.node] = slerp(Quat(a[0], a[1], a[2], a[3]), Quat(b[0], b[1], b[2], b[3]), f);
        } else {
            Vec3 v(lerpf(a[0], b[0], f), lerpf(a[1], b[1], f), lerpf(a[2], b[2], f));
            if (ch.path == 0) pose.t[ch.node] = v; else pose.s[ch.node] = v;
        }
    }
}

void CharacterModel::globals(const Pose& pose, std::vector<Mat4>& out) const {
    out.resize(nodes_.size());
    for (int i : order_) {
        Mat4 local = Mat4::trs(pose.t[i], pose.r[i], pose.s[i]);
        out[i] = nodes_[i].parent >= 0 ? out[nodes_[i].parent] * local : local;
    }
}

void blendPose(const Pose& a, const Pose& b, float w, Pose& out) {
    size_t n = std::min(a.t.size(), b.t.size());
    out.t.resize(n);
    out.r.resize(n);
    out.s.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out.t[i] = lerp(a.t[i], b.t[i], w);
        out.r[i] = slerp(a.r[i], b.r[i], w);
        out.s[i] = lerp(a.s[i], b.s[i], w);
    }
}

void Animator::play(int clip, bool loop, float fade, float speed) {
    if (clip == clip_ && loop == loop_) {
        speed_ = speed;
        return;
    }
    // Interruptions start at the last displayed blend, not its target clip.
    // Ordinary transitions still advance the outgoing clip while fading.
    fadeFromPose_ = fade > 0 && fade_ > 0 && !displayedPose_.t.empty();
    if (fadeFromPose_) fadeSourcePose_ = displayedPose_;
    prevClip_ = clip_;
    prevTime_ = time_;
    prevLoop_ = loop_;
    prevSpeed_ = speed_;
    clip_ = clip;
    time_ = 0;
    loop_ = loop;
    speed_ = speed;
    fadeDuration_ = fade;
    fade_ = prevClip_ >= 0 ? fade : 0;
}

void Animator::update(float dt) {
    time_ += dt * speed_;
    prevTime_ += dt * prevSpeed_;
    if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt);
}

bool Animator::finished(const CharacterModel& model) const {
    if (clip_ < 0 || clip_ >= static_cast<int>(model.clips().size())) return true;
    return !loop_ && time_ >= model.clips()[clip_].duration;
}

void Animator::evaluate(const CharacterModel& model, Pose& out) {
    model.bindPose(scratchA_);
    model.sample(clip_, time_, loop_, scratchA_);
    if (fade_ > 0 && prevClip_ >= 0 && fadeDuration_ > 0) {
        if (fadeFromPose_) {
            blendPose(fadeSourcePose_, scratchA_, 1.0f - fade_ / fadeDuration_, out);
        } else {
            model.bindPose(scratchB_);
            model.sample(prevClip_, prevTime_, prevLoop_, scratchB_);
            blendPose(scratchB_, scratchA_, 1.0f - fade_ / fadeDuration_, out);
        }
    } else {
        out = scratchA_;
    }
    displayedPose_ = out;
}

}  // namespace zz
