import test from 'node:test';
import assert from 'node:assert/strict';
import {ABNORMALITIES,PREFERENCES,NO_REGULAR_WORK} from '../data/catalog.mjs';
import {RECOMMENDATIONS} from '../data/recommendations.mjs';
import {EGO_LIMITS} from '../data/ego_limits.mjs';
import {EGO_NAMES} from '../data/ego_names.mjs';
import {SEARCH_PINYIN} from '../data/search_pinyin.mjs';

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

test('推荐表与 E.G.O 上限覆盖所有异想体',()=>{
  const ids=ABNORMALITIES.map(a=>a.id);
  assert.deepEqual(Object.keys(RECOMMENDATIONS).sort(),[...ids].sort());
  assert.deepEqual(Object.keys(EGO_LIMITS).sort(),[...ids].sort());
  assert.deepEqual(Object.keys(EGO_NAMES).sort(),[...ids].sort());
  assert.deepEqual(Object.keys(SEARCH_PINYIN).sort(),[...ids].sort());
  for(const id of ids){
    const recommendation=RECOMMENDATIONS[id],ego=EGO_LIMITS[id],names=EGO_NAMES[id];
    for(const field of ['containment','weapon','suit','suppression'])
      assert.equal(typeof recommendation[field],'string',`${id} ${field}`);
    for(const field of ['weapon','suit'])
      assert.ok(Number.isInteger(ego[field])&&ego[field]>=0&&ego[field]<=5,`${id} ${field}`);
    for(const field of ['weapon','suit'])
      assert.equal(Boolean(names[field]),ego[field]>0,`${id} ${field} name`);
    for(const field of ['abnormality','weapon']){
      assert.match(SEARCH_PINYIN[id][field].full,/^[a-z0-9]*$/);
      assert.match(SEARCH_PINYIN[id][field].initials,/^[a-z0-9]*$/);
    }
    if(recommendation.weapon==='-')assert.equal(ego.weapon,0,id);
    if(recommendation.suit==='-')assert.equal(ego.suit,0,id);
  }
  assert.deepEqual(EGO_LIMITS['O-03-03'],{weapon:5,suit:5});
  assert.deepEqual(EGO_LIMITS['O-02-63'],{weapon:1,suit:1});
  assert.equal(EGO_NAMES['O-03-03'].weapon,'忏悔');
  assert.equal(SEARCH_PINYIN['F-01-02'].abnormality.full,'jiaohuashaonv');
});
