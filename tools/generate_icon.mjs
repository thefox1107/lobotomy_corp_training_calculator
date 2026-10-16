import {writeFileSync} from 'node:fs';

function iconBitmap(size){
  const pixels=Buffer.alloc(size*size*4);
  for(let y=0;y<size;y++)for(let x=0;x<size;x++){
    const u=(x+.5)*64/size,v=(y+.5)*64/size;
    const cx=Math.max(9,Math.min(55,u)),cy=Math.max(9,Math.min(55,v));
    const opacity=Math.max(0,Math.min(1,9.5-Math.hypot(u-cx,v-cy)));
    let color=[34,42,46];
    if(u>=7&&u<12&&v>=11&&v<53)color=[170,68,58];
    const L=(u>=20&&u<25&&v>=19&&v<46)||(u>=20&&u<35&&v>=41&&v<46);
    const C=((v>=19&&v<24)||(v>=41&&v<46))&&u>=39&&u<54 ||
            (u>=39&&u<44&&v>=19&&v<46);
    if(L||C)color=[251,249,244];
    const i=((size-1-y)*size+x)*4;
    pixels[i]=color[2];pixels[i+1]=color[1];pixels[i+2]=color[0];
    pixels[i+3]=Math.round(opacity*255);
  }
  return pixels;
}
function entry(size){
  const bitmap=iconBitmap(size),maskRow=Math.ceil(size/32)*4;
  const header=Buffer.alloc(40);
  header.writeUInt32LE(40,0);
  header.writeInt32LE(size,4);
  header.writeInt32LE(size*2,8);
  header.writeUInt16LE(1,12);
  header.writeUInt16LE(32,14);
  header.writeUInt32LE(bitmap.length+maskRow*size,20);
  return Buffer.concat([header,bitmap,Buffer.alloc(maskRow*size)]);
}
const sizes=[16,32,48,64];
const payloads=sizes.map(entry);
const directory=Buffer.alloc(6+16*sizes.length);
directory.writeUInt16LE(1,2);
directory.writeUInt16LE(sizes.length,4);
let offset=directory.length;
for(let i=0;i<sizes.length;i++){
  const p=6+i*16,size=sizes[i];
  directory[p]=size;directory[p+1]=size;
  directory.writeUInt16LE(1,p+4);
  directory.writeUInt16LE(32,p+6);
  directory.writeUInt32LE(payloads[i].length,p+8);
  directory.writeUInt32LE(offset,p+12);
  offset+=payloads[i].length;
}
writeFileSync(new URL('../native/app.ico',import.meta.url),Buffer.concat([directory,...payloads]));
console.log('Generated native/app.ico');
