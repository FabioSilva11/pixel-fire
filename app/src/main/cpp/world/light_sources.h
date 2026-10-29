#pragma once
#include "world.h"
#include <algorithm>
#include <cmath>
namespace zz {
// O mundo finito guarda os efeitos no layout, fora dos metadados dos chunks.
inline void removeDestroyedLights(const World& world,std::vector<Vec3>& points,bool campfires=false) {
    points.erase(std::remove_if(points.begin(),points.end(),[&](const Vec3& p) {
        int x=int(std::floor(p.x)),z=int(std::floor(p.z));
        if (!world.columnReady(x,z)) return false;
        for(int y=std::max(0,int(p.y)-2);y<=std::min(world.height()-1,int(p.y)+1);++y) {
            uint8_t b=world.get(x,y,z);
            if (campfires ? b==B_CAMPFIRE : blockInfo(b).emissive) return false;
        }
        return true;
    }),points.end());
}
}
