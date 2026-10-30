import { WebSocketServer } from 'ws';
import { decodeInput,inputFloats,VERSION,TYPE_PING,TYPE_PONG } from './protocol.mjs';
import { RateLimiter } from './limits.mjs';

export function attachTransport(server,identity,matchmaker,{allowedOrigins=[]}={}) {
  const wss=new WebSocketServer({noServer:true,maxPayload:1024,perMessageDeflate:false});
  const addresses=new RateLimiter({capacity:30,refillPerSecond:2,maxKeys:5000});
  server.on('upgrade',(request,socket,head)=>{
    socket.on('error',()=>{});
    try {
      if(request.url!=='/play'||matchmaker.draining)throw new Error('unavailable');
      const origin=request.headers.origin;
      if(origin&&!allowedOrigins.includes(origin))throw new Error('origin');
      if(!addresses.allow(request.socket.remoteAddress))throw new Error('rate');
      const authorization=request.headers.authorization;
      if(typeof authorization!=='string'||!authorization.startsWith('Bearer '))throw new Error('auth');
      const token=authorization.slice(7),player=identity.resolve(token);
      if(matchmaker.clients.size>=100&&!matchmaker.clients.has(player.id))throw new Error('capacity');
      wss.handleUpgrade(request,socket,head,ws=>wss.emit('connection',ws,request,player,token));
    } catch {socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\nContent-Length: 0\r\n\r\n');}
  });
  wss.on('connection',(ws,request,player,token)=>{
    matchmaker.connect(player.id,ws);
    const commands=new RateLimiter({capacity:50,refillPerSecond:40,maxKeys:1}),texts=new RateLimiter({capacity:4,refillPerSecond:4,maxKeys:1});
    let alive=true,invalid=0;
    const sessionValid=()=>{try {return identity.resolve(token).id===player.id;}catch {ws.close(4001,'Sessão expirada');return false;}};
    ws.on('pong',()=>{alive=true;});
    ws.on('message',(data,binary)=>{
      matchmaker.bytesReceived=(matchmaker.bytesReceived??0)+data.length;
      if(!sessionValid()||matchmaker.clients.get(player.id)?.ws!==ws)return;
      if(!commands.allow('connection')){ws.close(1008,'Limite de comandos excedido');return;}
      try {
        if(binary) {
          if(data.length===12&&data[0]===TYPE_PING&&data[1]===VERSION&&data[2]===0&&data[3]===0&&Number.isFinite(data.readDoubleLE(4))) {
            if(ws.bufferedAmount>131072){ws.close(1013,'Conexão lenta');return;}
            const pong=Buffer.from(data);pong[0]=TYPE_PONG;ws.send(pong);return;
          }
          const input=decodeInput(data);matchmaker.input(player.id,inputFloats(input),ws);
        } else {
          if(!texts.allow('connection')||data.length>1024){ws.close(1008,'Limite de mensagens excedido');return;}
          const message=JSON.parse(data.toString());
          if(!message||typeof message!=='object'||Array.isArray(message))throw new Error('Mensagem inválida');
          if(message.type==='queue')matchmaker.enqueue(player.id,message);
          else if(message.type==='cancel')matchmaker.cancel(player.id);
          else if(message.type==='leave')matchmaker.abandon(player.id);
          else if(message.type==='ready')matchmaker.ready(player.id,message,ws);
          else throw new Error('Comando desconhecido');
        }
      } catch(error) {
        if(++invalid>=8){ws.close(1008,'Comandos inválidos');return;}
        matchmaker.send(player.id,{type:'error',message:String(error.message).slice(0,160)});
      }
    });
    ws.on('close',()=>matchmaker.disconnect(player.id,ws));ws.on('error',()=>{});
    const heartbeat=setInterval(()=>{if(!sessionValid())return;if(!alive){ws.terminate();return;}alive=false;ws.ping();},10000);
    heartbeat.unref();ws.on('close',()=>clearInterval(heartbeat));
  });
  const cleanup=setInterval(()=>addresses.prune(),60000);cleanup.unref();wss.on('close',()=>clearInterval(cleanup));
  return wss;
}
