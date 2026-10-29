#include "game.h"
#include <algorithm>
#include <cmath>

namespace zz {
bool Game::protectedTerrain(int x,int y,int z) const {
    for (const auto& n : nests_)
        if (!n.destroyed && std::abs(x+.5f-n.core.x)<=2 && std::abs(z+.5f-n.core.z)<=2 && y<=n.core.y)
            return true;
    return false;
}

void Game::terrainEdited(int x,int y,int z) {
    fluids_.changed(x,y,z);
    navX_=navZ_=-1;
    navTimer_=0;
    minimapTimer_=0;
    if (island_ && !bigMap_.empty()) {
        int i=std::min(kBigMapSize-1,x*kBigMapSize/kIslandSize);
        int j=std::min(kBigMapSize-1,z*kBigMapSize/kIslandSize);
        int top=world_.topBlockY(x,z);
        bigMap_[size_t(j)*kBigMapSize+i]=0xFF000000u | blockInfo(top<0?B_AIR:world_.get(x,top,z)).mapColor;
    }
}

void Game::writeTerrain(int x,int y,int z,uint8_t block) {
    if (!world_.inBounds(x,y,z) || !world_.columnReady(x,z) || world_.get(x,y,z)==block) return;
    world_.set(x,y,z,block);
    terrainEdited(x,y,z);
}

bool Game::breakTerrain(int x,int y,int z) {
    if (mode_!=Mode::Mission) return false;
    return breakTerrainBlock(world_,x,y,z,
        [this](int a,int b,int c){ return protectedTerrain(a,b,c); },
        [this](int a,int b,int c){ terrainEdited(a,b,c); });
}

void Game::updateWater(float dt) {
    float x=player_.position.x,z=player_.position.z;
    fluids_.activeRadius=32;
    fluids_.update(
        [this](int a,int b,int c)->uint16_t {
            return world_.inBounds(a,b,c) && world_.columnReady(a,c) ? world_.get(a,b,c) : B_BEDROCK;
        },
        [this](int a,int b,int c,uint16_t block){ writeTerrain(a,b,c,uint8_t(block)); },
        dt,x,player_.position.y,z);
}
}
