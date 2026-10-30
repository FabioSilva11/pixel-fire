#include "world/horizonte_adapter.h"
#include "world/horizonte/block_mapping.h"
#include <cassert>
#include <iostream>
int main(){
    zz::horizonte::Terrain terrain(42);
    // This exact seed/column previously had an airborne stone mass at y61..70, above ground17.
    constexpr int x=316,z=28;
    assert(terrain.groundHeight(x,z)==17);
    auto chunk=terrain.chunk(x/16,z/16);
    for(int y=61;y<=70;++y)assert(zz::horizonte::toBlock(chunk[(y*16+(z&15))*16+(x&15)])==zz::B_AIR);
    assert(zz::isSolid(zz::horizonte::toBlock(chunk[(17*16+(z&15))*16+(x&15)])));
    std::cout<<"Ground terrain retained; former floating mountain column is clear PASS\n";
}
