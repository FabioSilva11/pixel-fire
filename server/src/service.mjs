import https from 'node:https';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { monitorEventLoopDelay } from 'node:perf_hooks';
import { IdentityStore } from './identity.mjs';
import { PartyStore } from './party.mjs';
import { Matchmaker } from './matchmaker.mjs';
import { attachTransport } from './transport.mjs';
import { PersistenceStore } from './persistence.mjs';

const native=createRequire(import.meta.url)('../native/build/zzbr.node');
const persistence=new PersistenceStore(process.env.ZZ_DATA_FILE??'/var/lib/zombie-zone/results.json');
const identity=new IdentityStore({persistence}),parties=new PartyStore();
const maxPlayers=Number(process.env.ZZ_MAX_PLAYERS??8);
const matchmaking=new Matchmaker(native,parties,{maxPlayers,maxRooms:1,persistence});
const tls={key:readFileSync(process.env.ZZ_TLS_KEY??'/etc/zombie-zone/server.key'),cert:readFileSync(process.env.ZZ_TLS_CERT??'/etc/zombie-zone/server.crt'),minVersion:'TLSv1.2'};
const rates=new Map(),delay=monitorEventLoopDelay({resolution:20});delay.enable();
function respond(res,status,body){res.writeHead(status,{'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff'});res.end(JSON.stringify(body));}
async function body(req) {
  let size=0;const parts=[];
  for await(const chunk of req){size+=chunk.length;if(size>4096)throw new Error('Pedido muito grande');parts.push(chunk);}
  const value=JSON.parse(Buffer.concat(parts).toString()||'{}');if(!value||typeof value!=='object'||Array.isArray(value))throw new Error('Pedido inválido');return value;
}
const api=https.createServer(tls,async(req,res)=>{
  try {
    const now=Date.now(),ip=req.socket.remoteAddress;let rate=rates.get(ip);
    if(!rate||now-rate.time>=60000){rate={time:now,count:0};if(rates.size>5000)throw new Error('Serviço ocupado');rates.set(ip,rate);}
    if(++rate.count>300){respond(res,429,{error:'Aguarde antes de tentar novamente'});return;}
    if(req.method==='GET'&&req.url==='/health') {
      respond(res,200,{ok:!matchmaking.draining,protocol:1,mapVersion:native.mapVersion,mapHash:native.mapHash,regions:13,mapSize:native.mapSize,maxPlayers:matchmaking.maxPlayers,
        rooms:matchmaking.rooms.size,players:matchmaking.clients.size,queuedGroups:matchmaking.queue.size,tickHz:20,snapshotHz:10,
        rssMiB:Math.round(process.memoryUsage().rss/1048576),uptimeSeconds:Math.floor(process.uptime()),draining:matchmaking.draining});return;
    }
    if(req.method==='POST'&&req.url==='/auth/challenge'){const data=await body(req);respond(res,200,identity.challenge(data.publicKey));return;}
    if(req.method==='POST'&&req.url==='/auth/complete'){const data=await body(req);respond(res,200,identity.authenticate(data.id,data.signature));return;}
    const player=identity.resolve(req.headers.authorization?.replace(/^Bearer /,''));
    if(req.method==='GET'&&req.url==='/party'){respond(res,200,{party:parties.forPlayer(player.id)});return;}
    if(req.method==='POST'&&req.url==='/party/create'){const b=await body(req);respond(res,200,{party:parties.create(player.id,b.teamSize,b.autoFill!==false)});return;}
    if(req.method==='POST'&&req.url==='/party/join'){const b=await body(req);respond(res,200,{party:parties.join(player.id,b.code)});return;}
    if(req.method==='POST'&&req.url==='/party/leave'){parties.leave(player.id);respond(res,200,{ok:true});return;}
    respond(res,404,{error:'Rota inexistente'});
  } catch(error){if(!res.headersSent)respond(res,400,{error:String(error.message).slice(0,160)});else res.end();}
});
api.requestTimeout=10000;api.headersTimeout=8000;api.keepAliveTimeout=3000;api.maxConnections=128;
const game=https.createServer(tls,(req,res)=>respond(res,404,{error:'Use WebSocket /play'}));game.maxConnections=128;
attachTransport(game,identity,matchmaking);
const tick=setInterval(()=>matchmaking.tick(),50);
const cleanup=setInterval(()=>{identity.prune();for(const [ip,r] of rates)if(Date.now()-r.time>120000)rates.delete(ip);},60000);cleanup.unref();
let previousCpu=process.cpuUsage(),previousOut=0,previousIn=0,previousMetrics=Date.now();
const metrics=setInterval(()=>{
  const elapsed=Math.max(1,Date.now()-previousMetrics),cpu=process.cpuUsage(previousCpu);
  console.log(JSON.stringify({event:'metrics',rooms:matchmaking.rooms.size,connections:matchmaking.clients.size,rssMiB:Math.round(process.memoryUsage().rss/1048576),
    cpuPercent:Math.round((cpu.user+cpu.system)/elapsed/10),outBytesPerSecond:Math.round((matchmaking.bytesSent-previousOut)*1000/elapsed),inBytesPerSecond:Math.round((matchmaking.bytesReceived-previousIn)*1000/elapsed),
    maxTickMs:Math.round(matchmaking.maxTickMs*100)/100,eventLoopP99Ms:Math.round(delay.percentile(99)/1e4)/100}));
  previousCpu=process.cpuUsage();previousOut=matchmaking.bytesSent;previousIn=matchmaking.bytesReceived;previousMetrics=Date.now();
  matchmaking.maxTickMs=0;delay.reset();
},30000);metrics.unref();
api.listen(Number(process.env.ZZ_API_PORT??8080),process.env.ZZ_BIND_HOST??'0.0.0.0');game.listen(Number(process.env.ZZ_GAME_PORT??27015),process.env.ZZ_BIND_HOST??'0.0.0.0');
console.log(JSON.stringify({event:'started',protocol:1,mapHash:native.mapHash,maxPlayers:matchmaking.maxPlayers}));
let stopping=false;
for(const signal of ['SIGTERM','SIGINT'])process.on(signal,()=>{
  if(stopping)return;stopping=true;matchmaking.beginDrain(90000);
  const wait=setInterval(()=>{if(matchmaking.rooms.size)return;clearInterval(wait);clearInterval(tick);clearInterval(metrics);api.close();game.close();setTimeout(()=>process.exit(0),300).unref();},500);
  setTimeout(()=>process.exit(0),95000).unref();
});
