import { randomBytes, randomUUID } from 'node:crypto';
import { encodeSnapshot } from './protocol.mjs';

export class Matchmaker {
  constructor(native,parties,{maxPlayers=8,maxRooms=1,waitMs=12000,countdownMs=3000,readyTimeoutMs=180000,reconnectMs=30000,now=()=>Date.now(),persistence=null}={}) {
    for(const value of [maxPlayers,maxRooms,waitMs,countdownMs,readyTimeoutMs,reconnectMs])if(!Number.isFinite(value)||value<0)throw new Error('Configuração inválida');
    this.native=native;this.parties=parties;this.maxPlayers=Math.max(2,Math.min(50,Math.floor(maxPlayers)));this.maxRooms=Math.max(1,Math.min(4,Math.floor(maxRooms)));
    this.waitMs=waitMs;this.countdownMs=countdownMs;this.readyTimeoutMs=readyTimeoutMs;this.reconnectMs=reconnectMs;this.now=now;this.persistence=persistence;
    this.clients=new Map();this.queue=new Map();this.rooms=new Map();this.assignments=new Map();this.draining=false;this.drainAt=Infinity;
    this.tickCount=0;this.maxTickMs=0;this.rejectedInputs=0;
    this.bytesSent=0;this.bytesReceived=0;
  }
  send(id,message) {
    const client=this.clients.get(id);if(!client||client.ws.readyState!==1)return;
    if(client.ws.bufferedAmount>131072){client.ws.close(1013,'Conexão lenta');return;}
    try {const payload=typeof message==='object'&&!Buffer.isBuffer(message)?JSON.stringify(message):message;client.ws.send(payload);this.bytesSent+=Buffer.byteLength(payload);} catch {client.ws.close(1011,'Falha de envio');}
  }
  connect(id,ws) {
    const previous=this.clients.get(id);if(previous)previous.ws.close(4000,'Conexão substituída');
    this.clients.set(id,{ws,look:previous?.look??0,joinedAt:this.now(),ready:false});
    const assignment=this.assignments.get(id),room=assignment&&this.rooms.get(assignment.room);
    if(room) {
      if(assignment.disconnectedAt!==null&&this.now()-assignment.disconnectedAt>=this.reconnectMs){ws.close(4001,'Prazo de reconexão expirado');this.clients.delete(id);return;}
      assignment.disconnectedAt=null;assignment.loadingAt=this.now();room.ready.delete(id);
      if(room.state==='countdown'){room.state='loading';room.countdownAt=null;}
      this.native.connected(room.handle,assignment.slot,0);this.startMessage(id,room,assignment.slot,true);
    } else {this.assignments.delete(id);this.send(id,{type:'connected',mapHash:this.native.mapHash,mapVersion:this.native.mapVersion,protocol:1});}
  }
  disconnect(id,ws) {
    if(this.clients.get(id)?.ws!==ws)return;
    this.clients.delete(id);
    const assignment=this.assignments.get(id),room=assignment&&this.rooms.get(assignment.room);
    if(room){assignment.disconnectedAt=this.now();room.ready.delete(id);this.native.connected(room.handle,assignment.slot,0);if(room.state==='countdown'){room.state='loading';room.countdownAt=null;}}
    for(const [code,entry] of this.queue)if(entry.party.members.includes(id))this.cancelParty(code,'Integrante desconectado');
  }
  enqueue(id,options) {
    if(this.draining)throw new Error('Servidor encerrando; tente novamente em breve');
    if(!this.clients.has(id))throw new Error('Conexão ausente');
    if(this.assignments.has(id))throw new Error('Você já está em uma partida');
    if(![1,2,4].includes(options.teamSize)||options.mapHash!==this.native.mapHash||options.protocol!==1)throw new Error('Formação ou versão do mapa incompatível');
    if(!Number.isInteger(options.look)||options.look<0||options.look>536870911)throw new Error('Aparência inválida');
    this.clients.get(id).look=options.look;
    let party=this.parties.forPlayer(id);
    if(!party)party=this.parties.create(id,options.teamSize,options.autoFill!==false);
    if(party.teamSize!==options.teamSize)throw new Error('A formação precisa ser igual à do grupo');
    if(party.members.some(member=>!this.clients.has(member)||this.assignments.has(member)))throw new Error('Todos do grupo precisam estar disponíveis e conectados');
    if(this.queue.size>=1000)throw new Error('Fila cheia');
    party=this.parties.lock(id);
    if(options.training===true) {
      if(this.rooms.size>=this.maxRooms){this.parties.unlock(party.code);throw new Error('Servidor ocupado; tente novamente após a partida atual');}
      return this.createRoom([[...party.members]],party.teamSize,[party.code],true);
    }
    this.queue.set(party.code,{party,time:this.now()});
    for(const member of party.members)this.send(member,{type:'queued',message:'Aguardando adversários',party:party.code,humans:party.members.length});
  }
  cancelParty(code,message) {
    const entry=this.queue.get(code);if(!entry)return;
    this.queue.delete(code);this.parties.unlock(code);for(const member of entry.party.members)this.send(member,{type:'cancelled',message});
  }
  cancel(id) {
    const p=this.parties.forPlayer(id);if(!p)return;
    if(this.queue.has(p.code)&&p.owner!==id)throw new Error('Somente o líder pode cancelar a fila');
    this.cancelParty(p.code,'Busca cancelada');
  }
  abandon(id) {
    const assignment=this.assignments.get(id),room=assignment&&this.rooms.get(assignment.room);
    if(!room){const party=this.parties.forPlayer(id);if(party)this.cancelParty(party.code,'Integrante saiu');this.parties.removeFromMatch(id);this.send(id,{type:'matchClosed',reason:'left'});return;}
    if(room.state!=='running'){this.releaseRoom(room,'participantLeft');return;}
    this.native.forfeit(room.handle,assignment.slot);room.ready.delete(id);
    room.identities=room.identities.filter(member=>member!==id);this.assignments.delete(id);
    this.parties.removeFromMatch(id);const client=this.clients.get(id);if(client)client.ready=false;
    this.send(id,{type:'matchClosed',reason:'left'});
    if(!room.identities.length)this.releaseRoom(room,'abandoned');
  }
  pump() {
    if(this.draining||this.rooms.size>=this.maxRooms)return;
    for(const size of [1,2,4]) {
      const entries=[...this.queue.values()].filter(e=>e.party.teamSize===size);
      const teams=[],codes=[];let count=0;
      for(const entry of entries) {
        const p=entry.party;if(count+p.members.length>this.maxPlayers)continue;
        let team=p.autoFill?teams.find(t=>t.fill&&t.members.length+p.members.length<=size):null;
        if(!team){team={fill:p.autoFill,members:[]};teams.push(team);}
        team.members.push(...p.members);codes.push(p.code);count+=p.members.length;
      }
      if(teams.length<2)continue;
      const remainingMs=Math.max(0,this.waitMs-(this.now()-entries[0].time));
      if(remainingMs>0){for(const team of teams)for(const id of team.members)this.send(id,{type:'countdown',stage:'lobby',remainingMs,humans:count});continue;}
      for(const code of codes)this.queue.delete(code);
      this.createRoom(teams.map(t=>t.members),size,codes,false);break;
    }
  }
  createRoom(teams,size,codes,training) {
    if(this.draining||this.rooms.size>=this.maxRooms)throw new Error('Servidor ocupado');
    const identities=teams.flat();
    if(![1,2,4].includes(size)||!identities.length||identities.length>this.maxPlayers||new Set(identities).size!==identities.length||teams.some(t=>t.length>size)||identities.some(id=>!this.clients.has(id)||this.assignments.has(id)))throw new Error('Equipe inválida');
    const handle=this.native.create(size,randomBytes(4).readUInt32LE(),0,50,55,35);
    const room={id:randomUUID(),handle,teamSize:size,codes,identities,training,created:this.now(),endedAt:null,state:'loading',ready:new Set(),countdownAt:null};
    let slot=0;
    try {
      teams.forEach((members,team)=>members.forEach(id=>{
        if(!this.native.join(handle,slot,team,this.clients.get(id).look??0,0))throw new Error('Falha ao montar equipe');
        this.native.connected(handle,slot,0);this.assignments.set(id,{room:room.id,slot,lastSequence:0,disconnectedAt:null,loadingAt:this.now()});++slot;
      }));
      if(training) {
        const target=Math.min(this.maxPlayers,Math.max(size*2,8));let team=0,members=teams[0]?.length??0;
        while(slot<target){if(members>=size){++team;members=0;}if(!this.native.join(handle,slot,team,slot%6,1))throw new Error('Falha ao montar treino');++slot;++members;}
      }
      room.participants=identities.map(identity=>({identity,slot:this.assignments.get(identity).slot}));
      this.rooms.set(room.id,room);
      for(const id of identities)this.startMessage(id,room,this.assignments.get(id).slot,false);
      return room;
    } catch(error){for(const id of identities)this.assignments.delete(id);for(const code of codes)this.parties.unlock(code);this.native.destroy?.(handle);throw error;}
  }
  startMessage(id,room,slot,resume) {
    this.send(id,{type:'start',matchId:room.id,localId:slot,teamSize:room.teamSize,mapHash:this.native.mapHash,mapVersion:this.native.mapVersion,protocol:1,training:room.training,resume,loading:true,readyTimeoutMs:this.readyTimeoutMs,lastSequence:this.assignments.get(id)?.lastSequence??0});
  }
  ready(id,message,ws) {
    const client=this.clients.get(id),a=this.assignments.get(id),room=a&&this.rooms.get(a.room);
    if(!client||(ws&&client.ws!==ws)||!room||message.mapHash!==this.native.mapHash||message.protocol!==1)throw new Error('READY ou mapa inválido');
    if(message.look!==undefined&&(!Number.isInteger(message.look)||message.look<0||message.look>536870911))throw new Error('Aparência inválida');
    if(client.ready)return;
    client.ready=true;room.ready.add(id);this.native.connected(room.handle,a.slot,1);
    this.send(id,encodeSnapshot(this.native.snapshot(room.handle,a.slot)));
    if(room.state==='loading'&&room.identities.every(member=>room.ready.has(member)&&this.clients.has(member))) {
      room.state='countdown';room.countdownAt=this.now();
      for(const member of room.identities)this.send(member,{type:'countdown',stage:'flight',remainingMs:this.countdownMs});
    }
  }
  input(id,floats,ws) {
    const client=this.clients.get(id),a=this.assignments.get(id),room=a&&this.rooms.get(a.room),seq=floats?.[0];
    if(!room||room.state!=='running'||!client?.ready||(ws&&client.ws!==ws)||!(floats instanceof Float32Array)||floats.length!==12||!Number.isInteger(seq)||seq<=a.lastSequence||seq>a.lastSequence+1024||seq>16777215){this.rejectedInputs++;return false;}
    if(!this.native.input(room.handle,a.slot,floats)){this.rejectedInputs++;return false;}
    a.lastSequence=seq;return true;
  }
  beginDrain(timeoutMs=90000) {
    if(this.draining)return;
    this.draining=true;this.drainAt=this.now()+Math.min(300000,Math.max(0,timeoutMs));
    for(const code of [...this.queue.keys()])this.cancelParty(code,'Servidor em atualização');
    for(const id of this.clients.keys())this.send(id,{type:'draining',remainingMs:this.drainAt-this.now()});
  }
  tick() {
    const start=performance.now(),now=this.now();++this.tickCount;
    for(const room of this.rooms.values()) {
      if(this.draining&&now>=this.drainAt){this.releaseRoom(room,'serverDraining');continue;}
      if(room.identities.some(id=>this.clients.has(id)))room.orphanedAt=null;
      else {room.orphanedAt??=now;if(now-room.orphanedAt>=this.reconnectMs){this.releaseRoom(room,'disconnected');continue;}}
      if(room.state==='loading'&&now-room.created>=this.readyTimeoutMs){this.releaseRoom(room,'loadingTimeout');continue;}
      for(const id of room.identities){const a=this.assignments.get(id),client=this.clients.get(id);if(room.state==='running'&&client&&!client.ready&&now-a.loadingAt>=this.reconnectMs){client.ws.close(4001,'Reconexão expirada');this.disconnect(id,client.ws);}}
      if(room.state==='countdown'&&now-room.countdownAt>=this.countdownMs)room.state='running';
      if(room.state!=='running')continue;
      this.native.step(room.handle,.05);
      if(this.tickCount%2===0) {
        let phase=0;
        for(const id of room.identities) {
          const a=this.assignments.get(id);if(!a)continue;
          // Viewer -1 is never exposed: every snapshot has a bound player slot.
          const state=this.native.snapshot(room.handle,a.slot);phase=state[1];
          if(this.clients.get(id)?.ready){
            this.send(id,encodeSnapshot(state));
            if(this.native.effects){const fx=encodeSnapshot(this.native.effects(room.handle,a.slot));fx[0]=5;this.send(id,fx);}
          }
        }
        if(phase===3&&room.endedAt===null) {
          room.endedAt=now;
          const result={matchId:room.id,endedAt:now,training:room.training,mapHash:this.native.mapHash,mapVersion:this.native.mapVersion,participants:room.participants,...this.native.results?.(room.handle)};
          try {this.persistence?.recordResult(result);} catch {console.error(JSON.stringify({event:'result_persistence_failed',matchId:room.id}));}
          for(const id of room.identities){const slot=this.assignments.get(id).slot;this.send(id,{type:'result',matchId:room.id,training:room.training,winner:result.winner,player:result.players?.find(p=>p.id===slot)});}
        }
      }
      if(room.endedAt!==null&&now-room.endedAt>20000)this.releaseRoom(room,'finished');
    }
    if(this.tickCount%20===0){this.pump();this.parties.prune(this.clients);}
    this.maxTickMs=Math.max(this.maxTickMs,performance.now()-start);
  }
  releaseRoom(room,reason='closed') {
    for(const id of room.identities){this.assignments.delete(id);const client=this.clients.get(id);if(client)client.ready=false;this.send(id,{type:'matchClosed',reason});}
    for(const code of room.codes)this.parties.unlock(code);
    this.native.destroy?.(room.handle);this.rooms.delete(room.id);
  }
}
