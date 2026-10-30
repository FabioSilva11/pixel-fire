import { test } from 'node:test';
import assert from 'node:assert/strict';
import { generateKeyPairSync, sign } from 'node:crypto';
import { IdentityStore } from '../src/identity.mjs';
import { PartyStore } from '../src/party.mjs';
import { encodeInput, decodeInput, encodeSnapshot } from '../src/protocol.mjs';

test('identity challenge is single-use, bound to key and expires', () => {
  let now=1000;
  const store=new IdentityStore({now:()=>now});
  const {publicKey,privateKey}=generateKeyPairSync('ec',{namedCurve:'prime256v1'});
  const spki=publicKey.export({type:'spki',format:'der'}).toString('base64');
  const challenge=store.challenge(spki);
  const signature=sign('sha256',Buffer.from(challenge.nonce,'base64'),privateKey).toString('base64');
  const auth=store.authenticate(challenge.id,signature);
  assert.equal(store.resolve(auth.token).id,auth.playerId);
  assert.throws(()=>store.authenticate(challenge.id,signature));
  const stale=store.challenge(spki);now+=61000;
  assert.throws(()=>store.authenticate(stale.id,signature));
  now+=3600001;assert.throws(()=>store.resolve(auth.token));
  assert.throws(()=>store.challenge('not a key'));
});
test('party capacity, owner and queue locking prevent duplicate joins', () => {
  const store=new PartyStore();
  const party=store.create('a',2,true);
  assert.equal(store.join('b',party.code).members.length,2);
  assert.throws(()=>store.join('c',party.code));
  assert.throws(()=>store.lock('b'));
  store.lock('a');assert.throws(()=>store.leave('b'));assert.throws(()=>store.join('c',party.code));
  store.unlock(party.code);store.leave('a');
  assert.equal(store.forPlayer('b').owner,'b');
  assert.throws(()=>store.create('x',3,true));
});
test('binary input rejects malformed, nonfinite and client-authoritative state', () => {
  const packet=encodeInput({sequence:7,moveX:1,moveZ:0,yaw:2,pitch:.1,buttons:1,slot:-1,target:-1});
  assert.equal(decodeInput(packet).sequence,7);
  assert.throws(()=>decodeInput(packet.subarray(0,12)));
  const bad=Buffer.from(packet);bad.writeFloatLE(NaN,8);assert.throws(()=>decodeInput(bad));
  assert.throws(()=>encodeInput({sequence:1,moveX:0,moveZ:0,yaw:0,pitch:0,buttons:0,damage:500}));
  assert.throws(()=>encodeInput({sequence:1,moveX:0,moveZ:0,yaw:0,pitch:0,buttons:0,x:999}));
  const snap=encodeSnapshot(new Float32Array([1,2,3]));
  assert.equal(snap[0],2);assert.equal(snap.readFloatLE(4),1);
});
