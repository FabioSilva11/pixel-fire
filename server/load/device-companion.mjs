// Independent authenticated test players for a physical Android smoke test. No admin endpoint or token file.
import https from 'node:https';
import { readFileSync } from 'node:fs';
import { generateKeyPairSync,sign } from 'node:crypto';
import { once } from 'node:events';
import { createInterface } from 'node:readline';
import WebSocket from 'ws';
import { encodeInput } from '../src/protocol.mjs';
const host=process.env.ZZ_LOAD_HOST??'187.102.244.208';
const ca=readFileSync(process.env.ZZ_LOAD_CA??new URL('../../app/src/main/res/raw/server_ca.crt',import.meta.url));
const count=Number(process.env.ZZ_COMPANIONS??1),teamSize=Number(process.env.ZZ_TEAM_SIZE??1);
const jumpAt=Number(process.env.ZZ_COMPANION_JUMP_AT??0);
if(!Number.isInteger(count)||count<1||count>7||![1,2,4].includes(teamSize))throw new Error('Invalid fixture size');
const clients=[];
function api(path,data,token){return new Promise((resolve,reject)=>{
  const body=JSON.stringify(data);const req=https.request({hostname:host,port:8080,path,method:'POST',ca,headers:{'content-type':'application/json','content-length':Buffer.byteLength(body),...(token?{authorization:`Bearer ${token}`}:{})}},res=>{
    let text='';res.on('data',d=>text+=d);res.on('end',()=>{try{const value=JSON.parse(text);if(res.statusCode!==200)throw new Error(value.error);resolve(value);}catch(e){reject(e);}});
  });req.on('error',reject);req.end(body);
});}
for(let i=0;i<count;++i){
  const keys=generateKeyPairSync('ec',{namedCurve:'prime256v1'});
  const challenge=await api('/auth/challenge',{publicKey:keys.publicKey.export({type:'spki',format:'der'}).toString('base64')});
  const auth=await api('/auth/complete',{id:challenge.id,signature:sign('sha256',Buffer.from(challenge.nonce,'base64'),keys.privateKey).toString('base64')});
  const ws=new WebSocket(`wss://${host}:27015/play`,{ca,headers:{authorization:`Bearer ${auth.token}`}});
  const client={ws,sequence:0,id:-1,phase:0,phaseTime:0,flight:0,snapshots:0,health:0,look:i+1};clients.push(client);
  ws.on('message',(data,binary)=>{
    if(binary){if(data[0]!==2)return;client.phase=data.readFloatLE(8);client.phaseTime=data.readFloatLE(4+22*4);++client.snapshots;
      const n=data.readFloatLE(4+16*4);for(let p=0;p<n;++p){const row=4+(24+p*32)*4;if(data.readFloatLE(row)===client.id){client.flight=data.readFloatLE(row+11*4);client.health=data.readFloatLE(row+9*4);}}
      return;
    }
    const m=JSON.parse(data);
    if(m.type==='connected'){client.mapHash=m.mapHash;ws.send(JSON.stringify({type:'queue',teamSize,autoFill:false,mapHash:m.mapHash,protocol:1,look:client.look}));}
    if(m.type==='start'){client.id=m.localId;client.sequence=m.lastSequence??0;ws.send(JSON.stringify({type:'ready',protocol:1,mapHash:m.mapHash,look:client.look}));}
    if(m.type==='result')client.phase=3;
    if(m.type==='matchClosed'){client.phase=0;client.flight=0;if(client.mapHash)setTimeout(()=>ws.readyState===1&&ws.send(JSON.stringify({type:'queue',teamSize,autoFill:false,mapHash:client.mapHash,protocol:1,look:client.look})),1500);}
    if(['queued','start','countdown','result','matchClosed','error'].includes(m.type))console.log(JSON.stringify({fixture:i,event:m.type,stage:m.stage,remainingMs:m.remainingMs,localId:m.localId,message:m.message,placement:m.player?.placement}));
  });
  ws.on('error',()=>console.error(JSON.stringify({fixture:i,event:'connection-error'})));
  ws.on('close',code=>console.log(JSON.stringify({fixture:i,event:'closed',code})));
  await once(ws,'open');
}
const tick=setInterval(()=>{
  for(const c of clients)if(c.ws.readyState===1&&(c.phase===1||c.phase===2))c.ws.send(encodeInput({sequence:++c.sequence,moveX:0,moveZ:0,yaw:0,pitch:0,buttons:c.flight===0&&c.phaseTime>=jumpAt?2:0}));
},50);
const metrics=setInterval(()=>console.log(JSON.stringify({event:'fixture-status',players:clients.map(c=>({phase:c.phase,flight:c.flight,health:c.health,snapshots:c.snapshots}))})),10000);
const input=createInterface({input:process.stdin});
function stop(){clearInterval(tick);clearInterval(metrics);input.close();for(const c of clients){c.ws.send(JSON.stringify({type:'leave'}));c.ws.close();}setTimeout(()=>process.exit(0),1000).unref();}
input.on('line',line=>{if(line.trim()==='finish')for(const c of clients)c.ws.send(JSON.stringify({type:'leave'}));if(line.trim()==='quit')stop();});
setTimeout(stop,Number(process.env.ZZ_COMPANION_SECONDS??300)*1000).unref();
process.on('SIGINT',stop);process.on('SIGTERM',stop);
