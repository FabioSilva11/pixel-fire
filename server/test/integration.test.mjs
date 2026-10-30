import { test } from 'node:test';
import assert from 'node:assert/strict';
import https from 'node:https';
import { readFileSync,existsSync } from 'node:fs';
import { generateKeyPairSync,sign } from 'node:crypto';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import WebSocket from 'ws';

const enabled=!!process.env.ZZ_TEST_CERT&&existsSync(new URL('../native/build/zzbr.node',import.meta.url));
test('TLS API + two independent identities + party + authoritative C++ match + reconnect', {skip:!enabled,timeout:30000},async t=>{
  const ca=readFileSync(process.env.ZZ_TEST_CERT);
  const apiPort=18080,gamePort=27016;
  const child=spawn(process.execPath,['src/service.mjs'],{cwd:new URL('..',import.meta.url),env:{...process.env,ZZ_API_PORT:String(apiPort),ZZ_GAME_PORT:String(gamePort),ZZ_TLS_CERT:process.env.ZZ_TEST_CERT,ZZ_TLS_KEY:process.env.ZZ_TEST_KEY},stdio:['ignore','pipe','pipe']});
  let stderr='';child.stderr.on('data',d=>stderr+=d);
  // This child is an isolated fixture; production SIGTERM deliberately drains for up to90s.
  t.after(()=>child.kill('SIGKILL'));
  await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error(stderr||'startup timeout')),8000);child.stdout.on('data',data=>{if(data.toString().includes('started')){clearTimeout(timer);resolve();}});child.on('exit',code=>{clearTimeout(timer);reject(new Error(`server exited ${code}: ${stderr}`));});});
  function request(path,data,token){return new Promise((resolve,reject)=>{
    const body=data===undefined?null:JSON.stringify(data);
    const req=https.request({hostname:'127.0.0.1',port:apiPort,path,method:body?'POST':'GET',ca,headers:{...(body?{'content-type':'application/json','content-length':Buffer.byteLength(body)}:{}),...(token?{authorization:`Bearer ${token}`}:{})}},res=>{let text='';res.on('data',d=>text+=d);res.on('end',()=>{const value=JSON.parse(text);res.statusCode===200?resolve(value):reject(new Error(value.error));});});req.on('error',reject);if(body)req.write(body);req.end();
  });}
  async function auth(){const pair=generateKeyPairSync('ec',{namedCurve:'prime256v1'});const c=await request('/auth/challenge',{publicKey:pair.publicKey.export({type:'spki',format:'der'}).toString('base64')});return request('/auth/complete',{id:c.id,signature:sign('sha256',Buffer.from(c.nonce,'base64'),pair.privateKey).toString('base64')});}
  const health=await request('/health');assert.equal(health.mapSize,1440);assert.equal(health.regions,13);
  const a=await auth(),b=await auth();assert.notEqual(a.playerId,b.playerId);
  const party=(await request('/party/create',{teamSize:2,autoFill:true},a.token)).party;
  const joined=(await request('/party/join',{code:party.code},b.token)).party;assert.equal(joined.members.length,2);
  const sockets=[a,b].map(user=>new WebSocket(`wss://127.0.0.1:${gamePort}/play`,{ca,headers:{authorization:`Bearer ${user.token}`}}));
  for(const ws of sockets)t.after(()=>ws.terminate());
  const messages=sockets.map(()=>[]),snapshots=sockets.map(()=>[]);
  sockets.forEach((ws,i)=>ws.on('message',(data,binary)=>{if(binary)snapshots[i].push(data);else messages[i].push(JSON.parse(data));}));
  await Promise.all(sockets.map(ws=>once(ws,'open')));
  sockets[0].send(JSON.stringify({type:'queue',teamSize:2,autoFill:true,training:true,look:0,mapHash:health.mapHash,protocol:1}));
  const waitFor=async condition=>{for(let i=0;i<100;i++){if(condition())return;await new Promise(r=>setTimeout(r,50));}throw new Error('event timeout');};
  await waitFor(()=>messages.every(list=>list.some(m=>m.type==='start')));
  assert(snapshots.every(s=>s.length===0));
  sockets.forEach(ws=>ws.send(JSON.stringify({type:'ready',mapHash:health.mapHash,protocol:1,look:0})));
  await waitFor(()=>snapshots.every(s=>s.length));
  const starts=messages.map(list=>list.find(m=>m.type==='start'));assert.equal(starts[0].matchId,starts[1].matchId);assert.notEqual(starts[0].localId,starts[1].localId);
  assert(starts[0].training);assert.equal(snapshots[0][0][0],2);
  sockets[0].close();await once(sockets[0],'close');
  const reconnected=new WebSocket(`wss://127.0.0.1:${gamePort}/play`,{ca,headers:{authorization:`Bearer ${a.token}`}});t.after(()=>reconnected.terminate());
  const resume=[];reconnected.on('message',(d,binary)=>{if(!binary)resume.push(JSON.parse(d));});
  await once(reconnected,'open');await waitFor(()=>resume.some(m=>m.type==='start'&&m.resume));
  assert.equal(resume.find(m=>m.type==='start').localId,starts[0].localId);
  reconnected.send(JSON.stringify({type:'ready',mapHash:health.mapHash,protocol:1,look:0}));
  const floodClosed=once(reconnected,'close');
  const ping=Buffer.alloc(12);ping[0]=3;ping[1]=1;
  for(let i=0;i<60;i++)reconnected.send(ping);
  const close=await Promise.race([floodClosed,new Promise((_,reject)=>setTimeout(()=>reject(new Error('Flood connection was not closed')),1200))]);
  assert.equal(close[0],1008);
});
