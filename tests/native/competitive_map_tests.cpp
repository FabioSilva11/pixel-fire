#include "world/competitive_map.h"
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    zz::br::CompetitiveMap a, b;
    assert(a.size() == 1440 && a.regions().size() == 13);
    assert(a.buildings().size()==52 && a.collisionResident() && a.residentChunkCount()==8100);
    assert(a.hash() == b.hash() && a.hash() != 0);
    assert(a.blockAt(-1, 30, 0) == zz::B_BEDROCK);
    assert(a.blockAt(1440, 30, 0) == zz::B_BEDROCK);
    for (const auto& r : a.regions()) {
        assert(r.x > 0 && r.z > 0 && r.x < 1440 && r.z < 1440);
        assert(r.name && *r.name);
    }
    for (const auto& building : a.buildings()) {
        assert(building.rotation % 90 == 0);
        assert(building.x > 0 && building.z > 0);
        // Door is open, floor solid and roof present; same query feeds server collision and Android chunks.
        const auto door = a.doorPosition(building);
        assert(a.blockAt(int(door.x), int(door.y), int(door.z)) == zz::B_AIR);
        assert(zz::isSolid(a.blockAt(int(door.x), int(door.y)-1, int(door.z))));
    }
    std::stringstream data(std::ios::in | std::ios::out | std::ios::binary);
    a.save(data);
    zz::br::CompetitiveMap restored;
    assert(restored.load(data) && restored.hash() == a.hash());
    assert(restored.collisionResident()&&restored.residentChunkCount()==8100);
    std::stringstream corrupt("BROKEN");
    assert(!restored.load(corrupt));
    assert(restored.hash() == a.hash());
    for (int z=0; z<1440; z+=29) for(int x=0; x<1440; x+=31) {
        const int h=a.heightAt(x,z);
        assert(h >= 1 && h < 128);
        assert(zz::isSolid(a.blockAt(x,h-1,z)));
        assert(a.blockAt(x,127,z)==zz::B_AIR);
    }
    std::cout << "competitive map: deterministic, finite, 13 regions, doors, roundtrip, corruption checks PASS hash=" << a.hash() << "\n";
}
