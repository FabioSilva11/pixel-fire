import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
test('README describes current online mode and Android version',()=>{
  const root=new URL('../../',import.meta.url);
  const readme=readFileSync(new URL('README.md',root),'utf8');
  const gradle=readFileSync(new URL('app/build.gradle.kts',root),'utf8');
  const version=/versionName\s*=\s*"([^"]+)"/.exec(gradle)[1],code=/versionCode\s*=\s*(\d+)/.exec(gradle)[1];
  assert(readme.includes(`Versão \`${version}\` (versionCode ${code})`));
  assert.match(readme,/Battle Royale online/i);assert.match(readme,/Solo, Duo e Squad/i);
  assert(!readme.includes('### PvP em Wi-Fi local'));
});
