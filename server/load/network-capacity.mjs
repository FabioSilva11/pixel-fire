import https from 'node:https';
import { readFileSync } from 'node:fs';
import { generateKeyPairSync,sign } from 'node:crypto';
import { once } from 'node:events';
import WebSocket from 'ws';
import { encodeInput } from '../src/protocol.mjs';
const host=process.env.ZZ_LOAD_HOST??'187.102.244.208';
const apiPort=Number(process.env.ZZ_LOAD_API_PORT??8080),gamePort=Number(process.env.ZZ_LOAD_GAME_PORT??27015);
const ca=readFileSync(process.env.ZZ_LOAD_CA??'/etc/zombie-zone/server.crt');
const count=Number(process.env.ZZ_LOAD_PLAYERS??50),duration=Number(process.env.ZZ_LOAD_SECONDS??90);
const clients=[],rtts=[];let bytes=0,snapshots=0,rejected=0;
function request(path,data){return new Promise((resolve,reject)=>{
  const body=data?JSON.stringify(data):null;
  const req=https.request({hostname:host,port:apiPort,path,method:body?'POST':'GET',ca,headers:body?{'content-type':'application/json','content-length':Buffer.byteLength(body)}:{}},res=>{
    let text='';res.on('data',d=>text+=d);res.on('end',()=>{let value;try{value=JSON.parse(text);}catch(e){return reject(e);}res.statusCode===200?resolve(value):reject(new Error(value.error));});
  });req.on('error',reject);if(body)req.write(body);req.end();
});}
async function createClient(index) {
  const key=generateKeyPairSync('ec',{namedCurve:'prime256v1'});
  const challenge=await request('/auth/challenge',{publicKey:key.publicKey.export({type:'spki',format:'der'}).toString('base64')});
  const auth=await request('/auth/complete',{id:challenge.id,signature:sign('sha256',Buffer.from(challenge.nonce,'base64'),key.privateKey).toString('base64')});
  const ws=new WebSocket(`wss://${host}:${gamePort}/play`,{ca,headers:{authorization:`Bearer ${auth.token}`}});
  const c={ws,index,sequence:0,started:false,localId:-1,flight:0,matchId:'',last:0};
  ws.on('message',(data,binary)=>{
    bytes+=data.length;
    if(binary) {
      if(data[0]===4){rtts.push(Date.now()-data.readDoubleLE(4));return;}
      if(data[0]!==2)return;++snapshots;c.last=Date.now();
      const players=data.readFloatLE(4+16*4);
      if(players>count)throw new Error(`Unexpected player count ${players}`);
      for(let i=0;i<players;i++){const at=4+(24+i*32)*4;if(data.readFloatLE(at)===c.localId)c.flight=data.readFloatLE(at+11*4);}
    } else {
      const message=JSON.parse(data);
      if(message.type==='start'){c.started=true;c.localId=message.localId;c.matchId=message.matchId;ws.send(JSON.stringify({type:'ready',protocol:1,mapHash:message.mapHash,look:index%6}));}
      if(message.type==='error'){++rejected;console.log(JSON.stringify({event:'server-rejected-command',message:message.message}));}
    }
  });
  await once(ws,'open');clients.push(c);
}
try {
  const health=await request('/health');if(health.rooms)throw new Error('Refusing load test while an existing room is active');
  if(health.maxPlayers<count)throw new Error('Admission cap below requested load');
  for(let batch=0;batch<count;batch+=5)await Promise.all(Array.from({length:Math.min(5,count-batch)},(_,n)=>createClient(batch+n)));
  for(const c of clients)c.ws.send(JSON.stringify({type:'queue',protocol:1,mapHash:health.mapHash,teamSize:1,autoFill:true,training:false,look:c.index%6}));
  const deadline=Date.now()+25000;
  while(!clients.every(c=>c.started)){if(Date.now()>deadline)throw new Error('Match start timeout');await new Promise(r=>setTimeout(r,100));}
  if(new Set(clients.map(c=>c.matchId)).size!==1)throw new Error('Clients were not assigned to one match');
  console.log(JSON.stringify({event:'load-started',players:count,durationSeconds:duration}));
  const begin=Date.now();let tick=0;
  while(Date.now()-begin<duration*1000) {
    const start=Date.now();++tick;
    for(const c of clients) {
      const buttons=c.flight===0?2:(c.flight===3?32|8:0);
      c.ws.send(encodeInput({sequence:++c.sequence,moveX:Math.sin(c.index+tick*.02)*.4,moveZ:1,yaw:Math.sin(c.index)*2,pitch:0,buttons}));
      if(tick%20===0){const ping=Buffer.alloc(12);ping[0]=3;ping[1]=1;ping.writeDoubleLE(Date.now(),4);c.ws.send(ping);}
    }
    await new Promise(r=>setTimeout(r,Math.max(1,50-(Date.now()-start))));
  }
  rtts.sort((a,b)=>a-b);
  const result={players:count,seconds:(Date.now()-begin)/1000,snapshots,bytes,bytesPerSecond:Math.round(bytes/duration),
    rttP50:rtts[Math.floor(rtts.length*.5)],rttP95:rtts[Math.floor(rtts.length*.95)],rttP99:rtts[Math.floor(rtts.length*.99)],rejected,
    disconnected:clients.filter(c=>c.ws.readyState!==WebSocket.OPEN).length,stale:clients.filter(c=>Date.now()-c.last>2000).length};
  console.log(JSON.stringify(result));
  if(result.disconnected||result.stale||result.rejected||snapshots<count*duration*7)process.exitCode=1;
} catch(error){console.error(error.message);process.exitCode=1;}
finally {for(const c of clients){if(c.ws.readyState===WebSocket.OPEN)c.ws.send(JSON.stringify({type:'leave'}));c.ws.close();}}
