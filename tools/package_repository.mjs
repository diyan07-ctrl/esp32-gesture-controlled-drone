import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';

// Dependency-free ZIP (stored entries); source snapshots never include .git or keys.
const root=process.cwd();
execFileSync(process.execPath,['tools/check_repository.mjs'],{stdio:'inherit'});
const excluded=new Set(['.git','.cache','.venv','.pio','node_modules','build','dist']);
function walk(dir){return fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>excluded.has(e.name)?[]:e.isDirectory()?walk(path.join(dir,e.name)):[path.join(dir,e.name)]);}
const files=walk(root).sort();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const entries=files.map(file=>({name:path.relative(root,file).replaceAll('\\','/'),data:fs.readFileSync(file)}));
const manifest={format:'Source snapshot; file hashes identify these contents independently of Git metadata',files:entries.map(e=>({path:e.name,bytes:e.data.length,sha256:hash(e.data)}))};
entries.push({name:'package-manifest.json',data:Buffer.from(JSON.stringify(manifest,null,2)+'\n')});
const table=Array.from({length:256},(_,n)=>{let c=n;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;return c>>>0;});
function crc32(b){let c=0xffffffff;for(const n of b)c=table[(c^n)&255]^(c>>>8);return (c^0xffffffff)>>>0;}
const locals=[],centrals=[];let offset=0;
for(const entry of entries){
 const name=Buffer.from('esp32-gesture-controlled-drone/'+entry.name),data=entry.data,crc=crc32(data);
 const local=Buffer.alloc(30);local.writeUInt32LE(0x04034b50,0);local.writeUInt16LE(20,4);local.writeUInt16LE(0x800,6);local.writeUInt16LE(23902,12); // 2026-10-30 DOS date
 local.writeUInt32LE(crc,14);local.writeUInt32LE(data.length,18);local.writeUInt32LE(data.length,22);local.writeUInt16LE(name.length,26);
 locals.push(local,name,data);
 const central=Buffer.alloc(46);central.writeUInt32LE(0x02014b50,0);central.writeUInt16LE(20,4);central.writeUInt16LE(20,6);central.writeUInt16LE(0x800,8);central.writeUInt16LE(23902,14);
 central.writeUInt32LE(crc,16);central.writeUInt32LE(data.length,20);central.writeUInt32LE(data.length,24);central.writeUInt16LE(name.length,28);central.writeUInt32LE(offset,42);
 centrals.push(central,name);offset+=local.length+name.length+data.length;
}
const centralBytes=Buffer.concat(centrals),end=Buffer.alloc(22);end.writeUInt32LE(0x06054b50,0);end.writeUInt16LE(entries.length,8);end.writeUInt16LE(entries.length,10);end.writeUInt32LE(centralBytes.length,12);end.writeUInt32LE(offset,16);
const archive=Buffer.concat([...locals,centralBytes,end]);
fs.mkdirSync('dist',{recursive:true});const output='dist/esp32-gesture-controlled-drone-source.zip';
fs.writeFileSync(output,archive,{flag:'wx'});
console.log(`Packaged ${files.length} source files plus manifest into ${output}`);
console.log(`SHA256 ${hash(archive)} (${archive.length} bytes)`);
