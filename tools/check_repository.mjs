import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';

const root=process.cwd();
const skip=new Set(['.git','.cache','.venv','.pio','node_modules','build','dist']);
function walk(dir){return fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>skip.has(e.name)?[]:e.isDirectory()?walk(path.join(dir,e.name)):[path.join(dir,e.name)]);}
const files=walk(root);
let checks=0;
function check(condition,message){checks++;assert.ok(condition,message);}
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
for(const file of files){
 const rel=path.relative(root,file),s=fs.readFileSync(file,'utf8');
 check(!/\.(bin|elf|map)$/i.test(file),`generated binary: ${rel}`);
 check(path.basename(file)!=='local_secrets.h',`private provisioning file: ${rel}`);
 check(!/(?:ghp_|github_pat_)[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/.test(s),`possible secret: ${rel}`);
 if(file.endsWith('.md')){
  for(const m of s.matchAll(/!?\[[^\]]*\]\(([^)]+)\)/g)){
   const target=m[1].split('#')[0];if(!target||/^(https?:|mailto:)/.test(target))continue;
   const resolved=path.resolve(path.dirname(file),decodeURIComponent(target));
   check(resolved.startsWith(root+path.sep)&&fs.existsSync(resolved),`broken/outside link ${rel}: ${target}`);
  }
  check((s.match(/^```/gm)||[]).length%2===0,`unclosed Markdown fence: ${rel}`);
 }
 if(file.endsWith('.mmd')){
  check(s.startsWith('flowchart '),`unexpected Mermaid format ${rel}`);
  check((s.match(/\[/g)||[]).length===(s.match(/\]/g)||[]).length,`unbalanced Mermaid node brackets ${rel}`);
 }
}
const config=read('firmware/common/configuration/config.h');
for(const flag of ['outputsVerified','powerVerified','propulsionVerified','controllerValidated'])check(config.includes(`${flag}=false`),`unsafe default ${flag}`);
check(config.includes('motors[4]={25,26,27,32}'),'motor GPIO code mismatch');
check(config.includes('sda=21, scl=22, battery=34'),'sensor/ADC pins mismatch');
check(config.includes('timeoutMs=200'),'command timeout mismatch');
const mapping=read('hardware/motor-mapping.csv').trim().split(/\r?\n/).slice(1).map(s=>s.split(','));
const expected=[['M1','front-left','ESC1','25','CW','1','1','1','-1'],['M2','front-right','ESC2','26','CCW','1','-1','1','1'],['M3','rear-right','ESC3','27','CW','1','-1','-1','-1'],['M4','rear-left','ESC4','32','CCW','1','1','-1','1']];
check(JSON.stringify(mapping)===JSON.stringify(expected),'motor mapping CSV mismatch');
check(read('firmware/common/control.h').includes('m.motor={t+r+p-y,t-r+p+y,t-r-p-y,t+r-p+y}'),'mixer coefficient mismatch');
const svg=read('diagrams/motor-layout.svg');
for(const label of ['M1 FL','M2 FR','M3 RR','M4 RL','GPIO25','GPIO26','GPIO27','GPIO32'])check(svg.includes(label),`missing motor SVG label ${label}`);
const main=read('firmware/drone/src/main.cpp');
const task=main.slice(main.indexOf('void controlTask'),main.indexOf('void setup'));
check(!/Serial\.|\bdelay\(/.test(task),'blocking serial/delay in control task');
check(task.includes('rx.accept(incoming.bytes,24,incoming.received'),'packet timestamp must be receive time');
check(task.includes('esc.write(motors,safety.armed())'),'output must respect supervisor');
check(read('firmware/common/esc.h').includes('if(!attached)return'),'ESC gate missing');
const ini=read('platformio.ini');
check(ini.includes('espressif32@6.9.0')&&ini.includes('[env:glove]')&&ini.includes('[env:drone]'),'build targets/platform pin missing');
const ci=read('.github/workflows/firmware-ci.yml');
check(ci.includes('pio run -e ${{ matrix.target }}')&&ci.includes('target: [glove, drone]'),'CI must build both apps');
for(const id of [...read('hardware/bom.csv').matchAll(/^"([A-Z]\d+)"/gm)].map(m=>m[1]))check(read('docs/bill-of-materials.md').includes('|'+id+'|'),`BOM document missing ${id}`);
check(Math.abs(11.1*2.2-24.42)<1e-9,'energy reference arithmetic');
check(Math.abs(12.6*10000/57000-2.210526315789474)<1e-9,'divider reference arithmetic');
check(12.6*10100/(46530+10100)<2.25,'divider tolerance reference');
console.log(`PASS: ${checks} repository/static assertions across ${files.length} files. This does not compile firmware or render Mermaid.`);
