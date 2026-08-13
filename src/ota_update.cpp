#include "ota_update.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_app_format.h>
#include <esp_attr.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <xtensa/xtensa_api.h>

#include <algorithm>
#include <cstring>

#include "adsb_network.h"
#include "airport_store.h"
#include "build_info.h"
#include "mqtt_service.h"

namespace ota_update {
namespace {

constexpr uint16_t OTA_PORT = 80;
constexpr uint16_t PACKAGE_FORMAT_VERSION = 1;
constexpr uint16_t PACKAGE_HEADER_SIZE = 512;
constexpr uint32_t MINIMUM_FIRMWARE_SIZE = 64U * 1024U;
constexpr uint32_t PREPARE_TIMEOUT_MS = 45U * 1000U;
constexpr uint32_t RESTART_DELAY_MS = 1500U;
constexpr uint32_t RESTART_SETTLE_MS = 500U;
constexpr uint32_t RESTART_TASK_STACK_BYTES = 4096U;
constexpr uint32_t RESTART_LOOP_QUIESCE_TIMEOUT_MS = 1000U;
constexpr BaseType_t RESTART_TASK_CORE = 0;
constexpr BaseType_t RESTART_LOOP_CORE = 1;
constexpr UBaseType_t RESTART_TASK_PRIORITY = configMAX_PRIORITIES - 1;
constexpr char RESTART_TASK_NAME[] = "ota_restart";
constexpr uint32_t RESTART_LOOP_WAITING = 0;
constexpr uint32_t RESTART_LOOP_QUIESCED = 1;
constexpr uint32_t RESTART_LOOP_ABORTED = 2;
constexpr uint8_t ESP_APPLICATION_MAGIC = 0xE9;
constexpr uint16_t ESP32_S3_IMAGE_CHIP_ID = 9;
constexpr char PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA";
constexpr char HARDWARE_ID[32] = "WAVESHARE-ESP32-S3-LCD-7";
constexpr char OTA_HOSTNAME[] = "bills-aircraft-radar";
constexpr char ACCESS_CODE_HEADER[] = "X-OTA-Code";

#pragma pack(push, 1)
struct PackageHeader {
  char magic[16];
  uint16_t formatVersion;
  uint16_t headerSize;
  char hardwareId[32];
  char buildId[96];
  uint32_t firmwareSize;
  uint8_t firmwareSha256[32];
  uint8_t reserved[328];
};
#pragma pack(pop)

static_assert(sizeof(PackageHeader) == PACKAGE_HEADER_SIZE,
              "Radar OTA package header must remain 512 bytes");
static_assert(sizeof(esp_image_header_t) == 24,
              "Unexpected ESP application image header size");

const char UPDATE_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bill's Aircraft Radar Update</title>
<style>
:root{color-scheme:dark;font-family:Arial,sans-serif;background:#041019;color:#e1ebf0}
body{margin:0;display:grid;place-items:center;min-height:100vh;padding:18px;box-sizing:border-box}
main{width:min(620px,100%);background:#0a1821;border:1px solid #23505b;border-radius:12px;padding:24px;box-sizing:border-box}
h1{margin:0 0 6px;color:#6edcff;font-size:26px}.sub{color:#64aab5;margin-bottom:22px}
label{display:block;margin:14px 0 6px;color:#8bddea}input{width:100%;box-sizing:border-box;padding:11px;border-radius:7px;border:1px solid #286672;background:#0c1c26;color:#fff}
button,a.button{display:inline-block;margin-top:16px;padding:11px 16px;border:0;border-radius:7px;background:#188054;color:white;font-weight:700;cursor:pointer;text-decoration:none}button.secondary,a.secondary{background:#144452;margin-left:8px}button:disabled{opacity:.45;cursor:not-allowed}
progress{width:100%;height:20px;margin-top:18px}.status{min-height:54px;margin-top:16px;padding:12px;background:#07141c;border-radius:7px;white-space:pre-wrap}.warn{color:#ffd66a;font-size:13px;margin-top:18px}
</style>
</head>
<body><main>
<h1>BILL'S AIRCRAFT RADAR</h1><div class="sub">Local firmware update</div>
<label for="code">Six-digit access code shown on the radar</label><input id="code" inputmode="numeric" maxlength="6" autocomplete="one-time-code">
<label for="file">Radar OTA package</label><input id="file" type="file" accept=".radarota,application/octet-stream">
<button id="install">PREPARE &amp; INSTALL</button><button class="secondary" id="cancel">CANCEL OTA</button>
<a class="button secondary" href="/airports">AIRPORT DATABASE</a>
<progress id="progress" max="100" value="0"></progress><div class="status" id="status">Select the generated firmware.radarota file.</div>
<div class="warn">Do not remove power while firmware is being written. This page accepts only a Bill's 7-inch Radar .radarota package.</div>
<script>
const code=()=>document.getElementById('code').value.trim();
const status=document.getElementById('status'), progress=document.getElementById('progress'), install=document.getElementById('install');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function callOnce(path,options={}){options.headers=Object.assign({},options.headers||{}, {'X-OTA-Code':code()});options.cache='no-store';const r=await fetch(path,options);const t=await r.text();let j={message:t};try{j=JSON.parse(t)}catch(e){}if(!r.ok){const err=new Error(j.message||('HTTP '+r.status));err.httpStatus=r.status;throw err}return j}
async function call(path,options={},attempts=3){let lastError;for(let attempt=0;attempt<attempts;attempt++){try{return await callOnce(path,options)}catch(e){lastError=e;if(e.httpStatus||attempt+1>=attempts)throw e;await sleep(300*(attempt+1))}}throw lastError}
async function waitReady(){await sleep(500);for(let i=0;i<45;i++){const j=await call('/status',{},3);status.textContent=j.message||j.state;if(j.state==='READY')return j;if(j.state==='ERROR')throw new Error(j.message);await sleep(1000)}throw new Error('Radar did not enter update-ready state')}
async function prepareUpload(){return call('/prepare',{method:'POST'},3)}
function uploadOnce(file){return new Promise((resolve,reject)=>{const x=new XMLHttpRequest();x.open('POST','/upload');x.setRequestHeader('X-OTA-Code',code());x.upload.onprogress=e=>{if(e.lengthComputable)progress.value=Math.round(e.loaded*100/e.total)};x.onload=()=>{let j={message:x.responseText};try{j=JSON.parse(x.responseText)}catch(e){};x.status>=200&&x.status<300?resolve(j):reject(new Error(j.message||('HTTP '+x.status)))};x.onerror=()=>reject(new Error('Upload connection reset'));const f=new FormData();f.append('firmware',file,file.name);x.send(f)})}
async function upload(file){let lastError;for(let attempt=0;attempt<2;attempt++){try{return await uploadOnce(file)}catch(e){lastError=e;if(attempt>0)break;await sleep(500);const j=await call('/status',{},3);const received=Number(j.received_bytes||0),written=Number(j.written_bytes||0);if(j.state!=='READY'||received!==0||written!==0)throw new Error('Upload connection lost after transfer may have started; check radar status before retrying.');status.textContent='Upload connection reset before transfer; retrying once...';progress.value=0;await sleep(500)}}throw lastError}
install.onclick=async()=>{const file=document.getElementById('file').files[0];if(!/^\d{6}$/.test(code())){status.textContent='Enter the six-digit access code.';return}if(!file||!file.name.toLowerCase().endsWith('.radarota')){status.textContent='Choose firmware.radarota.';return}install.disabled=true;progress.value=0;try{status.textContent='Waiting for network services to become idle...';await prepareUpload();await waitReady();status.textContent='Uploading and validating firmware...';await sleep(500);const j=await upload(file);status.textContent=j.message||'Update complete. Radar is restarting.';progress.value=100}catch(e){status.textContent=e.message}finally{install.disabled=false}};
document.getElementById('cancel').onclick=async()=>{try{const j=await call('/cancel',{method:'POST'});status.textContent=j.message}catch(e){status.textContent=e.message}};
</script></main></body></html>
)HTML";

const char AIRPORT_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Bill's Aircraft Radar Airports</title>
<style>
:root{color-scheme:dark;font-family:Arial,sans-serif;background:#041019;color:#e1ebf0}
*{box-sizing:border-box}body{margin:0;min-height:100vh;padding:max(14px,env(safe-area-inset-top)) max(14px,env(safe-area-inset-right)) max(18px,env(safe-area-inset-bottom)) max(14px,env(safe-area-inset-left));display:flex;justify-content:center}
main{width:min(720px,100%);align-self:flex-start;background:#0a1821;border:1px solid #23505b;border-radius:14px;padding:clamp(16px,4vw,26px)}
h1{margin:0 0 5px;color:#6edcff;font-size:clamp(23px,6vw,30px)}.sub{color:#64aab5;margin-bottom:18px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:14px 0}.card{background:#07141c;border:1px solid #163b45;border-radius:9px;padding:12px}.k{color:#64aab5;font-size:12px;text-transform:uppercase}.v{font-size:16px;margin-top:4px;overflow-wrap:anywhere}
.section{margin-top:18px;padding-top:16px;border-top:1px solid #163b45}.section h2{font-size:18px;color:#8bddea;margin:0 0 8px}.hint{color:#7faab2;font-size:13px;line-height:1.4}
label{display:block;margin:13px 0 7px;color:#8bddea;font-weight:700}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}input,select{width:100%;font-size:16px;padding:14px;border-radius:8px;border:1px solid #286672;background:#0c1c26;color:#fff}
button,a.button{width:100%;display:block;margin-top:12px;padding:15px 16px;min-height:50px;border:0;border-radius:8px;background:#188054;color:white;font-size:16px;font-weight:800;text-align:center;text-decoration:none;cursor:pointer}a.secondary,button.secondary{background:#144452}button.ghost{background:#102b35}button:disabled{opacity:.45;cursor:not-allowed}
progress{width:100%;height:22px;margin-top:18px}.status{min-height:70px;margin-top:14px;padding:13px;background:#07141c;border-radius:8px;white-space:pre-wrap;overflow-wrap:anywhere}.warn{color:#ffd66a;font-size:13px;line-height:1.45;margin-top:16px}details{margin-top:18px}summary{cursor:pointer;color:#8bddea;font-weight:700;padding:8px 0}
@media(max-width:500px){.grid,.row{grid-template-columns:1fr}main{border-radius:10px}}
</style>
</head>
<body><main>
<h1>BILL'S AIRCRAFT RADAR</h1><div class="sub">Airport Database</div>
<div class="grid"><div class="card"><div class="k">Persistent store</div><div id="store" class="v">Enter access code</div></div><div class="card"><div class="k">Installed data date</div><div id="region" class="v">—</div></div><div class="card"><div class="k">Records</div><div id="records" class="v">—</div></div><div class="card"><div class="k">Coverage</div><div id="coverage" class="v">—</div></div></div>
<label for="code">Six-digit access code shown on the radar</label><input id="code" inputmode="numeric" pattern="[0-9]*" maxlength="6" autocomplete="one-time-code">

<div class="section"><h2>Build for this location</h2><div class="hint">Your browser downloads the public OurAirports data and builds the regional package locally. The center coordinates are used only for filtering and are not stored in the package.</div>
<div class="row"><div><label for="lat">Latitude</label><input id="lat" inputmode="decimal" placeholder="42.83047"></div><div><label for="lon">Longitude</label><input id="lon" inputmode="decimal" placeholder="-88.16204"></div></div>
<div class="row"><div><label for="radius">Coverage radius</label><select id="radius"><option value="120" selected>120 miles — recommended</option><option value="90">90 miles</option><option value="160">160 miles</option><option value="200">200 miles</option><option value="300">300 miles</option><option value="500">500 miles</option></select></div><div><label for="name">Region name</label><input id="name" maxlength="63" placeholder="HOME REGION"></div></div>
<button id="buildInstall">BUILD &amp; INSTALL AIRPORT DATABASE</button><button id="download" class="ghost" disabled>DOWNLOAD GENERATED PACKAGE</button>
</div>

<details><summary>Advanced: install an existing .radarapt file</summary><label for="file">Airport package (.radarapt)</label><input id="file" type="file" accept=".radarapt,application/octet-stream"><button id="manualInstall" class="secondary">INSTALL SELECTED PACKAGE</button></details>
<a class="button secondary" href="/update">FIRMWARE UPDATE</a>
<progress id="progress" max="100" value="0"></progress><div class="status" id="status">Enter the access code and location, then tap BUILD &amp; INSTALL.</div>
<div class="warn">Airport data is awareness-only, not for navigation. During installation the complete package is buffered in PSRAM and validated before flash is erased. Restart the radar after a successful install to activate the new region.</div>
<script>
// AIRPORT_BROWSER_BUILDER_BEGIN
const AIRPORTS_URL='https://raw.githubusercontent.com/davidmegginson/ourairports-data/main/airports.csv';
const RUNWAYS_URL='https://raw.githubusercontent.com/davidmegginson/ourairports-data/main/runways.csv';
const EARTH_RADIUS_MILES=3958.7613, RECORD_SIZE=55, HEADER_SIZE=256, GENERATOR_VERSION=3;
function ascii(s){return String(s||'').normalize('NFKD').replace(/[^\x00-\x7F]/g,'')}
function cleanIdent(s){return ascii(s).toUpperCase().trim().replace(/[^A-Z0-9-]/g,'').slice(0,7)}
function cleanText(s,max){return ascii(s).toUpperCase().replace(/[\\"]/g,' ').trim().replace(/\s+/g,' ').slice(0,max)}
function nfloat(v){const n=Number(v);return Number.isFinite(n)?n:null}
function nint(v,def=0){const n=nfloat(v);return n===null?def:Math.round(n)}
function distanceMiles(a,b,c,d){const p1=a*Math.PI/180,p2=c*Math.PI/180,da=(c-a)*Math.PI/180,dl=(d-b)*Math.PI/180;const x=Math.sin(da/2)**2+Math.cos(p1)*Math.cos(p2)*Math.sin(dl/2)**2;return 2*EARTH_RADIUS_MILES*Math.atan2(Math.sqrt(x),Math.sqrt(Math.max(0,1-x)))}
function csvRows(text,cb){let row=[],field='',quoted=false;for(let i=0;i<=text.length;i++){const ch=i<text.length?text[i]:'\n';if(quoted){if(ch==='"'&&text[i+1]==='"'){field+='"';i++}else if(ch==='"')quoted=false;else field+=ch}else if(ch==='"')quoted=true;else if(ch===','){row.push(field);field=''}else if(ch==='\n'){row.push(field.replace(/\r$/,''));field='';if(row.length>1||row[0]!=='')cb(row);row=[]}else field+=ch}}
function headers(row){const m={};row.forEach((v,i)=>m[v.replace(/^\uFEFF/,'')]=i);return m}
function val(row,h,k){const i=h[k];return i===undefined?'':(row[i]||'')}
function classify(row,h){const t=val(row,h,'type').trim().toLowerCase();if(t==='closed'||t==='seaplane_base'||t==='balloonport')return null;if(t==='heliport')return 3;if(t==='large_airport'||t==='medium_airport')return 0;if(t!=='small_airport')return null;const scheduled=val(row,h,'scheduled_service').trim().toLowerCase()==='yes',icao=cleanIdent(val(row,h,'icao_code')),gps=cleanIdent(val(row,h,'gps_code')),local=cleanIdent(val(row,h,'local_code'));return scheduled||icao.startsWith('K')||gps.startsWith('K')||(local&&local.length<=3)?1:2}
function runwayHeading(row,h){let x=nfloat(val(row,h,'le_heading_degT'));if(x===null)x=nfloat(val(row,h,'he_heading_degT'));if(x===null)return 0;const rounded=Math.round(x)%180;return rounded===0&&x>0?180:rounded}
function parseAirports(text,lat,lon,radius){let h=null,out=[],used=new Set(),source=0;csvRows(text,row=>{if(!h){h=headers(row);return}source++;const cat=classify(row,h);if(cat===null)return;const la=nfloat(val(row,h,'latitude_deg')),lo=nfloat(val(row,h,'longitude_deg'));if(la===null||lo===null||la < -90||la > 90||lo < -180||lo > 180)return;if(distanceMiles(lat,lon,la,lo)>radius)return;let ident='';for(const k of ['gps_code','icao_code','local_code','ident']){const c=cleanIdent(val(row,h,k));if(c&&!used.has(c)){ident=c;break}}if(!ident)return;const internal=val(row,h,'ident').trim(),name=cleanText(val(row,h,'name')||ident,31)||ident,elev=Math.max(-32768,Math.min(32767,nint(val(row,h,'elevation_ft'))));out.push({ident,name,latitude:la,longitude:lo,elevation:elev,runway_length:0,runway_heading:0,category:cat,internal});used.add(ident)});out.sort((a,b)=>a.ident.localeCompare(b.ident)||a.latitude-b.latitude||a.longitude-b.longitude);return {airports:out,source}}
function applyRunways(text,airports){const wanted=new Map(airports.map((a,i)=>[a.internal,i])),best=new Map();let h=null;csvRows(text,row=>{if(!h){h=headers(row);return}const id=val(row,h,'airport_ident').trim();if(!wanted.has(id))return;if(['1','true','yes'].includes(val(row,h,'closed').trim().toLowerCase()))return;const len=nint(val(row,h,'length_ft'));if(len<=0)return;const candidate={length:Math.min(65535,len),heading:Math.max(0,Math.min(360,runwayHeading(row,h)))};const old=best.get(id);if(!old||candidate.length>old.length||(candidate.length===old.length&&candidate.heading<old.heading))best.set(id,candidate)});for(const [id,r] of best){const a=airports[wanted.get(id)];a.runway_length=r.length;a.runway_heading=r.heading}return best.size}
function sha256(bytes){const K=new Uint32Array([0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2]);const l=bytes.length,bitHi=Math.floor(l/0x20000000),bitLo=(l<<3)>>>0,total=((l+9+63)>>6)<<6,msg=new Uint8Array(total);msg.set(bytes);msg[l]=0x80;const dv=new DataView(msg.buffer);dv.setUint32(total-8,bitHi,false);dv.setUint32(total-4,bitLo,false);let H=new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]),w=new Uint32Array(64);const rr=(x,n)=>(x>>>n)|(x<<(32-n));for(let o=0;o<total;o+=64){for(let i=0;i<16;i++)w[i]=dv.getUint32(o+i*4,false);for(let i=16;i<64;i++){const s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>>3),s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>>10);w[i]=(w[i-16]+s0+w[i-7]+s1)>>>0}let [a,b,c,d,e,f,g,h]=H;for(let i=0;i<64;i++){const S1=rr(e,6)^rr(e,11)^rr(e,25),ch=(e&f)^((~e)&g),t1=(h+S1+ch+K[i]+w[i])>>>0,S0=rr(a,2)^rr(a,13)^rr(a,22),maj=(a&b)^(a&c)^(b&c),t2=(S0+maj)>>>0;h=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b;b=a;a=(t1+t2)>>>0}H[0]=(H[0]+a)>>>0;H[1]=(H[1]+b)>>>0;H[2]=(H[2]+c)>>>0;H[3]=(H[3]+d)>>>0;H[4]=(H[4]+e)>>>0;H[5]=(H[5]+f)>>>0;H[6]=(H[6]+g)>>>0;H[7]=(H[7]+h)>>>0}const out=new Uint8Array(32),od=new DataView(out.buffer);H.forEach((x,i)=>od.setUint32(i*4,x,false));return out}
function fixed(view,off,size,text,max){const b=new TextEncoder().encode(cleanText(text,max));for(let i=0;i<size;i++)view.setUint8(off+i,i<b.length?b[i]:0)}
function fixedIdent(view,off,text){const b=new TextEncoder().encode(cleanIdent(text));for(let i=0;i<8;i++)view.setUint8(off+i,i<b.length?b[i]:0)}
function buildRadarapt(airports,date,coverage,radius){if(!airports.length)throw new Error('No eligible airports were found in this radius.');if(airports.length>65535)throw new Error('Too many airports for the package format.');const payload=new Uint8Array(airports.length*RECORD_SIZE),v=new DataView(payload.buffer);airports.forEach((a,i)=>{const o=i*RECORD_SIZE;fixedIdent(v,o,a.ident);fixed(v,o+8,32,a.name,31);v.setInt32(o+40,Math.round(a.latitude*1e6),true);v.setInt32(o+44,Math.round(a.longitude*1e6),true);v.setInt16(o+48,a.elevation,true);v.setUint16(o+50,a.runway_length,true);v.setUint16(o+52,a.runway_heading,true);v.setUint8(o+54,a.category)});const digest=sha256(payload),all=new Uint8Array(HEADER_SIZE+payload.length),h=new DataView(all.buffer),magic=new TextEncoder().encode('BILLS-AIRPORTDB\0');all.set(magic,0);h.setUint16(16,1,true);h.setUint16(18,HEADER_SIZE,true);h.setUint16(20,RECORD_SIZE,true);h.setUint16(22,GENERATOR_VERSION,true);h.setUint32(24,airports.length,true);h.setUint16(28,radius,true);h.setUint16(30,0,true);fixed(h,32,16,date,15);fixed(h,48,64,coverage||'CUSTOM REGIONAL DATABASE',63);h.setUint32(112,payload.length,true);all.set(digest,116);all.set(payload,HEADER_SIZE);return all}
// AIRPORT_BROWSER_BUILDER_END
const el=id=>document.getElementById(id),code=()=>el('code').value.trim(),sleep=ms=>new Promise(r=>setTimeout(r,ms));let generated=null;
async function call(path,options={}){options.headers=Object.assign({},options.headers||{}, {'X-OTA-Code':code()});options.cache='no-store';const r=await fetch(path,options);const t=await r.text();let j={message:t};try{j=JSON.parse(t)}catch(e){}if(!r.ok){const e=new Error(j.message||('HTTP '+r.status));e.httpStatus=r.status;throw e}return j}
async function waitReady(){await sleep(400);for(let i=0;i<45;i++){const j=await call('/status');el('status').textContent=j.message||j.state;if(j.state==='READY')return;if(j.state==='ERROR')throw new Error(j.message);await sleep(1000)}throw new Error('Radar did not enter upload-ready state')}
async function refresh(){if(!/^\d{6}$/.test(code()))return;try{const j=await call('/airports/status');el('store').textContent=j.store_state;el('region').textContent=j.database_date||'Not installed';el('records').textContent=String(j.records||0);el('coverage').textContent=j.coverage||'—'}catch(e){el('status').textContent=e.message}}
function uploadBlob(blob,name='airports.radarapt'){return new Promise((resolve,reject)=>{const x=new XMLHttpRequest();x.open('POST','/airports/upload');x.setRequestHeader('X-OTA-Code',code());x.upload.onprogress=e=>{if(e.lengthComputable)el('progress').value=Math.round(e.loaded*100/e.total)};x.onload=()=>{let j={message:x.responseText};try{j=JSON.parse(x.responseText)}catch(e){};x.status>=200&&x.status<300?resolve(j):reject(new Error(j.message||('HTTP '+x.status)))};x.onerror=()=>reject(new Error('Upload connection reset'));const f=new FormData();f.append('airports',blob,name);x.send(f)})}
async function installBlob(blob){el('status').textContent='Waiting for network services to become idle...';await call('/prepare',{method:'POST'});await waitReady();el('status').textContent='Uploading and validating airport database...';const j=await uploadBlob(blob);el('status').textContent=j.message;el('progress').value=100;await refresh()}
async function fetchText(url,label){el('status').textContent='Downloading '+label+' in your browser...';const r=await fetch(url,{cache:'no-store',mode:'cors'});if(!r.ok)throw new Error(label+' download failed: HTTP '+r.status);return r.text()}
el('code').addEventListener('input',refresh);
el('buildInstall').onclick=async()=>{const lat=Number(el('lat').value),lon=Number(el('lon').value),radius=Number(el('radius').value),coverage=cleanText(el('name').value||'CUSTOM REGIONAL DATABASE',63);if(!/^\d{6}$/.test(code())){el('status').textContent='Enter the six-digit access code.';return}if(!Number.isFinite(lat)||lat < -90||lat > 90||!Number.isFinite(lon)||lon < -180||lon > 180){el('status').textContent='Enter valid latitude and longitude.';return}el('buildInstall').disabled=true;el('manualInstall').disabled=true;el('progress').value=0;try{const airportText=await fetchText(AIRPORTS_URL,'airport data (~13 MB)');el('status').textContent='Filtering airports for your region...';await sleep(0);const parsed=parseAirports(airportText,lat,lon,radius);if(!parsed.airports.length)throw new Error('No eligible airports found in that radius.');const runwayText=await fetchText(RUNWAYS_URL,'runway data (~4 MB)');el('status').textContent='Matching runways and building package...';await sleep(0);const runwayMatches=applyRunways(runwayText,parsed.airports),today=new Date().toISOString().slice(0,10),bytes=buildRadarapt(parsed.airports,today,coverage,radius);generated=new Blob([bytes],{type:'application/octet-stream'});el('download').disabled=false;el('status').textContent='Built '+parsed.airports.length+' airports ('+runwayMatches+' with runway details), '+Math.round(bytes.length/1024)+' KiB. Preparing radar install...';await installBlob(generated)}catch(e){el('status').textContent=e.message}finally{el('buildInstall').disabled=false;el('manualInstall').disabled=false}};
el('download').onclick=()=>{if(!generated)return;const a=document.createElement('a');a.href=URL.createObjectURL(generated);a.download='airports.radarapt';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)};
el('manualInstall').onclick=async()=>{const file=el('file').files[0];if(!/^\d{6}$/.test(code())){el('status').textContent='Enter the six-digit access code.';return}if(!file||!file.name.toLowerCase().endsWith('.radarapt')){el('status').textContent='Choose an airports.radarapt package.';return}el('buildInstall').disabled=true;el('manualInstall').disabled=true;el('progress').value=0;try{await installBlob(file)}catch(e){el('status').textContent=e.message}finally{el('buildInstall').disabled=false;el('manualInstall').disabled=false}};
</script></main></body></html>
)HTML";

WebServer server(OTA_PORT);
State currentState = State::UNAVAILABLE;
const esp_partition_t* updatePartition = nullptr;
bool routesConfigured = false;
bool serverRunning = false;
bool mdnsRunning = false;
bool stopServerPending = false;
bool releaseMaintenanceAfterStopPending = false;
bool exclusiveHoldConfirmed = false;
uint32_t armedUntilMs = 0;
uint32_t prepareDeadlineMs = 0;
uint32_t restartAtMs = 0;
uint32_t restartExecuteAtMs = 0;
TaskHandle_t restartTaskHandle = nullptr;
DRAM_ATTR uint32_t restartLoopState = RESTART_LOOP_WAITING;
bool restartTaskCreationAttempted = false;
char accessCode[7]{};
char statusMessage[128] = "OTA service unavailable";

PackageHeader packageHeader{};
uint8_t packageHeaderBytes[PACKAGE_HEADER_SIZE]{};
size_t packageHeaderReceived = 0;
uint8_t imagePrefix[sizeof(esp_image_header_t)]{};
size_t imagePrefixReceived = 0;
uint32_t payloadReceived = 0;
uint32_t payloadWritten = 0;
esp_ota_handle_t otaHandle = 0;
bool otaHandleActive = false;
mbedtls_sha256_context shaContext;
bool shaActive = false;
bool uploadAccepted = false;
uint8_t buildMatchFailure[sizeof(packageHeader.buildId)]{};
size_t buildPatternLength = 0;
size_t buildMatchLength = 0;
bool buildIdSeen = false;
int uploadResponseCode = 500;
char uploadResponseMessage[128] = "Upload did not complete";

uint8_t* airportUploadBuffer = nullptr;
size_t airportUploadCapacity = 0;
size_t airportUploadReceived = 0;
bool airportUploadAccepted = false;
int airportUploadResponseCode = 500;
char airportUploadResponseMessage[160] = "Airport upload did not complete";

void setMessage(const char* message) {
  snprintf(statusMessage, sizeof(statusMessage), "%s",
           message ? message : "");
}

void setUploadResponse(int code, const char* message) {
  uploadResponseCode = code;
  snprintf(uploadResponseMessage, sizeof(uploadResponseMessage), "%s",
           message ? message : "");
}

void setAirportUploadResponse(int code, const char* message) {
  airportUploadResponseCode = code;
  snprintf(airportUploadResponseMessage, sizeof(airportUploadResponseMessage),
           "%s", message ? message : "");
}

void resetAirportUploadSession() {
  if (airportUploadBuffer) {
    heap_caps_free(airportUploadBuffer);
    airportUploadBuffer = nullptr;
  }
  airportUploadCapacity = 0;
  airportUploadReceived = 0;
  airportUploadAccepted = false;
  setAirportUploadResponse(500, "Airport upload did not complete");
}

bool codeMatches() {
  if (!server.hasHeader(ACCESS_CODE_HEADER)) return false;
  const String supplied = server.header(ACCESS_CODE_HEADER);
  return supplied.length() == 6 && supplied.equals(accessCode);
}

void sendJson(int code, const char* message) {
  char body[384];
  const uint8_t progress = packageHeader.firmwareSize
      ? static_cast<uint8_t>(std::min<uint32_t>(
            100U, (payloadWritten * 100U) / packageHeader.firmwareSize))
      : 0;
  snprintf(body, sizeof(body),
           "{\"state\":\"%s\",\"message\":\"%s\","
           "\"progress\":%u,\"received_bytes\":%lu,"
           "\"written_bytes\":%lu,\"firmware_bytes\":%lu,"
           "\"build\":\"%s\"}",
           stateName(currentState), message ? message : statusMessage,
           static_cast<unsigned>(progress),
           static_cast<unsigned long>(payloadReceived),
           static_cast<unsigned long>(payloadWritten),
           static_cast<unsigned long>(packageHeader.firmwareSize), BUILD_ID);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendAirportStatus(int code, const char* message) {
  char coverage[65]{};
  const char* sourceCoverage = airport_store::databaseCoverage();
  for (size_t i = 0; sourceCoverage && sourceCoverage[i] && i < sizeof(coverage) - 1; ++i) {
    const char c = sourceCoverage[i];
    coverage[i] = (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20U)
        ? '_' : c;
  }
  char date[17]{};
  const char* sourceDate = airport_store::databaseDate();
  for (size_t i = 0; sourceDate && sourceDate[i] && i < sizeof(date) - 1; ++i) {
    const char c = sourceDate[i];
    date[i] = (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20U)
        ? '_' : c;
  }
  char body[512];
  snprintf(body, sizeof(body),
           "{\"message\":\"%s\",\"store_state\":\"%s\","
           "\"records\":%lu,\"radius_miles\":%u,"
           "\"database_date\":\"%s\",\"coverage\":\"%s\","
           "\"max_package_bytes\":%lu,\"received_bytes\":%lu}",
           message ? message : "",
           airport_store::stateName(),
           static_cast<unsigned long>(airport_store::recordCount()),
           static_cast<unsigned>(airport_store::radiusMiles()),
           date, coverage,
           static_cast<unsigned long>(airport_store::maxPackageSize()),
           static_cast<unsigned long>(airportUploadReceived));
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void resetUploadSession() {
  if (otaHandleActive) {
    esp_ota_abort(otaHandle);
    otaHandleActive = false;
  }
  if (shaActive) {
    mbedtls_sha256_free(&shaContext);
    shaActive = false;
  }
  memset(&packageHeader, 0, sizeof(packageHeader));
  memset(packageHeaderBytes, 0, sizeof(packageHeaderBytes));
  memset(imagePrefix, 0, sizeof(imagePrefix));
  packageHeaderReceived = 0;
  imagePrefixReceived = 0;
  payloadReceived = 0;
  payloadWritten = 0;
  otaHandle = 0;
  uploadAccepted = false;
  memset(buildMatchFailure, 0, sizeof(buildMatchFailure));
  buildPatternLength = 0;
  buildMatchLength = 0;
  buildIdSeen = false;
  setUploadResponse(500, "Upload did not complete");
}

void releaseMaintenanceHold() {
  adsb::releaseMaintenanceHold();
  mqtt_service::releaseMaintenanceHold();
}

void failUpload(const char* message, int responseCode = 400) {
  if (otaHandleActive) {
    esp_ota_abort(otaHandle);
    otaHandleActive = false;
  }
  if (shaActive) {
    mbedtls_sha256_free(&shaContext);
    shaActive = false;
  }
  uploadAccepted = false;
  currentState = State::ERROR;
  setMessage(message);
  setUploadResponse(responseCode, message);
  Serial.printf("OTA update failed: %s\n", message ? message : "unknown");
  Serial.println(
      "OTA exclusive hold remains active for a bounded retry or cancellation");
}

void prepareBuildIdentityMatcher() {
  buildPatternLength = 0;
  while (buildPatternLength < sizeof(packageHeader.buildId) &&
         packageHeader.buildId[buildPatternLength] != 0) {
    ++buildPatternLength;
  }
  buildMatchLength = 0;
  buildIdSeen = false;
  memset(buildMatchFailure, 0, sizeof(buildMatchFailure));
  for (size_t index = 1, prefix = 0; index < buildPatternLength; ++index) {
    while (prefix > 0 &&
           packageHeader.buildId[index] != packageHeader.buildId[prefix]) {
      prefix = buildMatchFailure[prefix - 1];
    }
    if (packageHeader.buildId[index] == packageHeader.buildId[prefix]) {
      ++prefix;
    }
    buildMatchFailure[index] = static_cast<uint8_t>(prefix);
  }
}

void observeBuildIdentity(const uint8_t* data, size_t length) {
  if (buildIdSeen || buildPatternLength == 0) return;
  for (size_t index = 0; index < length; ++index) {
    const char current = static_cast<char>(data[index]);
    while (buildMatchLength > 0 &&
           current != packageHeader.buildId[buildMatchLength]) {
      buildMatchLength = buildMatchFailure[buildMatchLength - 1];
    }
    if (current == packageHeader.buildId[buildMatchLength]) {
      ++buildMatchLength;
      if (buildMatchLength == buildPatternLength) {
        buildIdSeen = true;
        return;
      }
    }
  }
}

bool validatePackageHeader() {
  memcpy(&packageHeader, packageHeaderBytes, sizeof(packageHeader));
  if (memcmp(packageHeader.magic, PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC)) != 0) {
    failUpload("Not a Bill's Radar OTA package");
    return false;
  }
  if (packageHeader.formatVersion != PACKAGE_FORMAT_VERSION ||
      packageHeader.headerSize != PACKAGE_HEADER_SIZE) {
    failUpload("Unsupported radar OTA package version");
    return false;
  }
  if (packageHeader.hardwareId[sizeof(packageHeader.hardwareId) - 1] != 0 ||
      strcmp(packageHeader.hardwareId, HARDWARE_ID) != 0) {
    failUpload("Package is for different hardware");
    return false;
  }
  if (packageHeader.buildId[sizeof(packageHeader.buildId) - 1] != 0 ||
      strncmp(packageHeader.buildId, "7IN-", 4) != 0) {
    failUpload("Package build identity is invalid");
    return false;
  }
  prepareBuildIdentityMatcher();
  if (!updatePartition ||
      packageHeader.firmwareSize < MINIMUM_FIRMWARE_SIZE ||
      packageHeader.firmwareSize > updatePartition->size) {
    failUpload("Firmware does not fit the inactive OTA slot", 413);
    return false;
  }

  const esp_err_t beginResult =
      esp_ota_begin(updatePartition, packageHeader.firmwareSize, &otaHandle);
  if (beginResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "OTA partition begin failed: %s",
             esp_err_to_name(beginResult));
    failUpload(message, 500);
    return false;
  }
  otaHandleActive = true;
  mbedtls_sha256_init(&shaContext);
  if (mbedtls_sha256_starts(&shaContext, 0) != 0) {
    failUpload("SHA-256 initialization failed", 500);
    return false;
  }
  shaActive = true;
  Serial.printf("OTA package accepted: build=%s, firmware=%lu bytes\n",
                packageHeader.buildId,
                static_cast<unsigned long>(packageHeader.firmwareSize));
  return true;
}

bool validateImagePrefix() {
  esp_image_header_t imageHeader{};
  memcpy(&imageHeader, imagePrefix, sizeof(imageHeader));
  if (imageHeader.magic != ESP_APPLICATION_MAGIC) {
    failUpload("Firmware payload has invalid ESP image magic");
    return false;
  }
  if (imageHeader.chip_id != ESP32_S3_IMAGE_CHIP_ID) {
    failUpload("Firmware payload is not for ESP32-S3");
    return false;
  }
  return true;
}

bool writeOtaBytes(const uint8_t* data, size_t length) {
  if (!length) return true;
  const esp_err_t result = esp_ota_write(otaHandle, data, length);
  if (result != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Firmware write failed: %s",
             esp_err_to_name(result));
    failUpload(message, 500);
    return false;
  }
  payloadWritten += static_cast<uint32_t>(length);
  return true;
}

bool processPayload(const uint8_t* data, size_t length) {
  if (!length) return true;
  if (!shaActive || !otaHandleActive) {
    failUpload("OTA writer was not initialized", 500);
    return false;
  }
  if (payloadReceived > packageHeader.firmwareSize ||
      length > packageHeader.firmwareSize - payloadReceived) {
    failUpload("Package contains more firmware data than declared");
    return false;
  }
  observeBuildIdentity(data, length);
  if (mbedtls_sha256_update(&shaContext, data, length) != 0) {
    failUpload("SHA-256 update failed", 500);
    return false;
  }
  payloadReceived += static_cast<uint32_t>(length);

  if (imagePrefixReceived < sizeof(imagePrefix)) {
    const size_t needed = sizeof(imagePrefix) - imagePrefixReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(imagePrefix + imagePrefixReceived, data, copyLength);
    imagePrefixReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (imagePrefixReceived < sizeof(imagePrefix)) return true;
    if (!validateImagePrefix()) return false;
    if (!writeOtaBytes(imagePrefix, sizeof(imagePrefix))) return false;
  }
  return writeOtaBytes(data, length);
}

bool processUploadBytes(const uint8_t* data, size_t length) {
  if (!uploadAccepted) return false;
  if (packageHeaderReceived < PACKAGE_HEADER_SIZE) {
    const size_t needed = PACKAGE_HEADER_SIZE - packageHeaderReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(packageHeaderBytes + packageHeaderReceived, data, copyLength);
    packageHeaderReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (packageHeaderReceived == PACKAGE_HEADER_SIZE &&
        !validatePackageHeader()) {
      return false;
    }
  }
  if (length && packageHeaderReceived == PACKAGE_HEADER_SIZE) {
    return processPayload(data, length);
  }
  return true;
}

void finishUpload(size_t totalPackageBytes) {
  if (!uploadAccepted) return;
  if (packageHeaderReceived != PACKAGE_HEADER_SIZE || !otaHandleActive ||
      !shaActive) {
    failUpload("Upload ended before the package header was complete");
    return;
  }
  const uint32_t expectedPackageSize =
      PACKAGE_HEADER_SIZE + packageHeader.firmwareSize;
  if (totalPackageBytes != expectedPackageSize ||
      payloadReceived != packageHeader.firmwareSize ||
      payloadWritten != packageHeader.firmwareSize) {
    failUpload("Firmware package is truncated or has an incorrect length");
    return;
  }
  if (!buildIdSeen) {
    failUpload("Firmware payload does not contain the declared build ID");
    return;
  }

  uint8_t actualSha256[32]{};
  if (mbedtls_sha256_finish(&shaContext, actualSha256) != 0) {
    failUpload("SHA-256 finalization failed", 500);
    return;
  }
  mbedtls_sha256_free(&shaContext);
  shaActive = false;
  if (memcmp(actualSha256, packageHeader.firmwareSha256,
             sizeof(actualSha256)) != 0) {
    failUpload("Firmware SHA-256 does not match the package header");
    return;
  }

  const esp_err_t endResult = esp_ota_end(otaHandle);
  otaHandleActive = false;
  if (endResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "ESP image validation failed: %s",
             esp_err_to_name(endResult));
    failUpload(message);
    return;
  }
  const esp_err_t bootResult = esp_ota_set_boot_partition(updatePartition);
  if (bootResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Boot partition update failed: %s",
             esp_err_to_name(bootResult));
    failUpload(message, 500);
    return;
  }

  uploadAccepted = false;
  currentState = State::SUCCESS;
  setMessage("Firmware verified. Radar is restarting.");
  setUploadResponse(200, statusMessage);
  restartAtMs = millis() + RESTART_DELAY_MS;
  restartExecuteAtMs = 0;
  Serial.printf("OTA update verified: %s (%lu bytes); restart scheduled\n",
                packageHeader.buildId,
                static_cast<unsigned long>(packageHeader.firmwareSize));
}

void handleUploadData() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      resetUploadSession();
      if (!codeMatches()) {
        setUploadResponse(403, "Access code rejected");
        return;
      }
      if (currentState != State::READY || !adsb::maintenanceHoldActive() ||
          !mqtt_service::maintenanceHoldActive()) {
        setUploadResponse(409, "Radar is not ready for firmware upload");
        return;
      }
      if (!upload.filename.endsWith(".radarota")) {
        setUploadResponse(415, "Select a .radarota package");
        return;
      }
      uploadAccepted = true;
      currentState = State::UPLOADING;
      setMessage("Receiving and validating firmware");
      setUploadResponse(500, "Upload did not complete");
      Serial.printf("OTA upload started: %s\n", upload.filename.c_str());
      break;

    case UPLOAD_FILE_WRITE:
      if (uploadAccepted && upload.currentSize) {
        processUploadBytes(upload.buf, upload.currentSize);
        delay(0);
      }
      break;

    case UPLOAD_FILE_END:
      finishUpload(upload.totalSize);
      break;

    case UPLOAD_FILE_ABORTED:
      if (uploadAccepted || currentState == State::UPLOADING) {
        failUpload("Firmware upload was interrupted");
      } else {
        setUploadResponse(400, "Firmware upload was interrupted");
      }
      break;
  }
}

void handleAirportUploadData() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START: {
      resetAirportUploadSession();
      if (!codeMatches()) {
        setAirportUploadResponse(403, "Access code rejected");
        return;
      }
      if (currentState != State::READY || !adsb::maintenanceHoldActive() ||
          !mqtt_service::maintenanceHoldActive()) {
        setAirportUploadResponse(409,
                                 "Radar is not ready for airport upload");
        return;
      }
      if (!upload.filename.endsWith(".radarapt")) {
        setAirportUploadResponse(415, "Select an airports.radarapt package");
        return;
      }
      airportUploadCapacity = airport_store::maxPackageSize();
      if (airportUploadCapacity == 0) {
        setAirportUploadResponse(503, "Airport partition is unavailable");
        return;
      }
      airportUploadBuffer = static_cast<uint8_t*>(heap_caps_malloc(
          airportUploadCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (!airportUploadBuffer) {
        airportUploadCapacity = 0;
        setAirportUploadResponse(507,
                                 "PSRAM allocation for airport upload failed");
        return;
      }
      airportUploadAccepted = true;
      currentState = State::UPLOADING;
      setMessage("Receiving airport database into PSRAM");
      setAirportUploadResponse(500, "Airport upload did not complete");
      Serial.printf("Airport upload started: %s, capacity=%u bytes\n",
                    upload.filename.c_str(),
                    static_cast<unsigned>(airportUploadCapacity));
      break;
    }

    case UPLOAD_FILE_WRITE:
      if (!airportUploadAccepted || upload.currentSize == 0) break;
      if (airportUploadReceived > airportUploadCapacity ||
          upload.currentSize > airportUploadCapacity - airportUploadReceived) {
        airportUploadAccepted = false;
        setAirportUploadResponse(413,
                                 "Airport package exceeds partition capacity");
        setMessage("Airport package is too large");
        currentState = State::ERROR;
        break;
      }
      memcpy(airportUploadBuffer + airportUploadReceived,
             upload.buf, upload.currentSize);
      airportUploadReceived += upload.currentSize;
      delay(0);
      break;

    case UPLOAD_FILE_END: {
      if (!airportUploadAccepted || !airportUploadBuffer) break;
      if (upload.totalSize != airportUploadReceived) {
        airportUploadAccepted = false;
        setAirportUploadResponse(400,
                                 "Airport package upload length mismatch");
        setMessage("Airport upload was truncated");
        currentState = State::ERROR;
        break;
      }

      char installError[160]{};
      const bool installed = airport_store::installPackage(
          airportUploadBuffer, airportUploadReceived,
          installError, sizeof(installError));
      if (!installed) {
        airportUploadAccepted = false;
        setAirportUploadResponse(
            400, installError[0] ? installError
                                 : "Airport package validation failed");
        setMessage("Airport database install failed");
        currentState = State::ERROR;
        Serial.printf("Airport install failed: %s\n",
                      installError[0] ? installError : "validation failure");
        break;
      }

      airportUploadAccepted = false;
      setAirportUploadResponse(
          200, "Airport database verified and installed. Restart radar to activate it.");
      setMessage("Airport database installed; restart radar to activate it");
      currentState = State::READY;
      Serial.printf(
          "Airport database installed: %lu bytes, %lu records, %u-mile region, %s, %s\n",
          static_cast<unsigned long>(airportUploadReceived),
          static_cast<unsigned long>(airport_store::recordCount()),
          static_cast<unsigned>(airport_store::radiusMiles()),
          airport_store::databaseDate(), airport_store::databaseCoverage());
      break;
    }

    case UPLOAD_FILE_ABORTED:
      airportUploadAccepted = false;
      setAirportUploadResponse(400, "Airport upload was interrupted");
      setMessage("Airport upload was interrupted; stored database unchanged");
      currentState = State::ERROR;
      break;
  }
}

void handlePrepare() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  if (currentState == State::PREPARING) {
    sendJson(202, statusMessage);
    return;
  }
  if (currentState == State::READY) {
    sendJson(200, statusMessage);
    return;
  }
  if (currentState != State::ARMED && currentState != State::ERROR) {
    sendJson(409, "OTA service is not available for preparation");
    return;
  }
  resetUploadSession();
  resetAirportUploadSession();
  if (!adsb::requestMaintenanceHold()) {
    sendJson(409, "Wi-Fi recovery in progress; try preparation again");
    return;
  }
  mqtt_service::requestMaintenanceHold();
  currentState = State::PREPARING;
  prepareDeadlineMs = millis() + PREPARE_TIMEOUT_MS;
  setMessage("Waiting for network services to become idle");
  sendJson(202, statusMessage);
}

void handleStatus() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  sendJson(200, statusMessage);
}

void handleAirportStatus() {
  if (!codeMatches()) {
    sendAirportStatus(403, "Access code rejected");
    return;
  }
  sendAirportStatus(200, airport_store::ready()
      ? "Persistent airport database ready"
      : "Persistent airport database not installed");
}

void handleCancel() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  if (currentState == State::UPLOADING || currentState == State::SUCCESS) {
    sendJson(409, "An active operation cannot be cancelled");
    return;
  }
  resetAirportUploadSession();
  currentState = State::INACTIVE;
  armedUntilMs = 0;
  prepareDeadlineMs = 0;
  accessCode[0] = 0;
  setMessage("Local OTA disabled");
  sendJson(200, statusMessage);
  releaseMaintenanceAfterStopPending = true;
  stopServerPending = true;
}

void handleUploadComplete() {
  sendJson(uploadResponseCode, uploadResponseMessage);
}

void handleAirportUploadComplete() {
  const int responseCode = airportUploadResponseCode;
  char responseMessage[sizeof(airportUploadResponseMessage)];
  snprintf(responseMessage, sizeof(responseMessage), "%s",
           airportUploadResponseMessage);
  sendAirportStatus(responseCode, responseMessage);
  resetAirportUploadSession();
}

void configureRoutes() {
  if (routesConfigured) return;
  const char* headerKeys[] = {ACCESS_CODE_HEADER};
  server.collectHeaders(headerKeys, 1);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Location", "/update", true);
    server.send(302, "text/plain", "");
  });
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", UPDATE_PAGE);
  });
  server.on("/airports", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", AIRPORT_PAGE);
  });
  server.on("/prepare", HTTP_POST, handlePrepare);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/airports/status", HTTP_GET, handleAirportStatus);
  server.on("/cancel", HTTP_POST, handleCancel);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadData);
  server.on("/airports/upload", HTTP_POST,
            handleAirportUploadComplete, handleAirportUploadData);
  server.onNotFound([]() { sendJson(404, "Not found"); });
  routesConfigured = true;
}

