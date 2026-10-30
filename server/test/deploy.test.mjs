import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readdirSync,readFileSync } from 'node:fs';
test('Linux deployment scripts retain executable LF shebangs after Windows edits',()=>{
  const directory=new URL('../deploy/',import.meta.url);
  for(const file of readdirSync(directory).filter(name=>name.endsWith('.sh'))){
    const source=readFileSync(new URL(file,directory),'utf8');
    assert(source.startsWith('#!/bin/sh\n'),file);assert(!source.includes('\r'),file);
  }
});
