import { randomBytes } from 'node:crypto';
export class PartyStore {
  constructor(){this.parties=new Map();this.membership=new Map();}
  create(player,size,fill=true) {
    if(![1,2,4].includes(size))throw new Error('Formação inválida');
    this.leave(player);
    if(this.parties.size>=1000)throw new Error('Limite de grupos');
    let code;do {code=randomBytes(4).toString('hex').toUpperCase();}while(this.parties.has(code));
    const p={code,owner:player,teamSize:size,autoFill:!!fill,members:[player],locked:false,updated:Date.now()};
    this.parties.set(code,p);this.membership.set(player,code);return this.view(p);
  }
  forPlayer(player){return this.view(this.parties.get(this.membership.get(player)));}
  view(p){return p?{...p,members:[...p.members]}:null;}
  join(player,code) {
    const p=this.parties.get(String(code).trim().toUpperCase());
    if(!p||p.locked||p.members.length>=p.teamSize)throw new Error('Grupo inexistente, cheio ou em partida');
    if(p.members.includes(player))return this.view(p);
    this.leave(player);p.members.push(player);p.updated=Date.now();this.membership.set(player,p.code);return this.view(p);
  }
  leave(player) {
    const p=this.parties.get(this.membership.get(player));if(!p)return;
    if(p.locked)throw new Error('Saia da fila ou da partida primeiro');
    p.members=p.members.filter(id=>id!==player);this.membership.delete(player);p.updated=Date.now();
    if(!p.members.length)this.parties.delete(p.code);else if(p.owner===player)p.owner=p.members[0];
  }
  lock(player) {
    const p=this.parties.get(this.membership.get(player));
    if(!p||p.owner!==player)throw new Error('Somente o líder pode procurar partida');
    if(p.locked)throw new Error('Grupo já está na fila');p.locked=true;return this.view(p);
  }
  unlock(code){const p=this.parties.get(code);if(p){p.locked=false;p.updated=Date.now();}}
  removeFromMatch(player) {
    const p=this.parties.get(this.membership.get(player));if(!p)return;
    p.members=p.members.filter(id=>id!==player);this.membership.delete(player);p.updated=Date.now();
    if(!p.members.length)this.parties.delete(p.code);else if(p.owner===player)p.owner=p.members[0];
  }
  prune(connected){for(const p of this.parties.values())if(!p.locked&&Date.now()-p.updated>3600000&&!p.members.some(x=>connected.has(x)))for(const id of [...p.members])this.leave(id);}
}
