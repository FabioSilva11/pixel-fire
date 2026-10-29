#include "render/humanoid_asset.h"
#include "render/humanoid.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace zz;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Bytes {
    std::vector<uint8_t> b;
    void u8(uint8_t x) { b.push_back(x); }
    void u16(uint16_t x) { u8(x); u8(x >> 8); }
    void u32(uint32_t x) { u16(x); u16(x >> 16); }
    void f(float x) { uint32_t n; std::memcpy(&n, &x, 4); u32(n); }
    void str(const char* s) { u16(std::strlen(s)); while (*s) u8(*s++); }
};
// A hand-derived ZVH1 fixture: one animated root, a single triangle and one white texel.
static std::vector<uint8_t> fixture(int parent = -1, int mesh = 0, float position = 0, uint16_t index = 2, bool badTime = false) {
    Bytes w; for (char c : {'Z','V','H','1'}) w.u8(c);
    for (uint32_t x : {1u,1u,1u,1u,1u}) w.u32(x); w.f(2.72f);
    w.str("Hips"); w.u32(parent); w.u32(mesh);
    for (float x : {position,0.f,0.f,0.f,0.f,0.f,1.f,1.f,1.f,1.f}) w.f(x);
    w.u32(3); w.u32(3);
    for (float x : {0.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f, 1.f,0.f,0.f,0.f,1.f,0.f,1.f,0.f, 0.f,1.f,0.f,0.f,1.f,0.f,0.f,1.f}) w.f(x);
    w.u16(0); w.u16(1); w.u16(index); for (int i=0;i<4;++i) w.u8(255);
    w.str("die"); w.f(1); w.u32(1); w.u16(0); w.u8(0); w.u8(0); w.u32(2);
    w.f(badTime ? 1.f : 0.f); w.f(badTime ? 0.f : 1.f);
    for (float x : {0.f,0.f,0.f,0.f,-.5f,0.f}) w.f(x);
    return w.b;
}
static void parserTests() {
    HumanoidAssetData d; std::string e;
    require(parseHumanoidAsset(fixture(), d, &e), "valid ZVH1 must load");
    require(d.nodes.size()==1 && d.meshes[0].indices[2]==2 && d.rgba.size()==4, "valid model payload preserved");
    CharacterModel m; m.build(d.nodes,d.meshes,d.clips,d.bindHeight);
    Animator a; a.play(m.clipIndex("die"),false,0); a.update(3); Pose p; a.evaluate(m,p);
    require(a.finished(m) && std::fabs(p.t[0].y+.5f)<1e-5f,"death samples its final pose after duration");
    a.play(m.clipIndex("die"),false,.1f); a.evaluate(m,p);
    require(a.time()==3 && std::fabs(p.t[0].y+.5f)<1e-5f,"repeated death selection must not restart");
    for (auto bad : {fixture(0), fixture(-1,1), fixture(-1,0,std::numeric_limits<float>::quiet_NaN()),fixture(-1,0,0,3),fixture(-1,0,0,2,true)}) {
        require(!parseHumanoidAsset(bad,d,&e) && d.nodes.empty() && !e.empty(),"invalid hierarchy/mesh/finite/index/time rejected transactionally");
    }
    auto valid=fixture();
    for(size_t n=0;n<valid.size();++n) {
        std::vector<uint8_t> truncated(valid.begin(),valid.begin()+n);
        require(!parseHumanoidAsset(truncated,d,&e),"every truncated asset must fail safely");
    }
    auto oversized=fixture(); oversized[4]=255; oversized[5]=255; oversized[6]=255; oversized[7]=127;
    require(!parseHumanoidAsset(oversized,d,&e),"oversized node count rejected before allocation");
}
#ifndef ZZ_HUMANOID_PARSER_ONLY
static void realAssetTests(const char* file) {
    std::ifstream f(file,std::ios::binary); require(bool(f),"real asset file required");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{}); HumanoidAssetData d; std::string e;
    require(parseHumanoidAsset(bytes,d,&e),e.c_str()); require(d.nodes.size()==67,"full 67-bone hierarchy retained");
    CharacterModel m; m.build(d.nodes,d.meshes,d.clips,d.bindHeight);
    for (const char* n : {"RightHand","LeftHand","RightForeArm","LeftLeg","RightHandPinky4","LeftHandThumb4","LeftEye","RightEye","HeadTop_End"}) require(humanoidBone(m,n)>=0,"real hands/fingers/end/eye bones required");
    HumanoidMotion state; state.dead=true; state.swimming=true;
    auto a=humanoidAnimation(m,state); require(a.clip==m.clipIndex("die")&&!a.loop,"death outranks swimming and is one shot");
    state.dead=false; state.airborne=true; a=humanoidAnimation(m,state); require(a.clip==m.clipIndex("swim")&&a.loop,"swimming outranks airborne state");
    state={}; state.enemy=true; state.speed=1; a=humanoidAnimation(m,state); require(a.clip==m.clipIndex("mutant-walk"),"enemy uses mutant walk");
    state.enemy=false; a=humanoidAnimation(m,state); require(a.clip==m.clipIndex("walk"),"player does not use mutant walk");
    state.speed=0; state.armed=true; a=humanoidAnimation(m,state); require(a.clip==m.clipIndex("aim-idle"),"stationary armed player uses imported aim idle");
    require(!humanoidSwimming(true,false)&&humanoidSwimming(true,true)&&!humanoidSwimming(false,true),"shallow water must not trigger swimming");
    for(const char* clip : {"idle","ready","aim","walk","sprint","dance","mutant-walk","swim","die","aim-idle"}) {
        int c=m.clipIndex(clip); require(c>=0,"all gameplay clip roles required");
        for(float t : {0.f,.37f,4.f,30.f}) { Pose p; m.bindPose(p); m.sample(c,t,false,p); std::vector<Mat4> g; m.globals(p,g); for(auto x:g) for(float v:x.m) require(std::isfinite(v),"sampled global transforms finite"); }
    }
    Pose pose; m.bindPose(pose); m.sample(m.clipIndex("ready"),.2f,true,pose);
    humanoidHoldGun(pose,m,false,0,.2f); std::vector<Mat4> g; m.globals(pose,g);
    // Item frame is derived from the real hand; grip contact must coincide with the closed palm.
    Mat4 gun=humanoidGunFromHand(m,g); const auto hand=g[humanoidBone(m,"RightHand")];
    Vec3 palm=hand.transformPoint({-1.55f*.085f,-.95f*.085f,0});
    require(length(gun.transformPoint({0,-.1208f,.1872f})-palm)<.003f,"native grip frame must match right palm");
    Mat4 target=humanoidGunFrame(m,pose,false,0,.2f); Vec3 expected=target.transformPoint({0,-.1208f,.1872f});
    require(length(expected-palm)<.025f,"two-link IK reaches prototype grip");
}
#endif
int main(int argc,char** argv) {
    try { parserTests();
#ifndef ZZ_HUMANOID_PARSER_ONLY
        require(argc>1,"pass the exported ZVH1 path to exercise the real rig"); realAssetTests(argv[1]);
#endif
        std::puts("humanoid parser/state/pose tests passed"); return 0; }
    catch(const std::exception& e){ std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
