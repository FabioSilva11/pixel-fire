export const VERSION=1,TYPE_INPUT=1,TYPE_SNAPSHOT=2,TYPE_PING=3,TYPE_PONG=4;
const keys=new Set(['sequence','moveX','moveZ','yaw','pitch','buttons','slot','target','dropItem','dropCount','markX','markZ']);
export function encodeInput(command) {
  for(const key of Object.keys(command))if(!keys.has(key))throw new Error('Campo não permitido');
  const values=[command.sequence,command.moveX,command.moveZ,command.yaw,command.pitch,command.buttons,command.slot??-1,command.target??-1,
    command.dropItem??0,command.dropCount??0,command.markX??-1,command.markZ??-1];
  const b=Buffer.alloc(52);b[0]=TYPE_INPUT;b[1]=VERSION;
  for(let i=0;i<12;i++)b.writeFloatLE(values[i],4+i*4);
  decodeInput(b);return b;
}
export function decodeInput(packet) {
  const b=Buffer.from(packet.buffer??packet,packet.byteOffset??0,packet.byteLength??packet.length);
  if(b.length!==52||b[0]!==TYPE_INPUT||b[1]!==VERSION||b[2]!==0||b[3]!==0)throw new Error('Pacote inválido');
  const v=Array.from({length:12},(_,i)=>b.readFloatLE(4+i*4));
  if(!v.every(Number.isFinite)||!Number.isInteger(v[0])||v[0]<1||v[0]>16777215||Math.abs(v[1])>1.001||Math.abs(v[2])>1.001||
     Math.abs(v[3])>10000||Math.abs(v[4])>1.55||!Number.isInteger(v[5])||v[5]<0||v[5]>32767||
     !Number.isInteger(v[6])||v[6]<-1||v[6]>=24||!Number.isInteger(v[7])||v[7]<-1||v[7]>=50||
     !Number.isInteger(v[8])||v[8]<0||v[8]>28||!Number.isInteger(v[9])||v[9]<0||v[9]>10000||
     !((v[10]===-1&&v[11]===-1)||(v[10]>=0&&v[10]<1440&&v[11]>=0&&v[11]<1440)))throw new Error('Comando inválido');
  return Object.fromEntries([...keys].map((key,i)=>[key,v[i]]));
}
export function inputFloats(c) {return new Float32Array([...keys].map(k=>c[k]));}
export function encodeSnapshot(floats) {
  if(!(floats instanceof Float32Array)||floats.length>24000||!floats.every(Number.isFinite))throw new Error('Snapshot inválido');
  const b=Buffer.alloc(4+floats.byteLength);b[0]=TYPE_SNAPSHOT;b[1]=VERSION;
  Buffer.from(floats.buffer,floats.byteOffset,floats.byteLength).copy(b,4);return b;
}