void stopServer() {
  if (serverRunning) {
    server.stop();
    serverRunning = false;
  }
  if (mdnsRunning) {
    MDNS.end();
    mdnsRunning = false;
  }
  resetAirportUploadSession();
  stopServerPending = false;
}

__attribute__((noinline)) bool IRAM_ATTR parkCoreOneForRestart() {
  const uint32_t enabledInterrupts = xthal_get_intenable();
  xt_ints_off(0xFFFFFFFFU);
  uint32_t expectedState = RESTART_LOOP_WAITING;
  if (!__atomic_compare_exchange_n(
          &restartLoopState, &expectedState, RESTART_LOOP_QUIESCED, false,
          __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
    xt_ints_on(enabledInterrupts);
    return false;
  }

  for (;;) {
    __asm__ __volatile__("nop");
  }
}

void restartTask(void*) {
  const TickType_t waitStarted = xTaskGetTickCount();
  const TickType_t waitTicks =
      pdMS_TO_TICKS(RESTART_LOOP_QUIESCE_TIMEOUT_MS);
  for (;;) {
    const uint32_t state =
        __atomic_load_n(&restartLoopState, __ATOMIC_ACQUIRE);
    if (state == RESTART_LOOP_QUIESCED) break;
    if (state == RESTART_LOOP_ABORTED) {
      restartTaskHandle = nullptr;
      vTaskDeleteWithCaps(nullptr);
      return;
    }

    if (xTaskGetTickCount() - waitStarted >= waitTicks) {
      uint32_t expectedState = RESTART_LOOP_WAITING;
      if (__atomic_compare_exchange_n(
              &restartLoopState, &expectedState, RESTART_LOOP_ABORTED, false,
              __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
        restartTaskHandle = nullptr;
        setMessage(
            "Firmware verified; loop shutdown failed. Power-cycle the radar.");
        Serial.println(
            "OTA restart task stopped: Core-1 loop did not quiesce; firmware "
            "remains verified; automatic retry disabled");
        vTaskDeleteWithCaps(nullptr);
        return;
      }
      continue;
    }
    vTaskDelay(1);
  }

  const UBaseType_t stackHighWaterMark =
      uxTaskGetStackHighWaterMark(nullptr);
  Serial.printf(
      "OTA restart task: name=%s core=%d stack-hwm-bytes=%u\n",
      pcTaskGetName(nullptr), static_cast<int>(xPortGetCoreID()),
      static_cast<unsigned>(stackHighWaterMark));
  Serial.flush();
  esp_restart();
}

void createRestartTaskOnce() {
  if (restartTaskCreationAttempted) return;
  restartTaskCreationAttempted = true;

  const bool heapIntegrityOk = heap_caps_check_integrity_all(true);
  const size_t freeInternal = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestInternal = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const UBaseType_t loopStack = uxTaskGetStackHighWaterMark(nullptr);
  const TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
  const UBaseType_t idle0Stack =
      idle0 ? uxTaskGetStackHighWaterMark(idle0) : 0;
  const TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCPU(1);
  const UBaseType_t idle1Stack =
      idle1 ? uxTaskGetStackHighWaterMark(idle1) : 0;
#if (INCLUDE_xTaskGetHandle == 1)
  const TaskHandle_t espTimer = xTaskGetHandle("esp_timer");
  const UBaseType_t espTimerStack =
      espTimer ? uxTaskGetStackHighWaterMark(espTimer) : 0;
  const bool espTimerStackAvailable = espTimer != nullptr;
#else
  const UBaseType_t espTimerStack = 0;
  const bool espTimerStackAvailable = false;
#endif
  Serial.printf(
      "OTA restart resources: integrity=%s heap=%u block=%u psram=%u "
      "loop-stack=%u idle0-stack=%u idle1-stack=%u esp-timer-stack=%u "
      "esp-timer-available=%s\n",
      heapIntegrityOk ? "ok" : "FAILED",
      static_cast<unsigned>(freeInternal),
      static_cast<unsigned>(largestInternal), ESP.getFreePsram(),
      static_cast<unsigned>(loopStack), static_cast<unsigned>(idle0Stack),
      static_cast<unsigned>(idle1Stack),
      static_cast<unsigned>(espTimerStack),
      espTimerStackAvailable ? "yes" : "no");

  if (!heapIntegrityOk) {
    setMessage(
        "Firmware verified; heap integrity failed. Power-cycle the radar.");
    Serial.println(
        "OTA restart task not created: heap integrity failed; firmware "
        "remains verified; automatic retry disabled");
    return;
  }

  if (xPortGetCoreID() != RESTART_LOOP_CORE) {
    setMessage(
        "Firmware verified; restart caller core is unsafe. Power-cycle the radar.");
    Serial.printf(
        "OTA restart task not created: caller core=%d expected=%d; firmware "
        "remains verified; automatic retry disabled\n",
        static_cast<int>(xPortGetCoreID()),
        static_cast<int>(RESTART_LOOP_CORE));
    return;
  }
  const TaskHandle_t loopTaskHandle = xTaskGetCurrentTaskHandle();

  const BaseType_t result = xTaskCreatePinnedToCoreWithCaps(
      restartTask, RESTART_TASK_NAME, RESTART_TASK_STACK_BYTES, nullptr,
      RESTART_TASK_PRIORITY, &restartTaskHandle, RESTART_TASK_CORE,
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (result != pdPASS) {
    restartTaskHandle = nullptr;
    setMessage(
        "Firmware verified; automatic restart failed. Power-cycle the radar.");
    Serial.printf(
        "OTA restart task creation failed: result=%ld; firmware remains "
        "verified; automatic retry disabled\n",
        static_cast<long>(result));
    return;
  }
  Serial.printf(
      "OTA restart task created: name=%s core=%d stack-bytes=%u; "
      "parking loop name=%s core=%d\n",
      RESTART_TASK_NAME, static_cast<int>(RESTART_TASK_CORE),
      static_cast<unsigned>(RESTART_TASK_STACK_BYTES),
      pcTaskGetName(loopTaskHandle),
      static_cast<int>(xPortGetCoreID()));
  Serial.flush();

  if (!parkCoreOneForRestart()) {
    Serial.println(
        "OTA restart loop park cancelled: restart task timed out; firmware "
        "remains verified; automatic retry disabled");
    return;
  }
}

}  // namespace

const char* stateName(State state) {
  switch (state) {
    case State::UNAVAILABLE: return "UNAVAILABLE";
    case State::INACTIVE: return "DISABLED";
    case State::ARMED: return "ARMED";
    case State::PREPARING: return "PREPARING";
    case State::READY: return "READY";
    case State::UPLOADING: return "UPLOADING";
    case State::ERROR: return "ERROR";
    case State::SUCCESS: return "SUCCESS";
    default: return "UNKNOWN";
  }
}

bool begin() {
  configureRoutes();
  updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (!updatePartition || updatePartition->size < MINIMUM_FIRMWARE_SIZE) {
    currentState = State::UNAVAILABLE;
    setMessage("No usable inactive OTA partition");
    Serial.println("OTA unavailable: no usable inactive OTA partition");
    return false;
  }
  currentState = State::INACTIVE;
  setMessage("Local OTA is disabled");
  Serial.printf("OTA ready: inactive partition %s, %lu bytes\n",
                updatePartition->label,
                static_cast<unsigned long>(updatePartition->size));
  return true;
}

bool enable() {
  if (!updatePartition || currentState == State::UNAVAILABLE || busy()) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    currentState = State::ERROR;
    setMessage("Wi-Fi must be connected before enabling OTA");
    return false;
  }

  if (!adsb::requestMaintenanceHold()) {
    currentState = State::ERROR;
    setMessage("Wi-Fi recovery in progress; try enabling OTA again");
    return false;
  }
  mqtt_service::requestMaintenanceHold();

  resetUploadSession();
  resetAirportUploadSession();
  airport_store::initialize();
  snprintf(accessCode, sizeof(accessCode), "%06lu",
           static_cast<unsigned long>(esp_random() % 1000000U));
  armedUntilMs = millis() + ENABLE_WINDOW_MS;
  prepareDeadlineMs = 0;
  restartAtMs = 0;
  restartExecuteAtMs = 0;
  restartTaskHandle = nullptr;
  __atomic_store_n(
      &restartLoopState, RESTART_LOOP_WAITING, __ATOMIC_RELEASE);
  restartTaskCreationAttempted = false;
  releaseMaintenanceAfterStopPending = false;
  exclusiveHoldConfirmed = false;
  currentState = State::ARMED;
  setMessage("Pausing background network services for OTA");

  if (!serverRunning) {
    server.begin();
    serverRunning = true;
  }
  if (!mdnsRunning && MDNS.begin(OTA_HOSTNAME)) {
    MDNS.addService("http", "tcp", OTA_PORT);
    mdnsRunning = true;
  }
  Serial.printf("OTA enabled for five minutes: http://%s/update\n",
                WiFi.localIP().toString().c_str());
  Serial.printf("Airport database page: http://%s/airports\n",
                WiFi.localIP().toString().c_str());
  Serial.println(
      "OTA exclusive window requested: ADS-B and MQTT are pausing");
  return true;
}

void disable() {
  if (currentState == State::UPLOADING || currentState == State::SUCCESS) return;
  resetUploadSession();
  resetAirportUploadSession();
  stopServer();
  releaseMaintenanceHold();
  releaseMaintenanceAfterStopPending = false;
  exclusiveHoldConfirmed = false;
  armedUntilMs = 0;
  prepareDeadlineMs = 0;
  accessCode[0] = 0;
  currentState = updatePartition ? State::INACTIVE : State::UNAVAILABLE;
  setMessage(updatePartition ? "Local OTA is disabled"
                             : "OTA service unavailable");
  Serial.println("OTA disabled");
}

bool busy() {
  return currentState == State::PREPARING || currentState == State::READY ||
         currentState == State::UPLOADING || currentState == State::SUCCESS;
}

void service() {
  if (serverRunning) server.handleClient();
  if (stopServerPending) {
    stopServer();
    if (releaseMaintenanceAfterStopPending) {
      releaseMaintenanceAfterStopPending = false;
      releaseMaintenanceHold();
      exclusiveHoldConfirmed = false;
      Serial.println(
          "OTA exclusive hold released after HTTP server and mDNS stopped");
    }
  }

  const uint32_t now = millis();
  const bool exclusiveHoldActive =
      adsb::maintenanceHoldActive() &&
      mqtt_service::maintenanceHoldActive();
  if (serverRunning && currentState == State::ARMED &&
      exclusiveHoldActive && !exclusiveHoldConfirmed) {
    exclusiveHoldConfirmed = true;
    setMessage("Background network services paused; OTA window ready");
    Serial.println(
        "OTA exclusive window active: ADS-B parked and MQTT resources released");
  }
  if (currentState == State::PREPARING && exclusiveHoldActive) {
    exclusiveHoldConfirmed = true;
    currentState = State::READY;
    setMessage("Radar ready. Upload may begin.");
    Serial.println("OTA network maintenance holds active; upload permitted");
  }
  if ((currentState == State::PREPARING || currentState == State::READY) &&
      prepareDeadlineMs && (int32_t)(now - prepareDeadlineMs) >= 0) {
    currentState = State::ARMED;
    prepareDeadlineMs = 0;
    setMessage(
        "Upload preparation expired; background services remain paused");
    Serial.println(
        "OTA preparation expired; exclusive hold remains active until window close");
  }
  if (serverRunning && currentState != State::UPLOADING &&
      currentState != State::SUCCESS && armedUntilMs &&
      (int32_t)(now - armedUntilMs) >= 0) {
    disable();
  }
  if (serverRunning && currentState != State::UPLOADING &&
      currentState != State::SUCCESS && WiFi.status() != WL_CONNECTED) {
    resetUploadSession();
    resetAirportUploadSession();
    stopServer();
    releaseMaintenanceHold();
    releaseMaintenanceAfterStopPending = false;
    exclusiveHoldConfirmed = false;
    armedUntilMs = 0;
    prepareDeadlineMs = 0;
    currentState = State::ERROR;
    setMessage("OTA disabled because Wi-Fi disconnected");
    Serial.println(
        "OTA exclusive hold released after external Wi-Fi disconnect");
  }
  if (restartAtMs && (int32_t)(now - restartAtMs) >= 0) {
    restartAtMs = 0;
    stopServer();
    armedUntilMs = 0;
    prepareDeadlineMs = 0;
    accessCode[0] = 0;
    restartExecuteAtMs = now + RESTART_SETTLE_MS;
    Serial.println(
        "OTA restart shutdown: HTTP server and mDNS stopped; settling network tasks");
  }
  if (restartExecuteAtMs &&
      (int32_t)(now - restartExecuteAtMs) >= 0) {
    restartExecuteAtMs = 0;
    createRestartTaskOnce();
  }
}

void copyStatus(Status& status) {
  status = Status{};
  status.state = currentState;
  status.available = updatePartition != nullptr;
  status.serverRunning = serverRunning;
  status.maintenanceActive = adsb::maintenanceHoldActive() &&
                             mqtt_service::maintenanceHoldActive();
  status.firmwareBytes = packageHeader.firmwareSize;
  status.writtenBytes = payloadWritten;
  status.progressPercent = packageHeader.firmwareSize
      ? static_cast<uint8_t>(std::min<uint32_t>(
            100U, (payloadWritten * 100U) / packageHeader.firmwareSize))
      : 0;
  if (armedUntilMs && currentState != State::INACTIVE &&
      currentState != State::UNAVAILABLE) {
    const int32_t remaining = static_cast<int32_t>(armedUntilMs - millis());
    status.secondsRemaining = remaining > 0
        ? static_cast<uint32_t>(remaining) / 1000U
        : 0;
  }
  snprintf(status.accessCode, sizeof(status.accessCode), "%s", accessCode);
  if (serverRunning && WiFi.status() == WL_CONNECTED) {
    snprintf(status.ipAddress, sizeof(status.ipAddress), "http://%s/update",
             WiFi.localIP().toString().c_str());
    if (mdnsRunning) {
      snprintf(status.mdnsAddress, sizeof(status.mdnsAddress),
               "http://%s.local/update", OTA_HOSTNAME);
    }
  }
  snprintf(status.message, sizeof(status.message), "%s", statusMessage);
}

}  // namespace ota_update
