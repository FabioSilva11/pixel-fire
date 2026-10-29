#include "world/world.h"
#include <cstdio>
#include <cstdlib>
#include "world/fluids.h"
#include "world/world_clock.h"
#include "world/destruction.h"
#include "world/light_sources.h"
using namespace zz;
static void check(bool ok,const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
class FlatSource:public ChunkSource {
    void generate(Chunk& c,int height) override {
        c.blocks.assign(256*height,B_AIR);
        for(int y=0;y<10;++y) for(int i=0;i<256;++i) c.blocks[y*256+i]=B_STONE;
        for(auto& g:c.ground) g=10;
    }
};
static void persistentEdits() {
    FlatSource source;
    World w; w.resetStreaming(2048,128,2048,&source);
    for(int i=0;i<8;++i) w.stream(208,208,1,StreamBudget::bulk());
    check(w.get(208,9,208)==B_STONE,"fixture must have solid ground");
    w.set(208,9,208,B_AIR);
    check(w.groundHeight(208,208)==9,"crater must lower navigation floor");
    check(w.standHeight(208,208)==9,"crater must lower stand height");
    for(int i=0;i<30;++i) w.stream(1200,1200,1,StreamBudget::bulk());
    check(!w.chunkAt(13,13),"fixture must unload edited chunk");
    for(int i=0;i<8;++i) w.stream(208,208,1,StreamBudget::bulk());
    check(w.get(208,9,208)==B_AIR,"removed block must survive unload and regeneration");
    check(w.groundHeight(208,208)==9,"regenerated crater must keep lower floor");
    w.resetStreaming(2048,128,2048,&source);
    for(int i=0;i<8;++i) w.stream(208,208,1,StreamBudget::bulk());
    check(w.get(208,9,208)==B_STONE,"new match must reset destruction");
    std::puts("PASS: edits survive streaming and reset in new matches");
}
static void waterSurface() {
    World w; w.reset(16,128,16);
    w.set(5,10,5,B_WATER);
    check(w.isWaterAt({5.5f,10.5f,5.5f}),"water below surface must immerse");
    check(!w.isWaterAt({5.5f,10.95f,5.5f}),"air above water surface must not immerse");
    World flow; flow.reset(16,128,16);
    flow.set(4,10,4,B_WATER_FLOW_7);
    check(flow.isWaterAt({4.5f,10.0625f,4.5f}) && !flow.isWaterAt({4.5f,10.25f,4.5f}),"shallow flowing water must use its actual depth");
    flow.rebuildDirty(1);
    int highest=0;
    for (const auto& v:flow.chunks()[0]->mesh.water) highest=std::max(highest,int(v.y));
    check(highest==81,"last flowing level must render at one eighth of a block");
    World waterfall; waterfall.reset(16,128,16);
    waterfall.set(4,10,4,B_WATER_FALLING);
    waterfall.set(5,10,4,B_WATER_FLOW_1);
    waterfall.rebuildDirty(1);
    bool exposedStrip=false;
    const auto& vertices=waterfall.chunks()[0]->mesh.water;
    for(size_t i=0;i<vertices.size();i+=4) {
        bool sharedSide=true;
        int lo=10000,hi=0;
        for(size_t j=0;j<4;++j) {
            sharedSide=sharedSide && vertices[i+j].x==40;
            lo=std::min(lo,int(vertices[i+j].y));
            hi=std::max(hi,int(vertices[i+j].y));
        }
        if(sharedSide && lo==87 && hi==88) exposedStrip=true;
    }
    check(exposedStrip,"falling water must cover the exposed strip above adjacent shallow water");
}
static void fluidFlow() {
    World w; w.reset(32,128,32);
    for(int x=0;x<32;++x) for(int z=0;z<32;++z) w.set(x,0,z,B_STONE,false);
    w.set(12,5,12,B_WATER);
    fluids::FluidSimulation fluid;
    fluid.changed(12,5,12);
    auto read=[&](int x,int y,int z)->uint16_t { return w.inBounds(x,y,z)?w.get(x,y,z):B_BEDROCK; };
    auto write=[&](int x,int y,int z,uint16_t b) { w.set(x,y,z,uint8_t(b)); };
    for(int i=0;i<28;++i) fluid.update(read,write,.25f,12,4,12);
    check(isWaterBlock(w.get(12,1,12)),"source must fall to the floor");
    check(isWaterBlock(w.get(13,1,12)),"water must spread on supported floor");
    check(w.get(12,0,12)==B_STONE,"water must not replace solid floor");
    w.set(12,5,12,B_AIR); fluid.changed(12,5,12);
    for(int i=0;i<180;++i) fluid.update(read,write,.25f,12,4,12);
    check(w.get(12,4,12)==B_AIR,"falling water must retract when its source is removed");
    w.set(20,1,20,B_WATER); w.set(22,1,20,B_WATER); w.set(21,1,20,B_WATER_FLOW_1);
    fluid.changed(21,1,20);
    for(int i=0;i<20;++i) fluid.update(read,write,.25f,21,2,20);
    check(w.get(21,1,20)==B_WATER,"two supported source neighbors must renew a source");
    std::puts("PASS: water falling, spreading, retraction and source renewal");
}
static void destruction() {
    World w; w.reset(32,128,32);
    for(int y=0;y<10;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) w.set(x,y,z,B_STONE,false);
    w.setGroundHeight(8,8,10);
    check(breakTerrainBlock(w,8,9,8),"weapon impact must remove one terrain block");
    check(w.get(8,9,8)==B_AIR && w.get(8,8,8)==B_STONE,"one shot must remove only the impacted block");
    w.set(8,6,8,B_PORTAL); w.set(9,6,8,B_WATER);
    auto guard=[](int x,int y,int z) { return x==7 && y==6 && z==8; };
    int removed=destroyTerrainSphere(w,{8.5f,6.5f,8.5f},2,guard);
    check(removed>0,"grenade must carve a crater");
    check(w.get(8,6,8)==B_PORTAL && w.get(9,6,8)==B_WATER,"terrain destruction must preserve portal and water");
    check(w.get(7,6,8)==B_STONE && w.get(11,6,8)==B_STONE,"protected and out-of-radius blocks must remain");
    check(!breakTerrainBlock(w,8,0,8),"bottom world layer must remain intact");
    std::puts("PASS: gun impact, grenade sphere and essential blocks");
}
static void dayClock() {
    check(horizonte::dayTick(1200)==0,"day must wrap after 24000 ticks");
    check(horizonte::daylight(0)==1 && horizonte::daylight(600)==1,"first ten minutes must be daylight");
    check(horizonte::daylight(650)==.5f && horizonte::daylight(800)==0,"sunset and night must match original");
    check(horizonte::daylight(1150)==.5f,"dawn must match original");
    auto noon=horizonte::cycleSky(themeForStage(1).sky,300);
    auto midnight=horizonte::cycleSky(themeForStage(1).sky,900);
    check(noon.night==0 && midnight.night==1 && noon.sunDirection.y>.9f && midnight.sunDirection.y<-.9f,"sky lighting and sun must follow the original clock");
    std::puts("PASS: original 20-minute day/night cycle");
}
static void finiteLights() {
    World w; w.reset(16,128,16);
    w.set(4,8,4,B_LANTERN); w.set(6,8,4,B_CAMPFIRE); w.set(8,8,4,B_LANTERN);
    std::vector<Vec3> lights={{4.5f,8.6f,4.5f},{6.5f,8.8f,4.5f},{8.5f,8.6f,4.5f}};
    std::vector<Vec3> fires={{6.5f,9,4.5f}};
    removeDestroyedLights(w,lights); removeDestroyedLights(w,fires,true);
    check(lights.size()==3 && fires.size()==1,"intact finite light sources must remain visible");
    breakTerrainBlock(w,4,8,4); breakTerrainBlock(w,6,8,4);
    removeDestroyedLights(w,lights); removeDestroyedLights(w,fires,true);
    check(lights.size()==1 && lights[0].x==8.5f && fires.empty(),"destroyed finite lanterns and campfires must remove their scene effects");
    std::puts("PASS: finite arena light and campfire invalidation");
}
int main() { persistentEdits(); waterSurface(); fluidFlow(); destruction(); dayClock(); finiteLights(); }
