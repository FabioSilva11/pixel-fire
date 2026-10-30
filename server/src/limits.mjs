// Token bucket with a bounded origin table. The peer address is never taken from proxy headers.
export class RateLimiter {
  constructor({capacity,refillPerSecond,maxKeys=5000,idleMs=120000,now=()=>Date.now()}) {
    this.capacity=capacity;this.refill=refillPerSecond/1000;this.maxKeys=maxKeys;this.idleMs=idleMs;this.now=now;this.entries=new Map();
  }
  allow(key,cost=1) {
    const now=this.now();let entry=this.entries.get(key);
    if(!entry){if(this.entries.size>=this.maxKeys)return false;entry={tokens:this.capacity,at:now};this.entries.set(key,entry);}
    entry.tokens=Math.min(this.capacity,entry.tokens+Math.max(0,now-entry.at)*this.refill);entry.at=now;
    if(entry.tokens<cost)return false;entry.tokens-=cost;return true;
  }
  prune(){const now=this.now();for(const [key,value] of this.entries)if(now-value.at>=this.idleMs)this.entries.delete(key);}
}
