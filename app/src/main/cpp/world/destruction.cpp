#include "destruction.h"
#include <algorithm>
#include <cmath>
namespace zz {
bool breakTerrainBlock(World& world,int x,int y,int z,const TerrainProtection& protect,const TerrainChanged& changed) {
    if (y<=0 || !world.inBounds(x,y,z) || !world.columnReady(x,z)) return false;
    uint8_t b=world.get(x,y,z);
    if (b==B_AIR || b==B_BEDROCK || b==B_PORTAL || b==B_OBSIDIAN || isWaterBlock(b)) return false;
    if (protect && protect(x,y,z)) return false;
    world.set(x,y,z,B_AIR);
    if (changed) changed(x,y,z);
    return true;
}
int destroyTerrainSphere(World& world,const Vec3& p,float radius,const TerrainProtection& protect,const TerrainChanged& changed) {
    if (!std::isfinite(radius) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || radius<=0) return 0;
    radius=std::min(8.0f,radius);
    int removed=0;
    for (int y=std::max(1,int(std::floor(p.y-radius)));y<=std::min(world.height()-1,int(std::floor(p.y+radius)));++y)
        for (int z=std::max(0,int(std::floor(p.z-radius)));z<=std::min(world.depth()-1,int(std::floor(p.z+radius)));++z)
            for (int x=std::max(0,int(std::floor(p.x-radius)));x<=std::min(world.width()-1,int(std::floor(p.x+radius)));++x) {
                Vec3 delta=Vec3(x+.5f,y+.5f,z+.5f)-p;
                if (dot(delta,delta)<=radius*radius && breakTerrainBlock(world,x,y,z,protect,changed)) ++removed;
            }
    return removed;
}
}
