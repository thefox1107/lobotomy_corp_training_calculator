import {writeFileSync} from 'node:fs';
import {ABNORMALITIES,PREFERENCES,NO_REGULAR_WORK} from '../data/catalog.mjs';
const wide=s=>'L"'+[...s].map(c=>{const n=c.codePointAt(0);return n<128&&c!=='"'&&c!=='\\'?c:n<=0xffff?'\\u'+n.toString(16).padStart(4,'0'):'\\U'+n.toString(16).padStart(8,'0')}).join('')+'"';
const level={ZAYIN:0,TETH:1,HE:2,WAW:3,ALEPH:4};
const rows=ABNORMALITIES.map(a=>{
  const p=PREFERENCES[a.id];
  const rates=p?.rates??Array.from({length:4},()=>Array(5).fill(-101));
  const matrix='{'+rates.map(r=>'{'+r.join(',')+'}').join(',')+'}';
  return `  {${wide(a.id)},${wide(a.name)},${level[a.level]},${p?.boxes??0},${NO_REGULAR_WORK.has(a.id)?'false':'true'},${matrix}},`;
});
writeFileSync(new URL('../native/catalog.hpp',import.meta.url),`#pragma once\n#include <array>\nstruct Abnormality { const wchar_t* id; const wchar_t* name; int level; int maxBoxes; bool regularWork; int rates[4][5]; };\nconstexpr Abnormality CATALOG[] = {\n${rows.join('\n')}\n};\nconstexpr int CATALOG_SIZE = sizeof(CATALOG)/sizeof(CATALOG[0]);\n`,'utf8');
console.log(`Generated ${rows.length} fixed-grade abnormalities`);
