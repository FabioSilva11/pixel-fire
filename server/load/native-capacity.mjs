import { createRequire } from 'node:module';
import { performance } from 'node:perf_hooks';
const native=createRequire(import.meta.url)('../native/build/zzbr.node');
const reports=[];
for(const count of [2,8,20,40,50]) {
  const handle=native.create(1,12345,0,.1,5,5);
  for(let i=0;i<count;i++)native.join(handle,i,i,i,1);
  const times=[];let output=0;
  // Simulates 90 seconds per size including combat, zones, loot, and viewer-specific snapshots at10Hz.
  for(let tick=0;tick<1800;tick++) {
    const begin=performance.now();native.step(handle,.05);
    if(tick%2===0)for(let id=0;id<count;id++)output+=native.snapshot(handle,id).byteLength+4;
    times.push(performance.now()-begin);
  }
  times.sort((a,b)=>a-b);
  const report={players:count,p95TickMs:times[Math.floor(times.length*.95)],p99TickMs:times[Math.floor(times.length*.99)],maxTickMs:times.at(-1),
    rssMiB:process.memoryUsage().rss/1048576,simulationSeconds:90,snapshotBytesPerSecond:Math.round(output/90)};
  reports.push(report);console.log(JSON.stringify(report));
  if(report.p99TickMs>40||report.rssMiB>500){console.log('Admission increase stopped: insufficient margin');break;}
}
