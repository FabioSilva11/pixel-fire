#pragma once
#include "world.h"
#include <functional>
namespace zz {
using TerrainProtection = std::function<bool(int,int,int)>;
using TerrainChanged = std::function<void(int,int,int)>;
bool breakTerrainBlock(World&,int x,int y,int z,const TerrainProtection& protect={},const TerrainChanged& changed={});
int destroyTerrainSphere(World&,const Vec3&,float radius,const TerrainProtection& protect={},const TerrainChanged& changed={});
}
