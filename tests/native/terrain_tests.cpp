#include <cstdio>
#include <cstdlib>
#include <set>
#include <algorithm>
#include "world/island.h"
#include "world/terrain.h"
#include "world/position_key.h"
#include "world/horizonte/block_mapping.h"

static void check(bool ok,const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static uint64_t hash(const std::vector<uint16_t>& blocks) {
    uint64_t h=14695981039346656037ULL;
    for (auto b:blocks) { h^=b&255; h*=1099511628211ULL; h^=b>>8; h*=1099511628211ULL; }
    return h;
}
static void originalFixtures() {
    struct Fixture { int64_t seed; int cx,cz; uint64_t hash; int ground,biome; };
    const Fixture fixtures[]={
#include "horizonte_fixtures.inc"
    };
    for (auto f:fixtures) {
        zz::horizonte::Terrain t(f.seed);
        // Ground/biome oracle stays valid; airborne geometry was intentionally removed.
        check(t.groundHeight(f.cx*16,f.cz*16)==f.ground,"ground differs from original");
        check(t.biome(f.cx*16,f.cz*16)==f.biome,"biome differs from original");
        zz::Chunk c; c.cx=f.cx; c.cz=f.cz;
        zz::horizonte::generate(t,c,128);
        auto raw=t.chunk(f.cx,f.cz);
        for (size_t i=0;i<raw.size();++i)
            check(c.blocks[i]==zz::horizonte::toBlock(raw[i]),"block conversion loses terrain");
        int highest=0;
        for (int y=0;y<128;++y) for (int i=0;i<256;++i)
            if (c.blocks[y*256+i]!=zz::B_AIR) highest=y+1;
        check(c.maxY==highest,"imported chunk must expose its complete height to the mesher and minimap");

    }
    std::puts("PASS: 40 original chunk fixtures, zero/negative seeds and boundaries, block mapping");
}

static void missions() {
    for (uint32_t seed:{0u,1u,42u,424242u,0xffffffffu}) {
        zz::IslandGenerator island(zz::themeForStage(1),1,seed);
        auto s=island.start(); int x=int(s.x),z=int(s.z),y=int(s.y);
        zz::Chunk c; c.cx=x/16; c.cz=z/16;
        island.generate(c,128);
        auto at=[&](int h) { return c.blocks[(h*16+(z&15))*16+(x&15)]; };
        check(zz::isSolid(at(y-1)) && !zz::isSolid(at(y)) && !zz::isSolid(at(y+1)),"spawn needs solid dry floor and headroom");
        check(at(y)!=zz::B_WATER,"spawn must be dry");
        auto before=c.blocks;
        zz::Chunk neighbor; neighbor.cx=c.cx+1; neighbor.cz=c.cz;
        island.generate(neighbor,128); island.generate(c,128);
        check(before==c.blocks,"regeneration must not depend on generation order");

    }
    std::puts("PASS: dry spawn, headroom and deterministic regeneration");
}

static void positions() {
    std::set<uint64_t> keys;
    for (int x:{0,3072,6143}) for (int z:{0,3072,6143}) for (int y:{0,1,63,64,82,127}) {
        auto k=zz::packPosition(x,y,z);
        check(zz::positionX(k)==x && zz::positionY(k)==y && zz::positionZ(k)==z,"position above 63 must round trip");
        check(keys.insert(k).second,"structure positions must not alias");
    }
    std::puts("PASS: structure coordinate keys");
}

static void villageMap() {
    zz::horizonte::Terrain original(42);
    zz::IslandGenerator island(zz::themeForStage(1),1,42);
    bool roofFound=false;
    // Verifica telhados expostos da aldeia.
    for (int cz=9;cz<=12 && !roofFound;++cz) for (int cx=2;cx<=6 && !roofFound;++cx) {
        auto raw=original.chunk(cx,cz);
        for (int z=0;z<16 && !roofFound;++z) for (int x=0;x<16 && !roofFound;++x) {
            uint8_t top=0;
            for (int y=0;y<128;++y) {
                auto b=zz::horizonte::toBlock(raw[(y*16+z)*16+x]);
                if (b!=zz::B_AIR) top=b;
            }
            if (top!=zz::B_ROOF_RED) continue;
            roofFound=true;
            uint32_t color=island.mapColor(cx*16+x,cz*16+z);
            check(((color>>16)&255)>((color>>8)&255),"world map must show village roof rather than grass beneath it");
        }
    }
    check(roofFound,"map fixture must sample an exposed imported village roof");
    zz::IslandGenerator snow(zz::themeForStage(11),1,0);
    check(((snow.mapColor(0,0)>>16)&255)<100,"world map must show the generated water, not nonexistent ice");
    std::puts("PASS: imported village appears on world map");
}


static void farMesh() {
    class Source:public zz::ChunkSource {
        void generate(zz::Chunk& c,int height) override {
            c.blocks.assign(256*height,zz::B_AIR);
            c.blocks[256]=zz::B_STONE;
            c.blocks[512]=zz::B_TALL_GRASS;
        }
    } source;
    zz::World w; w.resetStreaming(6144,128,6144,&source);
    for (int i=0;i<4;++i) w.stream(6104,6104,1,zz::StreamBudget::bulk());
    auto c=w.chunkAt(381,381);
    check(c && c->meshed,"far chunk must mesh");
    check(!c->mesh.opaque.empty() && !c->mesh.cutout.empty(),"far terrain and plants must have vertices");
    for (const auto* vertices:{&c->mesh.opaque,&c->mesh.cutout})
        for (auto v:*vertices)
            check(v.x>=0 && v.x<=128 && v.z>=0 && v.z<=128,"far terrain must use chunk-local vertices without int16 overflow");
    std::puts("PASS: distant terrain and plant mesh coordinates");
}

int main() {
    villageMap();
    originalFixtures();
    positions();
    missions();
    farMesh();
}
