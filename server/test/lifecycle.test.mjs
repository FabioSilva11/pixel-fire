import { test } from 'node:test';
import assert from 'node:assert/strict';
import { Matchmaker } from '../src/matchmaker.mjs';
import { PartyStore } from '../src/party.mjs';

function fixture(options={}) {
  let now=100,steps=0,inputs=0,destroyed=0;
  const native={mapHash:42,mapVersion:3,create:()=>({}),join:()=>true,connected:()=>{},step:()=>steps++,input:()=>{inputs++;return true;},snapshot:()=>new Float32Array(72),destroy:()=>destroyed++};
  const parties=new PartyStore(),m=new Matchmaker(native,parties,{now:()=>now,countdownMs:3000,readyTimeoutMs:180000,...options});
  const messages=new Map(),sockets=new Map();
  for(const id of ['a','b']){messages.set(id,[]);const ws={readyState:1,bufferedAmount:0,send:x=>messages.get(id).push(Buffer.isBuffer(x)?x:JSON.parse(x)),close:()=>{}};sockets.set(id,ws);m.connect(id,ws);}
  const room=m.createRoom([['a'],['b']],1,[],false);
  const ready=id=>m.ready(id,{protocol:1,mapHash:42,look:0});
  return {m,room,native,parties,messages,sockets,ready,advance:n=>{now+=n;},get steps(){return steps;},get inputs(){return inputs;},get destroyed(){return destroyed;}};
}

test('no authoritative clock or input before every human loads map and countdown completes',()=>{
  const f=fixture();f.m.tick();assert.equal(f.steps,0);
  assert.equal(f.m.input('a',new Float32Array([1,0,0,0,0,0,-1,-1,0,0,-1,-1])),false);
  assert.throws(()=>f.m.ready('a',{protocol:1,mapHash:99}));
  f.ready('a');f.advance(5000);f.m.tick();assert.equal(f.steps,0);
  f.ready('b');f.m.tick();assert.equal(f.steps,0);
  assert(f.messages.get('a').some(x=>x.type==='countdown'&&x.stage==='flight'));
  f.advance(3000);f.m.tick();assert.equal(f.steps,1);
});

test('load timeout aborts whole room and releases party without inventing result',()=>{
  const f=fixture();f.ready('a');f.advance(180001);f.m.tick();
  assert.equal(f.m.rooms.size,0);assert.equal(f.destroyed,1);assert.equal(f.steps,0);
  assert(f.messages.get('a').some(x=>x.type==='matchClosed'&&x.reason==='loadingTimeout'));
});

test('explicit forfeit returns to lobby and cannot reconnect into the abandoned slot',()=>{
  const f=fixture();let forfeited=-1;f.native.forfeit=(_,slot)=>{forfeited=slot;};
  f.ready('a');f.ready('b');f.advance(3000);f.m.tick();
  f.m.abandon('a');assert.equal(forfeited,0);assert(!f.m.assignments.has('a'));
  assert(!f.room.identities.includes('a'));assert.equal(f.room.participants.length,2);
  const ws={readyState:1,bufferedAmount:0,send:()=>{},close:()=>{}};f.m.connect('a',ws);
  assert.equal(f.m.clients.get('a').ready,false);assert(!f.m.assignments.has('a'));
});

test('slot sequence survives reconnect and stale socket cannot control replacement',()=>{
  const f=fixture();f.ready('a');f.ready('b');f.advance(3000);f.m.tick();
  const input=new Float32Array([1,0,0,0,0,0,-1,-1,0,0,-1,-1]);
  assert.equal(f.m.input('a',input,f.sockets.get('a')),true);
  assert.equal(f.m.input('a',input,f.sockets.get('a')),false);
  const replacement={readyState:1,bufferedAmount:0,send:()=>{},close:()=>{}};
  f.m.connect('a',replacement);input[0]=2;
  assert.equal(f.m.input('a',input,f.sockets.get('a')),false);
  assert.equal(f.m.input('a',input,replacement),false);
  f.ready('a');assert.equal(f.m.input('a',input,replacement),true);
  input[0]=1;assert.equal(f.m.input('a',input,replacement),false);
});

test('bounded drain rejects allocation and closes unfinished matches as aborted',()=>{
  const f=fixture();f.m.beginDrain(5000);assert.equal(f.m.draining,true);
  assert.throws(()=>f.m.enqueue('a',{teamSize:1,mapHash:42,protocol:1,look:0}));
  f.advance(5001);f.m.tick();assert.equal(f.m.rooms.size,0);
  assert(f.messages.get('b').some(x=>x.type==='matchClosed'&&x.reason==='serverDraining'));
});

test('repeated READY cannot restart flight countdown',()=>{
  const f=fixture();f.ready('a');f.ready('b');f.advance(2000);f.ready('a');f.advance(1000);f.m.tick();assert.equal(f.steps,1);
});

test('native join failure rolls back every assignment and destroys handle',()=>{
  const f=fixture();f.m.releaseRoom(f.room);f.native.join=()=>false;
  assert.throws(()=>f.m.createRoom([['a'],['b']],1,[],false));
  assert.equal(f.m.assignments.size,0);assert.equal(f.m.rooms.size,0);assert.equal(f.destroyed,2);
});

test('connection replacement during flight countdown restores loading barrier',()=>{
  const f=fixture();f.ready('a');f.ready('b');f.advance(2500);
  const replacement={readyState:1,bufferedAmount:0,send:()=>{},close:()=>{}};
  f.m.connect('a',replacement);f.advance(1000);f.m.tick();assert.equal(f.steps,0);
  f.ready('a');f.advance(3000);f.m.tick();assert.equal(f.steps,1);
});

test('result emitted once and recorded with stable identities only on native finish',()=>{
  const recorded=[],f=fixture({persistence:{recordResult:r=>recorded.push(r)}});
  f.native.results=()=>({winner:1,players:[{id:0,team:0,placement:2,kills:1},{id:1,team:1,placement:1,kills:2}]});
  f.native.snapshot=()=>{const s=new Float32Array(72);s[1]=3;return s;};
  f.ready('a');f.ready('b');f.advance(3000);f.m.tick();f.m.tick();f.m.tick();f.m.tick();
  assert.equal(recorded.length,1);assert.deepEqual(recorded[0].participants,[{identity:'a',slot:0},{identity:'b',slot:1}]);
  assert.equal(f.messages.get('a').filter(x=>x.type==='result').length,1);
});
