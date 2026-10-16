import test from 'node:test';
import assert from 'node:assert/strict';
import {ABNORMALITIES,PREFERENCES,NO_REGULAR_WORK} from '../data/catalog.mjs';

test('异想体目录完整且危险等级固定',()=>{
  assert.equal(ABNORMALITIES.length,84);
  assert.equal(new Set(ABNORMALITIES.map(a=>a.id)).size,84);
  assert.equal(Object.keys(PREFERENCES).length,65);
  assert.equal(NO_REGULAR_WORK.size,19);
  for(const a of ABNORMALITIES){
    assert.ok(['ZAYIN','TETH','HE','WAW','ALEPH'].includes(a.level),a.id);
    assert.notEqual(!!PREFERENCES[a.id],NO_REGULAR_WORK.has(a.id),a.id);
  }
  for(const [id,p] of Object.entries(PREFERENCES)){
    assert.equal(p.rates.length,4,id);
    for(const row of p.rates){
      assert.equal(row.length,5,id);
      for(const n of row)assert.ok(Number.isInteger(n)&&n>=-100&&n<=100,id);
    }
  }
  assert.deepEqual(PREFERENCES['O-03-03'].rates[0],[50,40,30,30,30]);
  assert.deepEqual(PREFERENCES['T-04-50'].rates[1],[0,0,55,55,60]);
});
