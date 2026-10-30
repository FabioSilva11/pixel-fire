import { test } from 'node:test';
import assert from 'node:assert/strict';
import { PartyStore } from '../src/party.mjs';
import { Matchmaker } from '../src/matchmaker.mjs';
test('orphaned training room releases capacity after reconnect grace without awarding a win',()=>{
  const oldNow=Date.now;let now=100;Date.now=()=>now;
  try {
    const native={create:()=>({}),join:()=>true,connected:()=>{},step:()=>{},mapHash:1,snapshot:()=>new Float32Array(72)};
    const parties=new PartyStore(),m=new Matchmaker(native,parties);
    const ws={readyState:1,bufferedAmount:0,send:()=>{},close:()=>{}};
    m.connect('human',ws);const party=parties.create('human',1,true);parties.lock('human');
    m.createRoom([['human']],1,[party.code],true);assert.equal(m.rooms.size,1);
    m.disconnect('human',ws);m.tick();now=29999;m.tick();assert.equal(m.rooms.size,1);
    now=30101;m.tick();assert.equal(m.rooms.size,0);assert(!m.assignments.has('human'));assert.equal(parties.forPlayer('human').locked,false);
  } finally {Date.now=oldNow;}
});
