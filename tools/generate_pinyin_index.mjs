import {writeFileSync} from 'node:fs';
import {pinyin} from 'pinyin-pro';
import {ABNORMALITIES} from '../data/catalog.mjs';
import {EGO_NAMES} from '../data/ego_names.mjs';

function searchableParts(value){
  const syllables=pinyin(value,{toneType:'none',type:'array'});
  const clean=s=>s.toLowerCase().normalize('NFD').replace(/u\u0308/g,'v')
    .replace(/[\u0300-\u036f]/g,'').replace(/[^a-z0-9]/g,'');
  return {
    full:clean(syllables.join('')),
    initials:clean(syllables.map(s=>s[0]??'').join('')),
  };
}

const result={};
for(const abnormality of ABNORMALITIES){
  const weapon=EGO_NAMES[abnormality.id]?.weapon;
  if(weapon===undefined)throw Error(`Missing E.G.O name: ${abnormality.id}`);
  result[abnormality.id]={
    abnormality:searchableParts(abnormality.name),
    weapon:searchableParts(weapon),
  };
}
writeFileSync(new URL('../data/search_pinyin.mjs',import.meta.url),
  '// 由 tools/generate_pinyin_index.mjs 使用 pinyin-pro 3.29.5 生成；exe 离线搜索。\n'+
  'export const SEARCH_PINYIN = '+JSON.stringify(result,null,2)+';\n','utf8');
console.log(`Generated ${Object.keys(result).length} pinyin search entries`);
