#include "game/battle_royale.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace zz::br;
    Inventory inv;
    assert(inv.count(RIFLE)==0 && inv.count(MEDKIT)==0);
    assert(inv.add(RIFLE,1)==1 && inv.add(AMMO_RIFLE,90)==90);
    assert(inv.take(AMMO_RIFLE,20) && inv.count(AMMO_RIFLE)==70);
    assert(!inv.take(AMMO_RIFLE,71) && inv.count(AMMO_RIFLE)==70);
    assert(inv.add(999,5)==0);
    Config config; config.waitSeconds=0;config.planeSeconds=3;config.zoneWait=1;config.zoneShrink=2;
    BattleRoyale game(config);
    const int fullLook=(1<<28)|(7<<25)|(3<<23)|12345;
    assert(game.join(0,0,fullLook,false) && game.join(1,1,22,false));
    assert(!game.join(0,1,0,false) && !game.join(50,0,0,false));
    InputCommand in; in.sequence=1;in.moveX=2;
    assert(!game.submitInput(0,in));
    in.moveX=0;in.yaw=NAN; assert(!game.submitInput(0,in));
    in.yaw=0;in.buttons=JUMP; assert(game.submitInput(0,in));
    assert(!game.submitInput(0,in));
    assert(game.players()[0].sequence==0); // Receipt is not acknowledgement of simulation.
    InputCommand later=in;later.sequence=2;later.buttons=0;assert(game.submitInput(0,later));
    game.step(.05f);assert(game.players()[0].sequence==1&&game.players()[0].flight==FREEFALL);
    game.step(.05f);assert(game.players()[0].sequence==2);
    game.setConnected(0,false);later.sequence=3;assert(!game.submitInput(0,later));game.setConnected(0,true);
    later.sequence=16777216;assert(!game.submitInput(0,later));
    for(int i=0;i<2000 && !game.ended();++i) game.step(.05f);
    assert(game.players()[0].body.position.x>=1 && game.players()[0].body.position.x<1439);
    assert(game.players()[0].body.position.y < 128);
    const auto zone=game.zone();
    assert(zone.radius>=0 && zone.x-zone.radius>=0 && zone.z-zone.radius>=0);
    assert(zone.x+zone.radius<=1440 && zone.z+zone.radius<=1440);
    // No client-authoritative position/damage/inventory exists in the input contract.
    auto snapshot=game.snapshot(0); assert(snapshot.size()>32 && snapshot[0]==1);
    assert((uint32_t(snapshot[26]) | (uint32_t(snapshot[54])<<16))==uint32_t(fullLook));
    assert(game.ended());
    const int winner=game.winner();
    for(int i=0;i<50;++i) game.step(.05f);
    assert(game.winner()==winner);
    Config squads; squads.teamSize=4;
    BattleRoyale teams(squads);
    for(int i=0;i<4;++i) assert(teams.join(i,0,i,false));
    assert(!teams.join(4,0,4,false));
    // Controlled server fixture: held revive must be continuous and cannot be accelerated by two allies.
    Config coop;coop.teamSize=4;coop.waitSeconds=0;coop.planeSeconds=.1f;
    BattleRoyale match(coop);
    assert(match.join(0,0,0,false)&&match.join(1,0,0,false)&&match.join(2,0,0,false)&&match.join(3,1,0,false));
    match.step(.05f);match.step(.05f);
    auto& actors=const_cast<std::array<PlayerState,50>&>(match.players());
    zz::Vec3 spot;
    bool found=false;
    for(int z=64;z<1300&&!found;z+=16)for(int x=64;x<1300&&!found;x+=16) {
        int h=match.map().heightAt(x,z);bool clear=true;
        for(int dz=-10;dz<=5&&clear;++dz)for(int dx=-4;dx<=4&&clear;++dx)
            if(match.map().heightAt(x+dx,z+dz)!=h||match.map().solidAt(float(x+dx),float(h),float(z+dz)))clear=false;
        if(clear){spot={float(x),float(h),float(z)};found=true;}
    }
    assert(found);
    // A fixed simulation clock produces the same elapsed time and flight from batched dt.
    Config clockConfig;clockConfig.waitSeconds=0;clockConfig.planeSeconds=100;
    BattleRoyale fine(clockConfig),batched(clockConfig);
    assert(fine.join(0,0,0,false)&&batched.join(0,0,0,false));
    for(int i=0;i<10;++i)fine.step(.01f);batched.step(.1f);
    assert(std::abs(fine.time()-.1f)<.0001f&&std::abs(fine.time()-batched.time())<.0001f);
    for(int i=0;i<4;++i){actors[i].body.position=spot+zz::Vec3(float(i%3),0,0);actors[i].body.velocity={};actors[i].flight=LANDED;}
    actors[1].down=true;actors[1].health=1;actors[1].bleed=30;
    uint32_t seq=1;
    auto tick=[&](uint32_t buttons,int ticks,bool second=false) {
        for(int i=0;i<ticks;++i){InputCommand c;c.sequence=seq++;c.buttons=buttons;c.target=1;match.submitInput(0,c);if(second){c.sequence=seq++;match.submitInput(2,c);}match.step(.05f);}
    };
    tick(REVIVE,60);tick(0,3);tick(REVIVE,42);
    assert(actors[1].down); // 3s + interruption +2.1s must not count as a full revive.
    tick(0,3);tick(REVIVE,55,true);assert(actors[1].down);
    tick(REVIVE,50,true);assert(!actors[1].down);
    // Two clients race for one item. Only one receives the server-owned loot entity.
    auto& groundLoot=const_cast<std::vector<LootEntity>&>(match.loot());groundLoot.clear();groundLoot.push_back({10000,RIFLE,1,spot,false});
    actors[0].inventory=Inventory{};actors[3].inventory=Inventory{};actors[3].body.position=spot+zz::Vec3(1,0,0);
    InputCommand a;a.sequence=seq++;a.buttons=INTERACT;InputCommand b=a;b.sequence=seq++;
    assert(match.submitInput(0,a)&&match.submitInput(3,b));match.step(.05f);
    assert(actors[0].inventory.count(RIFLE)+actors[3].inventory.count(RIFLE)==1);
    assert(groundLoot[0].taken);
    // A full inventory cannot destroy a nearby authoritative spawn.
    actors[1].body.position=spot+zz::Vec3(10,0,10);actors[2].body.position=spot+zz::Vec3(12,0,10);
    const Inventory saved=actors[0].inventory;actors[0].inventory=Inventory{};
    for(int i=0;i<12;++i)actors[0].inventory.slots[i]={MEDKIT,5};
    groundLoot.clear();groundLoot.push_back({10002,RIFLE,1,spot,false});
    actors[3].body.position=spot+zz::Vec3(10,0,0);
    a.sequence=seq++;a.buttons=INTERACT;match.submitInput(0,a);match.step(.05f);
    assert(!groundLoot[0].taken&&groundLoot[0].count==1);
    actors[0].inventory=saved;
    groundLoot.clear();groundLoot.push_back({10001,MEDKIT,1,spot,false});
    a.sequence=seq++;a.buttons=0;match.submitInput(0,a);match.step(.05f);
    assert(actors[0].inventory.count(MEDKIT)==1); // same proximity collection as the existing mission.
    actors[3].body.position=spot+zz::Vec3(10,0,0);
    a.sequence=seq++;a.buttons=DROP;a.dropItem=MEDKIT;a.dropCount=1;match.submitInput(0,a);match.step(.05f);
    a.sequence=seq++;a.buttons=0;a.dropCount=0;match.submitInput(0,a);match.step(.05f);
    assert(actors[0].inventory.count(MEDKIT)==0); // discarded item cannot immediately auto-return.
    // Gloo cannot overlap an actor, consumes once on valid placement, and blocks bullets.
    actors[0].inventory.add(ICE,2);actors[0].yaw=0;actors[0].input.yaw=0;
    actors[3].body.position=spot+zz::Vec3(0,0,-3.2f);
    a.sequence=seq++;a.buttons=PLACE_ICE;assert(match.submitInput(0,a));match.step(.05f);
    assert(match.walls().empty()&&actors[0].inventory.count(ICE)==2);
    actors[3].body.position=spot+zz::Vec3(0,0,-6);
    a.sequence=seq++;a.buttons=PLACE_ICE;match.submitInput(0,a);match.step(.05f);
    assert(match.walls().size()==1&&actors[0].inventory.count(ICE)==1);
    a.sequence=seq++;match.submitInput(0,a);match.step(.05f);
    assert(match.walls().size()==1&&actors[0].inventory.count(ICE)==1);
    actors[0].equipped=0;actors[0].ammo=30;actors[0].reload=0;actors[0].cooldown=0;
    const float hp=actors[3].health;
    a.sequence=seq++;a.buttons=FIRE;match.submitInput(0,a);match.step(.05f);
    assert(actors[3].health==hp&&match.walls()[0].health<250);
    const int loaded=int(actors[0].ammo);
    assert(actors[0].inventory.add(PISTOL,1)==1);
    int pistolSlot=-1;for(int i=0;i<24;++i)if(actors[0].inventory.slots[i].id==PISTOL)pistolSlot=i;
    assert(pistolSlot>=0);
    a.sequence=seq++;a.buttons=0;a.slot=pistolSlot;match.submitInput(0,a);match.step(.05f);
    a.sequence=seq++;a.slot=0;match.submitInput(0,a);match.step(.05f);
    assert(int(actors[0].ammo)==loaded); // changing guns must not destroy or duplicate loaded ammunition.
    assert(actors[0].inventory.add(RIFLE,1)==0);
    const auto privateView=match.snapshot(3);
    for(size_t i=0;i<size_t(privateView[16]);++i) {
        const size_t row=24+i*32;
        if(privateView[row]==0)assert(privateView[row+13]==0&&privateView[row+14]==0&&privateView[row+18]==0&&privateView[row+26]==-1);
    }
    // Team rank belongs to the whole team, including a member eliminated before the final duel.
    actors[2].alive=false;actors[2].placement=0;actors[3].health=1;
    auto& walls=const_cast<std::vector<IceWall>&>(match.walls());walls.clear();
    actors[0].cooldown=0;actors[0].reload=0;
    a.sequence=seq++;a.buttons=FIRE;a.slot=-1;match.submitInput(0,a);match.step(.05f);
    assert(match.ended()&&match.winner()==0);
    assert(actors[0].placement==1&&actors[1].placement==1&&actors[2].placement==1&&actors[3].placement==2);
    std::cout << "battle royale: inventory, replay/NaN guard, phases, zone, full cosmetic bits, continuous revive, loot race, gloo collision and bullet cover PASS\n";
}
