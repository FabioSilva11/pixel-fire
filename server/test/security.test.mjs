import { test } from 'node:test';
import assert from 'node:assert/strict';
import { generateKeyPairSync, sign } from 'node:crypto';
import { mkdtempSync, rmSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { IdentityStore } from '../src/identity.mjs';
import { decodeInput, encodeInput } from '../src/protocol.mjs';

test('reauthentication replaces old session and expired resolve prunes token',()=>{
  let now=0;const store=new IdentityStore({now:()=>now});
  const pair=generateKeyPairSync('ec',{namedCurve:'prime256v1'}),key=pair.publicKey.export({type:'spki',format:'der'}).toString('base64');
  const auth=()=>{const c=store.challenge(key);return store.authenticate(c.id,sign('sha256',Buffer.from(c.nonce,'base64'),pair.privateKey).toString('base64'));};
  const first=auth(),second=auth();assert.equal(first.playerId,second.playerId);assert.throws(()=>store.resolve(first.token));
  now=3600001;assert.throws(()=>store.resolve(second.token));assert.equal(store.sessions.size,0);
});

test('map marks accept only the map bounds or paired unset sentinel',()=>{
  const input={sequence:1,moveX:0,moveZ:0,yaw:0,pitch:0,buttons:0};
  for(const [markX,markZ] of [[-2,0],[0,1440],[-1,10],[16777215,0]])assert.throws(()=>encodeInput({...input,markX,markZ}));
  assert.equal(decodeInput(encodeInput({...input,markX:1439,markZ:0})).markX,1439);
});

test('persistence retains bounded results and public identities across restart without session secrets',async()=>{
  const { PersistenceStore }=await import('../src/persistence.mjs');
  const dir=mkdtempSync(join(tmpdir(),'zz-persist-'));
  try {
    const path=join(dir,'state.json'),store=new PersistenceStore(path,{maxResults:2,maxIdentities:2});
    store.recordIdentity('a');store.recordIdentity('b');store.recordIdentity('c');
    for(const matchId of ['1','2','3'])store.recordResult({matchId,participants:[{identity:'a',slot:0}],winner:0});
    store.recordResult({matchId:'3',winner:99});
    const restored=new PersistenceStore(path,{maxResults:2,maxIdentities:2});
    assert.equal(restored.identities.length,2);assert.equal(restored.results.length,2);assert.equal(restored.results[1].winner,0);
    assert.equal(restored.resultsFor('a').length,2);assert.equal(restored.resultsFor('b').length,0);
    assert(!readFileSync(path,'utf8').includes('token'));
  } finally {rmSync(dir,{recursive:true,force:true});}
});

test('token bucket has bounded origins and replenishes without burst at window edge',async()=>{
  const { RateLimiter }=await import('../src/limits.mjs');let now=0;
  const limiter=new RateLimiter({capacity:2,refillPerSecond:1,maxKeys:2,now:()=>now});
  assert(limiter.allow('a'));assert(limiter.allow('a'));assert(!limiter.allow('a'));assert(limiter.allow('b'));assert(!limiter.allow('c'));
  now=999;assert(!limiter.allow('a'));now=1000;assert(limiter.allow('a'));assert(!limiter.allow('a'));
  now=120001;limiter.prune();assert(limiter.allow('c'));
});
