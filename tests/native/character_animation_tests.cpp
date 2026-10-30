#include "render/character.h"
#include "game/hero_presentation.h"
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
static CharacterModel heroFixture() {
    CharacterModel model;
    std::vector<AnimClip> clips;
    for (const char* name : {"idle","walk","walk-back","strafe-left","strafe-right","sprint","run-back","crouch-idle","crouch-walk","crouch-back","jump","swim","aim-idle","die","knife-walk","knife-back","knife-sprint"}) {
        AnimChannel channel; channel.node=0;channel.times={0,10};channel.values={0,0,0,10,0,0};
        AnimClip clip;clip.name=name;clip.duration=10;clip.channels={channel};clips.push_back(clip);
    }
    model.build({ModelNode{}},{},clips);
    return model;
}
static void originalHeroBehavior() {
    auto model=heroFixture(); Animator anim;HeroPresentation state;HeroMotion in;
    in.velocity={4,0,0};in.yaw=0;
    animateHero(model,anim,state,in,.1f);
    near(state.yaw,kPi*.5f,"unarmed movement faces direction of travel rather than camera");
    in.velocity={0,0,-6.525f};in.sprintMode=-1;
    for(int i=0;i<60;++i)animateHero(model,anim,state,in,1.f/60);
    if(anim.clip()!=model.clipIndex("walk")||state.armedBlend<.95f)throw std::runtime_error("full-stick walk must stay in walk clip and keep weapon in hands");
    in.sprintMode=0;
    in.velocity={0,0,-9};
    for(int i=0;i<60;++i)animateHero(model,anim,state,in,1.f/60);
    if(anim.clip()!=model.clipIndex("sprint")||state.armedBlend>.05f)throw std::runtime_error("sprint uses original clip and holsters gun");
    in.knife=true;
    for(int i=0;i<60;++i)animateHero(model,anim,state,in,1.f/60);
    if(anim.clip()!=model.clipIndex("knife-sprint"))throw std::runtime_error("equipped knife keeps original sprint animation");
    in.knife=false;
    in.velocity={};in.aim=1;
    for(int i=0;i<60;++i)animateHero(model,anim,state,in,1.f/60);
    if(anim.clip()!=model.clipIndex("aim-idle")||state.armedBlend<.95f)throw std::runtime_error("aim idle restores authored Mixamo clip and gun");
    in.aim=0;in.inWater=true;in.onGround=false;
    animateHero(model,anim,state,in,.1f);
    if(anim.clip()!=model.clipIndex("swim"))throw std::runtime_error("swimming keeps authored animation");
    in.inWater=false;in.onGround=true;in.crouched=true;
    animateHero(model,anim,state,in,.1f);
    if(anim.clip()!=model.clipIndex("crouch-idle"))throw std::runtime_error("crouch idle keeps authored animation");
    in.crouched=false;in.onGround=false;
    animateHero(model,anim,state,in,.1f);anim.update(20);
    if(!anim.finished(model))throw std::runtime_error("jump animation must not loop in air");
    in.alive=false;animateHero(model,anim,state,in,.1f);
    if(anim.clip()!=model.clipIndex("die"))throw std::runtime_error("elimination plays authored death");
}
int main() {
    {auto model=fixture();Pose empty;model.sampleBind(0,1.f,true,empty);near(empty.t.at(0).x,1.f,"lobby sampling initializes fresh pose");
    if(empty.r.size()!=1||empty.s.size()!=1)throw std::runtime_error("incomplete fresh lobby pose");}
    try { originalHeroBehavior(); interruptedFade(); consecutiveSwitches(); negativeChannelNode(); }
    catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
    std::puts("Character animation tests passed");
}
