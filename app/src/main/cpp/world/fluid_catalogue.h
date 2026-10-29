// Adaptador do catálogo requerido pelo algoritmo original de fluidos.
#pragma once
#include "blocks.h"
#include <array>
#include <string_view>
namespace zz::fluids {
struct Block {
    std::string_view name;
    uint32_t flags=0;
    int fluidKind=0, fluidLevel=0;
    bool solid=false;
};
inline const Block& block(uint16_t id) {
    static const auto data=[] {
        std::array<Block,B_COUNT> a{};
        for (int i=0;i<B_COUNT;++i) {
            a[i].name=i==B_AIR ? "air" : "solid";
            a[i].solid=blockInfo(uint8_t(i)).solid;
            if (blockInfo(uint8_t(i)).shape==Shape::Cross) a[i].flags=128;
            if (isWaterBlock(uint8_t(i))) {
                a[i].name=i==B_WATER ? "water" : "flowing_water";
                a[i].fluidKind=1;
                a[i].fluidLevel=waterLevel(uint8_t(i));
            }
        }
        return a;
    }();
    return data[id<B_COUNT ? id : 0];
}
inline int blockCount() { return B_COUNT; }
inline uint16_t blockId(std::string_view name) {
    return name=="water" ? B_WATER : name=="stone" ? B_STONE :
           name=="obsidian" ? B_OBSIDIAN : name=="cobblestone" ? B_COBBLE : B_AIR;
}
}
