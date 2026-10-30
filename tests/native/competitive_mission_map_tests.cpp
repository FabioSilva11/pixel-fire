#include "world/competitive_map.h"
#include "world/competitive_source.h"
#include "world/island.h"
#include <cassert>
#include <iostream>

int main() {
    zz::br::CompetitiveMap map;
    assert(map.missionTerrain());
    assert(map.size()==1440);
    zz::IslandGenerator original(zz::themeForStage(1),1,zz::br::kMapSeed);
    zz::br::configureCompetitiveMission(original);
    for(auto pair: {std::pair<int,int>{0,0},{12,18},{45,45},{67,39},{89,89}}) {
        zz::Chunk expected;expected.cx=pair.first+zz::br::kMissionOrigin/16;expected.cz=pair.second+zz::br::kMissionOrigin/16;
        original.generate(expected,128);
        std::vector<uint8_t> actual;map.copyChunk(pair.first,pair.second,actual);
        assert(actual==expected.blocks); // exact existing mission generator, including structures/caves/islands.
    }
    zz::CompetitiveMapSource source;
    zz::World finite;finite.reset(32,128,32);
    for(auto* c:finite.chunks())source.generate(*c,128);
    finite.rebuildDirty(100);
    assert(!finite.streaming()&&finite.chunks().size()==4);
    for(auto* c:finite.chunks())assert(c->generated&&c->meshed&&!c->dirty);
    // Moving the view cannot generate/unload anything: all finite chunks are ready before play.
    const auto chunks=finite.chunks().size();finite.stream(20,20,5,zz::StreamBudget::bulk());
    assert(finite.chunks().size()==chunks);
    std::cout<<"mission reuse: exact terrain/structures parity and finite full-preload PASS\n";
}
