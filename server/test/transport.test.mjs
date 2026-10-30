import { test } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { once } from 'node:events';
import WebSocket from 'ws';
import { attachTransport } from '../src/transport.mjs';

async function setup(t) {
  const server=http.createServer();let valid=true,ready=0;
  const wsById=new Map();
  const matchmaker={clients:wsById,draining:false,connect:(id,ws)=>wsById.set(id,{ws}),disconnect:(id,ws)=>{if(wsById.get(id)?.ws===ws)wsById.delete(id);},send:(id,message)=>wsById.get(id)?.ws.send(JSON.stringify(message)),ready:()=>ready++,enqueue:()=>{},cancel:()=>{}};
  const identity={resolve:token=>{if(token!=='valid'||!valid)throw new Error('expired');return {id:'a'};}};
  const wss=attachTransport(server,identity,matchmaker);server.listen(0,'127.0.0.1');await once(server,'listening');
  t.after(()=>{for(const ws of wss.clients)ws.terminate();wss.close();server.close();});
  const connect=async headers=>{const ws=new WebSocket(`ws://127.0.0.1:${server.address().port}/play`,{headers:{authorization:'Bearer valid',...headers}});t.after(()=>ws.terminate());return ws;};
  return {connect,matchmaker,setValid:x=>valid=x,get ready(){return ready;}};
}

test('READY transport calls map barrier instead of silently updating cosmetic look',async t=>{
  const f=await setup(t),ws=await f.connect();await once(ws,'open');
  ws.send(JSON.stringify({type:'ready',protocol:1,mapHash:42,look:0}));
  await new Promise(resolve=>setTimeout(resolve,30));assert.equal(f.ready,1);
});

test('native transport rejects browser origin by default',async t=>{
  const f=await setup(t),ws=await f.connect({origin:'https://untrusted.example'});ws.on('error',()=>{});
  const result=await Promise.race([once(ws,'unexpected-response'),once(ws,'open').then(()=>['opened'])]);
  if(result[1])result[1].destroy();assert.notEqual(result[0],'opened');
});

test('session expiry invalidates already established connection before next command',async t=>{
  const f=await setup(t),ws=await f.connect();await once(ws,'open');f.setValid(false);
  const closed=once(ws,'close');ws.send(JSON.stringify({type:'ready',protocol:1,mapHash:42}));
  const [code]=await Promise.race([closed,new Promise(resolve=>setTimeout(()=>resolve([0]),200))]);assert.equal(code,4001);assert.equal(f.ready,0);
});
