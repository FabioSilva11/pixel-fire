#include "game/competitive_wire.h"
#include <cassert>
#include <vector>
#include <cmath>
#include <iostream>
int main(){
    assert(!zz::br::clientInputActive(0)&&zz::br::clientInputActive(1)&&zz::br::clientInputActive(2)&&!zz::br::clientInputActive(3));
    int earlySequence=0;for(int tick=0;tick<1800;++tick)if(zz::br::clientInputActive(0))++earlySequence;
    assert(earlySequence==0); // A slower peer loading90s must not exhaust the server sequence jump window.
    std::vector<float> valid(72);valid[0]=1;
    assert(zz::br::validSnapshot(valid.data(),valid.size()));
    assert(!zz::br::validSnapshot(valid.data(),71));
    valid[16]=51;assert(!zz::br::validSnapshot(valid.data(),valid.size()));
    valid[16]=0;valid[4]=NAN;assert(!zz::br::validSnapshot(valid.data(),valid.size()));
    valid.assign(104,0);valid[0]=1;valid[16]=1;valid[24]=50;
    assert(!zz::br::validSnapshot(valid.data(),valid.size())); // invalid player id cannot index client storage.
    valid[24]=0;valid[27]=1;valid[35]=3;valid[50]=-1;
    assert(zz::br::validSnapshot(valid.data(),valid.size()));
    valid[42]=16777216;assert(!zz::br::validSnapshot(valid.data(),valid.size()));
    valid[42]=1;valid[72]=999;valid[73]=1;
    assert(!zz::br::validSnapshot(valid.data(),valid.size())); // unknown inventory item.
    valid[72]=1;valid[73]=2;assert(!zz::br::validSnapshot(valid.data(),valid.size())); // weapon stack duplication.
    std::cout<<"competitive snapshot bounds PASS\n";
}
