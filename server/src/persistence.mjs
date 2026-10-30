import { mkdirSync, existsSync, readFileSync, openSync, writeFileSync, fsyncSync, closeSync, renameSync, unlinkSync, statSync } from 'node:fs';
import { dirname } from 'node:path';

// Single process writer. Sessions, challenges and private keys deliberately remain ephemeral.
// Atomic rename + fsync leaves a complete previous or new state after an interrupted write.
export class PersistenceStore {
  constructor(path,{maxResults=1000,maxIdentities=10000}={}) {
    this.path=path;this.maxResults=maxResults;this.maxIdentities=maxIdentities;
    mkdirSync(dirname(path),{recursive:true,mode:0o700});
    const state=existsSync(path)?JSON.parse(readFileSync(path,'utf8')):{version:1,identities:[],results:[]};
    if(state.version!==1||!Array.isArray(state.identities)||!Array.isArray(state.results)||statSync(dirname(path)).isDirectory()===false)throw new Error('Persistência incompatível');
    this.identities=state.identities.slice(-maxIdentities);this.results=state.results.slice(-maxResults);
  }
  save() {
    const temp=`${this.path}.${process.pid}.tmp`;let fd;
    try {
      fd=openSync(temp,'wx',0o600);writeFileSync(fd,JSON.stringify({version:1,identities:this.identities,results:this.results}));fsyncSync(fd);closeSync(fd);fd=undefined;
      renameSync(temp,this.path);
      // Directory fsync is supported on deployment Linux; Windows cannot open directories this way.
      if(process.platform!=='win32'){const directory=openSync(dirname(this.path),'r');try {fsyncSync(directory);} finally {closeSync(directory);}}
    } catch(error){if(fd!==undefined)closeSync(fd);try {unlinkSync(temp);} catch {}throw error;}
  }
  recordIdentity(id) {
    const previous=this.identities.find(x=>x.id===id),now=Date.now();
    this.identities=this.identities.filter(x=>x.id!==id);this.identities.push({id,firstSeen:previous?.firstSeen??now,lastSeen:now});
    this.identities=this.identities.slice(-this.maxIdentities);this.save();
  }
  recordResult(result) {
    if(this.results.some(x=>x.matchId===result.matchId))return false;
    this.results.push(structuredClone(result));this.results=this.results.slice(-this.maxResults);this.save();return true;
  }
  resultsFor(identity) {
    return this.results.filter(x=>x.participants?.some(p=>p.identity===identity)).slice(-20).map(x=>{
      const slot=x.participants.find(p=>p.identity===identity).slot;
      return {matchId:x.matchId,endedAt:x.endedAt,training:x.training,mapHash:x.mapHash,mapVersion:x.mapVersion,winner:x.winner,player:x.players?.find(p=>p.id===slot)};
    });
  }
}
