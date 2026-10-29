#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <tuple>
namespace zz::fluids {
using WorldRead = std::function<uint16_t(int,int,int)>;
using WorldWrite = std::function<void(int,int,int,uint16_t)>;
struct FluidSimulation {
    using Pos = std::tuple<int,int,int>;
    std::deque<Pos> queue;
    std::set<Pos> queued;
    float clock=0, scanClock=0;
    float activeRadius=32;  // Área ativa ao redor do jogador.
    int tick=0;
    void enqueue(int x,int y,int z);
    void changed(int x,int y,int z);
    void update(const WorldRead&,const WorldWrite&,float dt,float px,float py,float pz);
};
}
