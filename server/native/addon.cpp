#include <node_api.h>
#include "game/battle_royale.h"
#include <memory>
#include <stdexcept>
#include <cmath>
#include <cstring>

namespace {
using zz::br::BattleRoyale;
struct Holder { std::unique_ptr<BattleRoyale> game; int64_t accounted=0; };
napi_value fail(napi_env env,const char* message){napi_throw_error(env,nullptr,message);return nullptr;}
bool number(napi_env env,napi_value value,double& out){return napi_get_value_double(env,value,&out)==napi_ok&&std::isfinite(out);}
bool integer(double v){return std::floor(v)==v;}
BattleRoyale* handle(napi_env env,napi_value v){void* p=nullptr;if(napi_get_value_external(env,v,&p)!=napi_ok||!p)return nullptr;return static_cast<Holder*>(p)->game.get();}
napi_value create(napi_env env,napi_callback_info info) {
    size_t argc=6;napi_value argv[6];napi_get_cb_info(env,info,&argc,argv,nullptr,nullptr);
    double v[6]={1,1,15,50,55,35};for(size_t i=0;i<argc;++i)if(!number(env,argv[i],v[i]))return fail(env,"Invalid configuration");
    if((v[0]!=1&&v[0]!=2&&v[0]!=4)||!integer(v[1])||v[1]<0||v[1]>4294967295.0||v[2]<0||v[2]>300||v[3]<.1||v[3]>120||v[4]<.1||v[4]>300||v[5]<.1||v[5]>300)return fail(env,"Configuration out of range");
    zz::br::Config c;c.teamSize=int(v[0]);c.seed=uint32_t(v[1]);c.waitSeconds=float(v[2]);c.planeSeconds=float(v[3]);c.zoneWait=float(v[4]);c.zoneShrink=float(v[5]);
    napi_value out;auto holder=std::make_unique<Holder>();
    try {holder->game=std::make_unique<BattleRoyale>(c);} catch(const std::exception&){return fail(env,"Simulation allocation failed");}
    holder->accounted=2500000;
    int64_t adjusted;napi_adjust_external_memory(env,holder->accounted,&adjusted);
    napi_create_external(env,holder.release(),[](napi_env e,void* p,void*){auto* h=static_cast<Holder*>(p);int64_t adjusted;if(h->accounted)napi_adjust_external_memory(e,-h->accounted,&adjusted);delete h;},nullptr,&out);return out;
}
napi_value destroy(napi_env env,napi_callback_info info) {
    size_t argc=1;napi_value a[1];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);
    void* raw=nullptr;if(argc!=1||napi_get_value_external(env,a[0],&raw)!=napi_ok||!raw)return fail(env,"destroy handle");
    auto* holder=static_cast<Holder*>(raw);holder->game.reset();int64_t adjusted;
    if(holder->accounted){napi_adjust_external_memory(env,-holder->accounted,&adjusted);holder->accounted=0;}
    napi_value out;napi_get_undefined(env,&out);return out;
}
napi_value join(napi_env env,napi_callback_info info) {
    size_t argc=5;napi_value a[5];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=5)return fail(env,"join arguments");
    auto* game=handle(env,a[0]);double v[4];for(int i=0;i<4;++i)if(!number(env,a[i+1],v[i]))return fail(env,"join numeric argument");
    if(!game||!integer(v[0])||!integer(v[1])||!integer(v[2])||v[0]<0||v[0]>=50||v[1]<0||v[1]>=50||v[2]<0||v[2]>536870911||(v[3]!=0&&v[3]!=1))return fail(env,"join bounds");
    napi_value out;napi_get_boolean(env,game->join(int(v[0]),int(v[1]),int(v[2]),v[3]!=0),&out);return out;
}
napi_value input(napi_env env,napi_callback_info info) {
    size_t argc=3;napi_value a[3];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=3)return fail(env,"input arguments");
    auto* game=handle(env,a[0]);double id;if(!game||!number(env,a[1],id)||!integer(id)||id<0||id>=50)return fail(env,"input player");
    napi_typedarray_type type;size_t length;void* raw;napi_value buffer;size_t offset;
    if(napi_get_typedarray_info(env,a[2],&type,&length,&raw,&buffer,&offset)!=napi_ok||type!=napi_float32_array||length!=12)return fail(env,"input format");
    auto* v=static_cast<float*>(raw);for(int i=0;i<12;++i)if(!std::isfinite(v[i])||std::abs(v[i])>16777215)return fail(env,"input range");
    if(!integer(v[0])||v[0]<1||std::abs(v[1])>1.001||std::abs(v[2])>1.001||std::abs(v[3])>10000||std::abs(v[4])>1.55||
       !integer(v[5])||v[5]<0||v[5]>32767||!integer(v[6])||v[6]<-1||v[6]>=24||!integer(v[7])||v[7]<-1||v[7]>=50||
       !integer(v[8])||v[8]<0||v[8]>28||!integer(v[9])||v[9]<0||v[9]>10000||
       !((v[10]==-1&&v[11]==-1)||(v[10]>=0&&v[10]<1440&&v[11]>=0&&v[11]<1440)))return fail(env,"input bounds");
    zz::br::InputCommand in;in.sequence=uint32_t(v[0]);in.moveX=v[1];in.moveZ=v[2];in.yaw=v[3];in.pitch=v[4];in.buttons=uint32_t(v[5]);
    in.slot=int(v[6]);in.target=int(v[7]);in.dropItem=int(v[8]);in.dropCount=int(v[9]);in.markX=v[10];in.markZ=v[11];
    napi_value out;napi_get_boolean(env,game->submitInput(int(id),in),&out);return out;
}
napi_value step(napi_env env,napi_callback_info info) {
    size_t argc=2;napi_value a[2];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=2)return fail(env,"step arguments");
    auto* game=handle(env,a[0]);double dt;if(!game||!number(env,a[1],dt)||dt<=0||dt>.1)return fail(env,"step range");
    game->step(float(dt));napi_value out;napi_get_undefined(env,&out);return out;
}
napi_value snapshot(napi_env env,napi_callback_info info) {
    size_t argc=2;napi_value a[2];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=2)return fail(env,"snapshot arguments");
    auto* game=handle(env,a[0]);double viewer;if(!game||!number(env,a[1],viewer)||!integer(viewer)||viewer<-1||viewer>=50)return fail(env,"snapshot viewer");
    auto data=game->snapshot(int(viewer));napi_value buffer,out;void* dst;
    napi_create_arraybuffer(env,data.size()*sizeof(float),&dst,&buffer);std::memcpy(dst,data.data(),data.size()*sizeof(float));
    napi_create_typedarray(env,napi_float32_array,data.size(),buffer,0,&out);return out;
}
napi_value connected(napi_env env,napi_callback_info info) {
    size_t argc=3;napi_value a[3];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=3)return fail(env,"connected arguments");
    auto* game=handle(env,a[0]);double id,value;if(!game||!number(env,a[1],id)||!number(env,a[2],value)||!integer(id)||id<0||id>=50||(value!=0&&value!=1))return fail(env,"connected player");
    game->setConnected(int(id),value!=0);napi_value out;napi_get_undefined(env,&out);return out;
}
napi_value results(napi_env env,napi_callback_info info) {
    size_t argc=1;napi_value a[1];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);if(argc!=1)return fail(env,"results arguments");
    auto* game=handle(env,a[0]);if(!game||!game->ended())return fail(env,"Match has not finished");
    napi_value out,list,winner;napi_create_object(env,&out);napi_create_array(env,&list);napi_create_int32(env,game->winner(),&winner);napi_set_named_property(env,out,"winner",winner);
    uint32_t index=0;
    for(const auto& p:game->players())if(p.present){
        napi_value entry;napi_create_object(env,&entry);
        const char* names[]={"id","team","placement","kills","bot"};int values[]={p.id,p.team,p.placement,p.kills,p.bot?1:0};
        for(int i=0;i<5;++i){napi_value value;napi_create_int32(env,values[i],&value);napi_set_named_property(env,entry,names[i],value);}
        napi_set_element(env,list,index++,entry);
    }
    napi_set_named_property(env,out,"players",list);return out;
}
napi_value forfeit(napi_env env,napi_callback_info info) {
    size_t argc=2;napi_value a[2];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);
    if(argc!=2)return fail(env,"forfeit arguments");
    auto* game=handle(env,a[0]);double id;
    if(!game||!number(env,a[1],id)||!integer(id)||id<0||id>=50)return fail(env,"forfeit player");
    game->forfeit(int(id));napi_value out;napi_get_undefined(env,&out);return out;
}
napi_value effects(napi_env env,napi_callback_info info) {
    size_t argc=2;napi_value a[2];napi_get_cb_info(env,info,&argc,a,nullptr,nullptr);
    if(argc!=2)return fail(env,"effects arguments");
    auto* game=handle(env,a[0]);double viewer;
    if(!game||!number(env,a[1],viewer)||!integer(viewer)||viewer<0||viewer>=50)return fail(env,"effects viewer");
    const auto& player=game->players()[int(viewer)];
    std::vector<float> data={game->time(),0};
    for(const auto& g:game->grenades())if(zz::length(g.pos-player.body.position)<200) {
        data.insert(data.end(),{float(g.owner),float(g.kind),g.pos.x,g.pos.y,g.pos.z,g.fuse});data[1]+=1;
    }
    napi_value buffer,out;void* dst;napi_create_arraybuffer(env,data.size()*sizeof(float),&dst,&buffer);
    std::memcpy(dst,data.data(),data.size()*sizeof(float));napi_create_typedarray(env,napi_float32_array,data.size(),buffer,0,&out);return out;
}
napi_value init(napi_env env,napi_value exports) {
    napi_property_descriptor descriptors[]={
        {"create",nullptr,create,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"destroy",nullptr,destroy,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"join",nullptr,join,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"input",nullptr,input,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"step",nullptr,step,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"snapshot",nullptr,snapshot,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"connected",nullptr,connected,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"results",nullptr,results,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"forfeit",nullptr,forfeit,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"effects",nullptr,effects,nullptr,nullptr,nullptr,napi_default,nullptr},
    };
    napi_define_properties(env,exports,sizeof(descriptors)/sizeof(descriptors[0]),descriptors);
    zz::br::CompetitiveMap map;napi_value hash;napi_create_uint32(env,map.hash(),&hash);napi_set_named_property(env,exports,"mapHash",hash);
    napi_value version,size;napi_create_int32(env,zz::br::kMapVersion,&version);napi_set_named_property(env,exports,"mapVersion",version);
    napi_create_int32(env,zz::br::kMapSize,&size);napi_set_named_property(env,exports,"mapSize",size);
    return exports;
}
}
NAPI_MODULE(NODE_GYP_MODULE_NAME,init)
