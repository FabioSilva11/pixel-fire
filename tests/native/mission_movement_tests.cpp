#include "game/mission_movement.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    const auto forward=zz::mission::movementWish(0,1,0,false,false,0,false,false,1,false);
    assert(std::abs(forward.z+6.525f)<.001f&&std::abs(forward.x)<.001f);
    const auto sprint=zz::mission::movementWish(0,1,0,true,false,0,false,false,1,false);
    assert(std::abs(zz::length(sprint)-9)<.001f);
    const auto right=zz::mission::movementWish(1,0,0,false,false,0,false,false,1,false);
    assert(right.x>6.5f&&std::abs(right.z)<.001f);
    zz::BodyState a,b;a.onGround=b.onGround=true;
    for(int i=0;i<20;++i)zz::mission::accelerate(a,forward,.05f);
    for(int i=0;i<60;++i)zz::mission::accelerate(b,forward,1.f/60);
    assert(zz::length(a.velocity-b.velocity)<.001f);
    // Walking into a low ceiling cannot force the standing collider through it.
    struct Room {
        bool boxCollides(const zz::Vec3& mn,const zz::Vec3& mx) const {return mn.y<0||mx.y>1.3f;}
        bool solid(int,int y,int) const {return y<0||y>=2;}
        bool isWaterAt(const zz::Vec3&) const {return false;}
    } room;
    zz::BodyState crouched;crouched.position={2,0,2};crouched.height=1.2f;crouched.onGround=true;
    zz::mission::GroundMotion motion;
    zz::mission::stepGroundMovement(room,crouched,motion,0,0,0,false,false,false,false,.05f);
    assert(crouched.height==1.2f&&!room.boxCollides(crouched.position+zz::Vec3(-.3f,0,-.3f),crouched.position+zz::Vec3(.3f,crouched.height,.3f)));
    // A released jump remains buffered until the first grounded frame.
    zz::BodyState jumper;jumper.position={3,.1f,3};jumper.velocity.y=-3;
    struct Floor {
        bool boxCollides(const zz::Vec3& mn,const zz::Vec3&) const {return mn.y<0;}
        bool solid(int,int y,int) const {return y<0;}
        bool isWaterAt(const zz::Vec3&) const {return false;}
    } floor;
    zz::mission::GroundMotion jumpMotion;
    zz::mission::stepGroundMovement(floor,jumper,jumpMotion,0,0,0,false,false,true,false,.05f);
    zz::mission::stepGroundMovement(floor,jumper,jumpMotion,0,0,0,false,false,false,false,.05f);
    assert(jumper.velocity.y>0);
    std::cout<<"shared original movement: speed, direction, acceleration invariance PASS\n";
}
