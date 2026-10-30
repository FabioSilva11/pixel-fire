import { createPublicKey, createHash, randomBytes, verify } from 'node:crypto';

export class IdentityStore {
  constructor({now=()=>Date.now(),persistence=null}={}) { this.now=now;this.persistence=persistence;this.challenges=new Map();this.sessions=new Map(); }
  prune() {
    const now=this.now();
    for(const [k,v] of this.challenges)if(v.expires<=now)this.challenges.delete(k);
    for(const [k,v] of this.sessions)if(v.expires<=now)this.sessions.delete(k);
  }
  challenge(spki) {
    this.prune();
    if(typeof spki!=='string'||spki.length>2048||this.challenges.size>=2000)throw new Error('Identidade inválida ou serviço ocupado');
    const publicKey=createPublicKey({key:Buffer.from(spki,'base64'),type:'spki',format:'der'});
    if(publicKey.asymmetricKeyType!=='ec'||publicKey.asymmetricKeyDetails?.namedCurve!=='prime256v1')throw new Error('Chave P-256 obrigatória');
    const id=randomBytes(18).toString('base64url'),nonce=randomBytes(32).toString('base64');
    const playerId=createHash('sha256').update(publicKey.export({type:'spki',format:'der'})).digest('hex');
    this.challenges.set(id,{publicKey,nonce,playerId,expires:this.now()+60000});
    return {id,nonce,expiresIn:60};
  }
  authenticate(id,signature) {
    if(typeof id!=='string'||id.length>64)throw new Error('Desafio inválido');
    const c=this.challenges.get(id);this.challenges.delete(id);
    if(!c||c.expires<=this.now()||typeof signature!=='string'||signature.length>512||
       !verify('sha256',Buffer.from(c.nonce,'base64'),c.publicKey,Buffer.from(signature,'base64')))throw new Error('Autenticação inválida ou expirada');
    this.prune();if(this.sessions.size>=2000)throw new Error('Servidor ocupado');
    this.persistence?.recordIdentity(c.playerId);
    for(const [old,s] of this.sessions)if(s.id===c.playerId)this.sessions.delete(old);
    const token=randomBytes(32).toString('base64url');
    const expires=this.now()+3600000;
    this.sessions.set(token,{id:c.playerId,expires});
    return {token,playerId:c.playerId,expiresIn:3600};
  }
  resolve(token) {
    if(typeof token!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(token))throw new Error('Sessão inválida');
    const s=this.sessions.get(token);
    if(!s||s.expires<=this.now()){this.sessions.delete(token);throw new Error('Sessão expirada');}
    return s;
  }
}
