#include "render/character.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace zz;
static void near(float actual, float expected, const char* message) {
    if (std::fabs(actual - expected) > 1e-5f) {
        std::fprintf(stderr, "%s: expected %g, got %g\n", message, expected, actual);
        throw std::runtime_error(message);
    }
}
static CharacterModel fixture() {
    CharacterModel model;
    std::vector<AnimClip> clips;
    for (int i = 0; i < 3; ++i) {
        AnimChannel channel; channel.node = 0; channel.times = {0, 10};
        channel.values = {float(i * 10), 0, 0, float(i * 10 + 10), 0, 0};
        AnimClip clip; clip.duration = 10; clip.channels = {channel}; clips.push_back(clip);
    }
    model.build({ModelNode{}}, {}, clips);
    return model;
}
static void interruptedFade() {
    auto model = fixture(); Animator a; Pose p;
    a.play(0); a.evaluate(model, p);
    a.play(1, true, 1); a.update(.25f); a.evaluate(model, p);
    near(p.t[0].x, 2.75f, "ordinary fade advances both clips");
    a.play(2, true, 1); a.evaluate(model, p);
    near(p.t[0].x, 2.75f, "idle-walk-run interruption preserves displayed blend");
    a.update(.5f); a.evaluate(model, p);
    near(p.t[0].x, 11.625f, "interruption fades from captured pose");
    a.update(.5f); a.evaluate(model, p);
    near(p.t[0].x, 21, "fade completes at advancing target");
    a.update(.5f); a.evaluate(model, p);
    near(p.t[0].x, 21.5f, "target continues after fade");
}
static void consecutiveSwitches() {
    auto model = fixture(); Animator a; Pose p;
    a.play(0); a.evaluate(model, p);
    a.play(1, true, 1); a.play(2, true, 1); a.evaluate(model, p);
    near(p.t[0].x, 0, "consecutive switches retain displayed source");
    a.play(1, false, 0); a.evaluate(model, p);
    near(p.t[0].x, 10, "zero fade switches immediately");
    a.update(20); a.evaluate(model, p);
    near(p.t[0].x, 20, "non-looping target clamps at final frame");
    if (!a.finished(model)) throw std::runtime_error("non-looping target finishes");
}
static void negativeChannelNode() {
    CharacterModel model; AnimClip clip; AnimChannel channel;
    channel.node = -1; channel.times = {0}; channel.values = {100, 100, 100};
    clip.channels = {channel}; model.build({ModelNode{}}, {}, {clip});
    Pose p; model.bindPose(p); model.sample(0, 0, false, p);
    near(p.t[0].x, 0, "invalid negative channel is ignored");
}
int main() {
    try { interruptedFade(); consecutiveSwitches(); negativeChannelNode(); }
    catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
    std::puts("Character animation tests passed");
}
