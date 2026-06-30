#pragma once
#include <pgmspace.h>

static const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ERT Receiver Heltec V2</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Courier New',monospace;background:#060d14;color:#00e5ff;min-height:100vh}
.bar{display:flex;align-items:center;gap:12px;padding:10px 18px;background:#0a1a24;border-bottom:2px solid #00bcd4;flex-wrap:wrap}
.bar h1{font-size:13px;color:#fff;text-transform:uppercase;letter-spacing:2px}
.badge{padding:3px 10px;border-radius:10px;border:1px solid #00bcd4;font-size:11px;color:#00e5ff;background:#041824}
.main{padding:14px 18px;display:flex;flex-direction:column;gap:12px}
.panel{background:#07121a;border:1px solid #0a3040;border-radius:6px;padding:12px}
.pt{font-size:10px;color:#fff;text-transform:uppercase;letter-spacing:2px;margin-bottom:10px;border-bottom:1px solid #0a3040;padding-bottom:5px}
.row{display:flex;align-items:center;gap:10px;margin-bottom:8px;flex-wrap:wrap}
.lbl{font-size:10px;color:#005f7a;text-transform:uppercase;min-width:120px}
input[type=number],input[type=range],input[type=text]{background:#000;color:#00e5ff;border:1px solid #0a3040;padding:4px 7px;font-family:inherit;font-size:12px;border-radius:3px;outline:none}
input[type=number]{width:110px}input[type=range]{width:160px}input[type=text]{width:90px}
input:focus{border-color:#00bcd4}
.btn{padding:6px 14px;background:#041824;border:1px solid #00bcd4;color:#00e5ff;font-family:inherit;font-size:11px;font-weight:bold;cursor:pointer;border-radius:3px}
.btn:hover{filter:brightness(1.4)}
.btn-apply{border-color:#00ff88;color:#00ff88}
.btn-refresh{border-color:#ffaa00;color:#ffaa00}
.stat{display:flex;gap:20px;flex-wrap:wrap;margin-top:4px}
.si{display:flex;flex-direction:column;gap:2px}
.sl{font-size:9px;color:#005f7a;text-transform:uppercase;letter-spacing:1px}
.sv{font-size:13px;font-weight:bold}
table{width:100%;border-collapse:collapse;font-size:11px}
th{background:#0a1a24;padding:6px 8px;text-align:left;color:#fff;border-bottom:1px solid #0a3040}
td{padding:5px 8px;border-bottom:1px solid #061018;color:#00e5ff}
tr:hover td{background:#0a1a24}
.ok{color:#00ff88}.no{color:#ff4444}
input[type=checkbox].mqttcb,input[type=checkbox].selcb{accent-color:#00ff88;width:15px;height:15px;cursor:pointer;vertical-align:middle}
.ssedot{width:8px;height:8px;border-radius:50%;background:#555;display:inline-block;margin-right:6px}
.ssedot.on{background:#00ff88}
#log{background:#000;padding:8px;border-radius:4px;border:1px solid #061018;max-height:160px;overflow-y:auto;font-size:10px;color:#00bcd4;line-height:1.6}
</style></head><body>
<div class="bar">
  <h1>&#x2B22; Heltec V2 SX1276 915MHz HA Decoder</h1>
  <span class="badge" id="fwBadge">—</span>
  <span class="badge" id="sseBadge"><span class="ssedot" id="sseDot"></span>SSE</span>
  <span class="badge" id="mqttBadge"><span class="ssedot" id="mqttDot"></span>MQTT</span>
  <span class="badge" id="rssiLive">RSSI —</span>
  <span class="badge" id="chipBadge" title="SX1276 RegVersion check (must be 0x12)">CHIP —</span>
  <span class="badge" id="ctrBadge" title="Library callbacks → filters → accepted">RAW:0 / OK:0</span>
  <span class="badge" id="nvsBadge">NVS —</span>
  <span class="badge" id="uptimeBadge">Up —</span>
  <span class="badge" id="healthBadge">SYS <span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:#555;vertical-align:middle"></span></span>
</div>
<div class="main">

  <div class="panel" id="errorsPanel" style="display:none;border-color:#ff4444">
    <div class="pt" style="color:#ff4444">&#x26A0; System Errors</div>
    <div id="errorsContent" style="font-size:12px;line-height:2.4"></div>
  </div>

  <div class="panel">
    <div class="pt">Settings</div>
    <div class="row">
      <span class="lbl">ESP32 Name</span>
      <input type="text" id="espNameIn" value="" maxlength="31" placeholder="Heltec V2" style="width:160px;color:#888" readonly>
      <button class="btn" id="espNameBtn" style="border-color:#00bcd4;color:#00e5ff" onclick="espNameToggle()">&#9998; Edit</button>
      <span id="espNameNote" style="font-size:10px;color:#005f7a;margin-left:8px"></span>
    </div>
    <div class="row">
      <span class="lbl">Frequency (MHz)</span>
      <input type="number" id="freqIn" value="914.224" step="0.001" min="300" max="928">
      <button class="btn btn-apply" onclick="applyFreq()">Apply</button>
    </div>
    <div class="row">
      <span class="lbl">RX Bandwidth</span>
      <select id="bwSel" style="background:#000;color:#00e5ff;border:1px solid #0a3040;padding:4px 7px;font-family:inherit;font-size:12px;border-radius:3px;outline:none">
        <option value="0">250 kHz  (reg 0x01, widest — best for ERT freq-hop)</option>
        <option value="1">200 kHz  (reg 0x09)</option>
        <option value="2">167 kHz  (reg 0x11)</option>
        <option value="3">125 kHz  (reg 0x02)</option>
        <option value="4">100 kHz  (reg 0x0A)</option>
        <option value="5">83 kHz   (reg 0x12)</option>
        <option value="6">62.5 kHz (reg 0x03, narrowest)</option>
      </select>
      <button class="btn btn-apply" onclick="applyBw()">Apply</button>
    </div>
    <div class="row">
      <span class="lbl">RSSI Gate (dBm)</span>
      <input type="range" id="gateSlider" min="-120" max="-10" step="1" value="-95">
      <span id="gateDisp" style="font-size:12px;min-width:60px">-95 dBm</span>
      <button class="btn btn-apply" onclick="applyGate()">Apply</button>
    </div>
    <div class="row" style="margin-top:6px">
      <button class="btn btn-refresh" id="ookToggleBtn" onclick="toggleOokPanel()">&#9654; Show OOK &amp; RSSI Registers</button>
      <button class="btn" id="resetBtn" style="border-color:#ff4444;color:#ff4444;margin-left:8px" onclick="resetDefaults()">&#x21BA; Reset Defaults</button>
      <button class="btn btn-apply" id="ertOptBtn" style="margin-left:8px;border-color:#00ff88;color:#00ff88" onclick="applyErtOpt()">&#9889; Apply ERT-Optimized</button>
    </div>
    <div style="font-size:10px;color:#888;margin-top:4px">
      <b>ERT-Optimized</b> sets: BW=250 kHz (widest FSK) · BitRate=65.46 kBaud (2× ERT chip rate) ·
      RegRssiThresh=0xFF (no gate) · RegOokPeak=0x08 (Peak/BitSyncOff/0.5 dB) · RegOokFix=0x06 (6 dB floor).
    </div>
    <div class="stat" id="statBar">
      <div class="si"><div class="sl">Last Signal RSSI</div><div class="sv" id="sRssi">—</div></div>
      <div class="si"><div class="sl">Background RSSI</div><div class="sv" id="sAvgRssi">—</div></div>
      <div class="si"><div class="sl">Freq</div><div class="sv" id="sFreq">—</div></div>
      <div class="si"><div class="sl">Gate</div><div class="sv" id="sGate">—</div></div>
      <div class="si"><div class="sl">RX BW</div><div class="sv" id="sBw">—</div></div>
      <div class="si"><div class="sl">Good Decodes</div><div class="sv" id="sGood">—</div></div>
    </div>
  </div>

  <div class="panel" id="ookPanel" style="display:none">
    <div class="pt">SX1276 OOK &amp; RSSI Registers</div>
    <table style="width:100%;font-size:12px;border-collapse:collapse">
      <thead><tr style="color:#005f7a">
        <th style="text-align:left;padding:4px">Register</th>
        <th style="padding:4px">Addr</th>
        <th style="padding:4px">Live</th>
        <th style="text-align:left;padding:4px;min-width:340px">Write</th>
      </tr></thead>
      <tbody>
        <tr>
          <td style="padding:4px">RegRssiThresh<br><span style="font-size:9px;color:#005f7a">HW RSSI gate — signal must<br>exceed this floor to decode</span></td>
          <td style="padding:4px;text-align:center">0x10</td>
          <td style="padding:4px;text-align:center" id="livRssiThr">—</td>
          <td style="padding:4px">
            <select id="selRssiThr" style="width:100%;font-size:11px">
              <option value="0xFF">0xFF — −127.5 dBm (no gate; SX1276 chip default ✓)</option>
              <option value="0xF0">0xF0 — −120 dBm (effectively no gate)</option>
              <option value="0xE0">0xE0 — −112 dBm (≈ thermal floor at 250 kHz BW)</option>
              <option value="0xD0">0xD0 — −104 dBm</option>
              <option value="0xC8">0xC8 — −100 dBm</option>
              <option value="0xC0">0xC0 −  −96 dBm</option>
              <option value="0xB4">0xB4 −  −90 dBm</option>
              <option value="0xAA">0xAA −  −85 dBm</option>
              <option value="0xA0">0xA0 −  −80 dBm</option>
              <option value="0x96">0x96 −  −75 dBm</option>
              <option value="0x8C">0x8C −  −70 dBm</option>
              <option value="0x78">0x78 −  −60 dBm</option>
              <option value="0x64">0x64 −  −50 dBm (very tight; only loud signals)</option>
            </select>
            <button class="btn btn-apply" style="margin-top:3px" onclick="applyOokReg('0x10','selRssiThr')">Write</button>
          </td>
        </tr>
        <tr>
          <td style="padding:4px">RegOokPeak<br><span style="font-size:9px;color:#005f7a">Threshold mode + how fast the<br>decision level decays between pulses</span></td>
          <td style="padding:4px;text-align:center">0x14</td>
          <td style="padding:4px;text-align:center" id="livOokPeak">—</td>
          <td style="padding:4px">
            <select id="selOokPeak" style="width:100%;font-size:11px">
              <option value="0x00">0x00 — Fixed,   BitSync OFF                  (uses RegOokFix)</option>
              <option value="0x08">0x08 — Peak,    BitSync OFF, step 0.5 dB/chip (rtl_433_ESP library default ✓)</option>
              <option value="0x09">0x09 — Peak,    BitSync OFF, step 1.0 dB/chip</option>
              <option value="0x0A">0x0A — Peak,    BitSync OFF, step 1.5 dB/chip</option>
              <option value="0x0B">0x0B — Peak,    BitSync OFF, step 2.0 dB/chip</option>
              <option value="0x0C">0x0C — Peak,    BitSync OFF, step 3.0 dB/chip</option>
              <option value="0x0D">0x0D — Peak,    BitSync OFF, step 4.0 dB/chip</option>
              <option value="0x0E">0x0E — Peak,    BitSync OFF, step 5.0 dB/chip</option>
              <option value="0x0F">0x0F — Peak,    BitSync OFF, step 6.0 dB/chip</option>
              <option value="0x10">0x10 — Average, BitSync OFF                  (uses RegOokAvg)</option>
              <option value="0x20">0x20 — Fixed,   BitSync ON                   (uses RegOokFix)</option>
              <option value="0x28">0x28 — Peak,    BitSync ON,  step 0.5 dB/chip (Semtech chip default)</option>
              <option value="0x29">0x29 — Peak,    BitSync ON,  step 1.0 dB/chip</option>
              <option value="0x2A">0x2A — Peak,    BitSync ON,  step 1.5 dB/chip</option>
              <option value="0x2B">0x2B — Peak,    BitSync ON,  step 2.0 dB/chip</option>
              <option value="0x2C">0x2C — Peak,    BitSync ON,  step 3.0 dB/chip</option>
              <option value="0x2D">0x2D — Peak,    BitSync ON,  step 4.0 dB/chip</option>
              <option value="0x2E">0x2E — Peak,    BitSync ON,  step 5.0 dB/chip</option>
              <option value="0x2F">0x2F — Peak,    BitSync ON,  step 6.0 dB/chip</option>
              <option value="0x30">0x30 — Average, BitSync ON                   (uses RegOokAvg)</option>
            </select>
            <button class="btn btn-apply" style="margin-top:3px" onclick="applyOokReg('0x14','selOokPeak')">Write</button>
          </td>
        </tr>
        <tr>
          <td style="padding:4px">RegOokFix<br><span style="font-size:9px;color:#005f7a">Minimum floor (Peak/Avg mode)<br>or fixed threshold (Fixed mode)</span></td>
          <td style="padding:4px;text-align:center">0x15</td>
          <td style="padding:4px;text-align:center" id="livOokFix">—</td>
          <td style="padding:4px">
            <select id="selOokFix" style="width:100%;font-size:11px">
              <option value="0x01">0x01 —  1 dB floor (minimum; almost no filtering)</option>
              <option value="0x03">0x03 —  3 dB floor</option>
              <option value="0x06">0x06 —  6 dB floor (most sensitive practical)</option>
              <option value="0x08">0x08 —  8 dB floor</option>
              <option value="0x0A">0x0A — 10 dB floor</option>
              <option value="0x0C">0x0C — 12 dB floor (SX1276 chip default)</option>
              <option value="0x0F">0x0F — 15 dB floor (rtl_433_ESP library default ✓)</option>
              <option value="0x12">0x12 — 18 dB floor</option>
              <option value="0x14">0x14 — 20 dB floor</option>
              <option value="0x19">0x19 — 25 dB floor</option>
              <option value="0x1E">0x1E — 30 dB floor</option>
              <option value="0x28">0x28 — 40 dB floor</option>
              <option value="0x32">0x32 — 50 dB floor (least sensitive practical)</option>
            </select>
            <button class="btn btn-apply" style="margin-top:3px" onclick="applyOokReg('0x15','selOokFix')">Write</button>
          </td>
        </tr>
        <tr>
          <td style="padding:4px">RegOokAvg<br><span style="font-size:9px;color:#005f7a">Peak threshold decay rate —<br>how fast threshold drops between bursts</span></td>
          <td style="padding:4px;text-align:center">0x16</td>
          <td style="padding:4px;text-align:center" id="livOokAvg">—</td>
          <td style="padding:4px">
            <select id="selOokAvg" style="width:100%;font-size:11px">
              <option value="0x12">0x12 — Decay 1× per chip      (rtl_433_ESP library default ✓)</option>
              <option value="0x32">0x32 — Decay 1× per 2 chips</option>
              <option value="0x52">0x52 — Decay 1× per 4 chips</option>
              <option value="0x72">0x72 — Decay 1× per 8 chips   (slowest; holds threshold longest)</option>
              <option value="0x92">0x92 — Decay 2× per chip</option>
              <option value="0xB2">0xB2 — Decay 4× per chip</option>
              <option value="0xD2">0xD2 — Decay 8× per chip</option>
              <option value="0xF2">0xF2 — Decay 16× per chip     (fastest; threshold tracks every chip)</option>
            </select>
            <button class="btn btn-apply" style="margin-top:3px" onclick="applyOokReg('0x16','selOokAvg')">Write</button>
          </td>
        </tr>
      </tbody>
    </table>
  </div>

  <div class="panel">
    <div class="pt">&#x2714; Selected Devices — MQTT Active</div>
    <table>
      <thead><tr>
        <th>MQTT</th><th>Custom Name</th><th>Unit</th><th>Scale</th><th></th><th>Last Seen</th><th>Model</th><th>ID</th><th>Consumption</th><th>RSSI</th><th>Msgs</th>
      </tr></thead>
      <tbody id="selBody"><tr><td colspan="11" style="color:#005f7a;text-align:center">No devices selected — check &#9744; a device below to enable MQTT</td></tr></tbody>
    </table>
  </div>

  <div class="panel">
    <div class="pt" style="display:flex;align-items:center;justify-content:space-between;gap:6px"><span>Other Detected Devices</span><span style="display:flex;gap:6px"><button class="btn" style="border-color:#00ff88;color:#00ff88;padding:3px 10px;font-size:10px" onclick="othAdd()">+ Add</button><button class="btn" style="border-color:#ff4444;color:#ff4444;padding:3px 10px;font-size:10px" onclick="othDelete()">&#x2715; Delete</button></span></div>
    <table>
      <thead><tr>
        <th>Sel</th><th>Last Seen</th><th>Model</th><th>ID</th><th>Consumption</th><th>RSSI</th><th>Messages</th>
      </tr></thead>
      <tbody id="othBody"><tr><td colspan="7" style="color:#005f7a;text-align:center">Waiting for decode events…</td></tr></tbody>
    </table>
  </div>

  <div class="panel">
    <div class="pt" style="display:flex;align-items:center;justify-content:space-between">
      <span>SX1276 Pulse Monitor — DIO0 / DIO1 / DIO2</span>
      <span><button id="pmBtn" class="btn" style="border-color:#ffaa00;color:#ffaa00;padding:3px 12px;font-size:10px" onclick="pmToggle()">Start Monitor</button></span>
    </div>
    <div style="font-size:10px;color:#888;margin-bottom:6px">
      Off by default — zero CPU overhead while disabled. When enabled, samples all three DIO pins at 20 kHz (50 µs per sample) on Core 1 (away from rtl_433 receiver on Core 0).
      Shows ~100 ms of recent activity. <span style="color:#0f0">Green</span>=DIO0 (packet IRQ), <span style="color:#0ff">Cyan</span>=DIO2 (OOK raw data — the one rtl_433_ESP listens to), <span style="color:#fa0">Orange</span>=DIO1 (timeout IRQ).
    </div>
    <canvas id="pmCanvas" width="1100" height="180" style="background:#000;border:1px solid #050;display:block;width:100%;max-width:1100px"></canvas>
    <div id="pmInfo" style="font-size:10px;color:#888;margin-top:4px">— disabled —</div>
  </div>

  <div class="panel">
    <div class="pt">Raw Decode Log</div>
    <div id="log"></div>
  </div>

</div>
<script>
let mqttSelected={},selectedDevices={},otherDevices={},deviceNamesMap={},deviceUnitsMap={},deviceScalesMap={},devEditState={};
function toggleMqtt(cb){
  const id=cb.dataset.id;
  fetch('/mqtt-toggle?id='+id+'&on='+(cb.checked?'1':'0')).then(r=>r.json()).then(res=>{
    if(res.ok){
      if(cb.checked){mqttSelected[id]=true;const k=Object.keys(otherDevices).find(k=>otherDevices[k].id===id);if(k){const dev=otherDevices[k];selectedDevices[id]={model:dev.model,id,consumption:'—',rssi:'—',mic:'?',lastSeen:dev.lastSeen,count:dev.count};delete otherDevices[k];}}
      else{delete mqttSelected[id];if(selectedDevices[id]){const dev=selectedDevices[id];otherDevices[dev.model+'|'+id]={model:dev.model,id,lastSeen:dev.lastSeen,count:dev.count};delete selectedDevices[id];}}
      renderTables();
    }else cb.checked=!cb.checked;
  }).catch(()=>{cb.checked=!cb.checked;});
}
fetch('/mqtt-selected').then(r=>r.json()).then(d=>{
  if(d.ids)d.ids.forEach((id,i)=>{
    const sid=String(id);mqttSelected[sid]=true;
    if(d.names&&d.names[i]!==undefined)deviceNamesMap[sid]=d.names[i];
    if(d.units&&d.units[i]!==undefined)deviceUnitsMap[sid]=d.units[i];
    if(d.scales&&d.scales[i]!==undefined)deviceScalesMap[sid]=String(d.scales[i]);
    const ageSec=(d.ages&&d.ages[i]!==undefined)?d.ages[i]:-1;
    const lastSeen=ageSec>=0?Date.now()-ageSec*1000:0;
    const initCount=(d.counts&&d.counts[i]!==undefined)?d.counts[i]:0;
    if(!selectedDevices[sid])selectedDevices[sid]={model:'ERT-SCM',id:sid,consumption:'—',rssi:'—',mic:'?',lastSeen,count:initCount};
    else selectedDevices[sid].lastSeen=lastSeen;
  });
  if(d.espname!==undefined)document.getElementById('espNameIn').value=d.espname;
  if(d.seen)d.seen.forEach((id,i)=>{const sid=String(id);const mdl=(d.seenModels&&d.seenModels[i])?d.seenModels[i]:'?';const k=mdl+'|'+sid;if(!mqttSelected[sid]&&!otherDevices[k]){const age=(d.seenAges&&d.seenAges[i]!==undefined)?d.seenAges[i]:-1;const cnt=(d.seenCounts&&d.seenCounts[i]!==undefined)?d.seenCounts[i]:0;const lastSeen=age>=0?Date.now()-age*1000:0;const cons=(d.seenConsumption&&d.seenConsumption[i]!==undefined&&d.seenConsumption[i]!==null)?d.seenConsumption[i]:'—';const rssi=(d.seenRssi&&d.seenRssi[i]!==undefined&&d.seenRssi[i]!==null)?d.seenRssi[i]:'—';otherDevices[k]={model:mdl,id:sid,lastSeen,count:cnt,consumption:cons,rssi};}});
  renderTables();
  if(d.last)d.last.forEach(item=>{if(item)addPacket(JSON.stringify(item),true);});
  return fetch('/decoded');
}).then(r=>r.json()).then(d=>{
  if(d.packets&&d.packets.length){d.packets.slice().reverse().forEach(p=>addPacket(p.json,true));document.getElementById('sGood').textContent=d.good;}
}).catch(()=>{});

function updateMqttBadge(){fetch('/mqtt-status').then(r=>r.json()).then(d=>{document.getElementById('mqttDot').className='ssedot'+(d.connected?' on':'');}).catch(()=>{document.getElementById('mqttDot').className='ssedot';});}
updateMqttBadge();setInterval(updateMqttBadge,8000);setInterval(renderTables,30000);

const sse=new EventSource('/events');
const dot=document.getElementById('sseDot');
sse.onopen=()=>{dot.className='ssedot on';};
sse.onerror=()=>{dot.className='ssedot';};
sse.addEventListener('decode',e=>{addPacket(e.data);});

function timeAgo(ms){if(!ms)return 'prev session';const s=Math.floor((Date.now()-ms)/1000);if(s<60)return s+'s ago';if(s<3600)return Math.floor(s/60)+' min ago';if(s<86400)return Math.floor(s/3600)+' hr ago';return Math.floor(s/86400)+'d ago';}
function fmtCount(n){if(n>=1000000)return(n/1000000).toFixed(1).replace(/\.0$/,'')+'M';if(n>=10000)return Math.round(n/1000)+'k';if(n>=1000)return(n/1000).toFixed(1).replace(/\.0$/,'')+'k';return n;}

function addPacket(raw,isReplay){
  if(typeof raw!=='string'){try{raw=JSON.stringify(raw);}catch(_){raw=String(raw);}}
  let d={};try{d=JSON.parse(raw);}catch(_){}
  const log=document.getElementById('log');
  log.textContent=raw+'\n'+log.textContent;
  if(log.textContent.length>4000)log.textContent=log.textContent.slice(0,4000);
  const devId=(d.id!==undefined&&d.id!==null)?String(d.id):'';
  const cons=d.consumption||d.Consumption||d.consumption_data||'—';
  const now=isReplay?0:Date.now();
  if(devId&&mqttSelected[devId]){
    if(!selectedDevices[devId])selectedDevices[devId]={model:d.model||'—',id:devId,consumption:'—',rssi:'—',mic:'?',lastSeen:now,count:0};
    const dev=selectedDevices[devId];if(now)dev.lastSeen=now;dev.count++;
    if(cons!=='—')dev.consumption=cons;if(d.rssi)dev.rssi=d.rssi;if(d.mic)dev.mic=d.mic;
  }else{
    const k=(d.model||'?')+'|'+devId;
    if(!otherDevices[k])otherDevices[k]={model:d.model||'—',id:devId,lastSeen:now,count:0,consumption:'—',rssi:'—'};
    if(now)otherDevices[k].lastSeen=now;otherDevices[k].count++;
    if(cons!=='—')otherDevices[k].consumption=cons;
    if(d.rssi!==undefined)otherDevices[k].rssi=d.rssi;
  }
  renderTables();
  if(d.rssi)document.getElementById('rssiLive').textContent='RSSI '+d.rssi+' dBm';
}

function renderTables(){
  Object.keys(devEditState).forEach(id=>{if(devEditState[id]){const ne=document.getElementById('dname_'+id);const ue=document.getElementById('dunit_'+id);const se=document.getElementById('dscale_'+id);if(ne)deviceNamesMap[id]=ne.value;if(ue)deviceUnitsMap[id]=ue.value;if(se)deviceScalesMap[id]=se.value;}});
  const selBody=document.getElementById('selBody');selBody.innerHTML='';
  const selKeys=Object.keys(selectedDevices);
  if(!selKeys.length){selBody.innerHTML='<tr><td colspan="11" style="color:#005f7a;text-align:center">No devices selected — check &#9744; a device below to enable MQTT</td></tr>';}
  else{selKeys.forEach(id=>{
    const dev=selectedDevices[id];
    const curName=deviceNamesMap[id]||'';const curUnit=deviceUnitsMap[id]||'';const curScale=deviceScalesMap[id]!==undefined?deviceScalesMap[id]:'1';
    const editing=devEditState[id]||false;
    const inC=editing?'#ffaa00':'#888';const inB='background:#000;border:1px solid #0a3040;padding:3px 6px;font-size:11px;border-radius:3px;color:'+inC;
    const rdonly=editing?'':'readonly';const btnTxt=editing?'&#10004; Apply':'&#9998; Edit';const btnClr=editing?'border-color:#00ff88;color:#00ff88':'border-color:#00bcd4;color:#00e5ff';
    const tr=document.createElement('tr');
    tr.innerHTML='<td style="text-align:center"><input type="checkbox" class="mqttcb" data-id="'+id+'" onchange="toggleMqtt(this)" checked></td>'+
      '<td><input id="dname_'+id+'" type="text" '+rdonly+' style="width:130px;'+inB+'" placeholder="Device ID '+id+'" value="'+curName+'"></td>'+
      '<td><input id="dunit_'+id+'" type="text" '+rdonly+' style="width:60px;'+inB+'" placeholder="CCF" value="'+curUnit+'"></td>'+
      '<td><input id="dscale_'+id+'" type="text" '+rdonly+' style="width:70px;'+inB+'" placeholder="1" value="'+curScale+'"></td>'+
      '<td style="white-space:nowrap"><button id="dedit_'+id+'" class="btn" style="'+btnClr+';padding:3px 8px;font-size:10px" onclick="devEdit(\''+id+'\')">'+btnTxt+'</button>'+
      ' <button id="ppub_'+id+'" class="btn" style="border-color:#ffaa00;color:#ffaa00;padding:3px 8px;font-size:10px" onclick="pubDevice(\''+id+'\')">&#8679; Pub</button></td>'+
      '<td>'+timeAgo(dev.lastSeen)+'</td><td>'+dev.model+'</td>'+
      '<td style="color:#00ff88;font-weight:bold">'+dev.id+'</td>'+
      '<td style="color:#00ff88">'+dev.consumption+'</td>'+
      '<td>'+dev.rssi+'</td>'+
      '<td style="color:#00ff88;font-weight:bold">'+fmtCount(dev.count)+'</td>';
    selBody.appendChild(tr);
  });}
  const othBody=document.getElementById('othBody');othBody.innerHTML='';
  const othKeys=Object.keys(otherDevices);
  if(!othKeys.length){othBody.innerHTML='<tr><td colspan="7" style="color:#005f7a;text-align:center">Waiting for decode events…</td></tr>';}
  else{othKeys.sort((a,b)=>(otherDevices[b].lastSeen||0)-(otherDevices[a].lastSeen||0));othKeys.forEach(k=>{
    const dev=otherDevices[k];
    const cb=dev.id?'<input type="checkbox" class="selcb" data-id="'+dev.id+'">':'';
    const cons=(dev.consumption!==undefined&&dev.consumption!==null)?dev.consumption:'—';
    const rssi=(dev.rssi!==undefined&&dev.rssi!==null&&dev.rssi!=='—')?(dev.rssi+' dBm'):'—';
    const tr=document.createElement('tr');
    tr.innerHTML='<td style="text-align:center">'+cb+'</td>'+
      '<td>'+timeAgo(dev.lastSeen)+'</td><td>'+dev.model+'</td>'+
      '<td>'+(dev.id||'—')+'</td>'+
      '<td style="color:#00ff88">'+cons+'</td>'+
      '<td>'+rssi+'</td>'+
      '<td style="color:#555">'+fmtCount(dev.count)+'</td>';
    othBody.appendChild(tr);
  });}
}

function othAdd(){const cbs=document.querySelectorAll('#othBody .selcb:checked');if(!cbs.length)return;const promises=[];cbs.forEach(cb=>{const id=cb.dataset.id;promises.push(fetch('/mqtt-toggle?id='+id+'&on=1').then(r=>r.json()).then(res=>{if(res.ok){mqttSelected[id]=true;const k=Object.keys(otherDevices).find(k=>otherDevices[k].id===id);if(k){const dev=otherDevices[k];selectedDevices[id]={model:dev.model,id,consumption:'—',rssi:'—',mic:'?',lastSeen:dev.lastSeen,count:dev.count};delete otherDevices[k];}}}));});Promise.all(promises).then(()=>{renderTables();fetch('/mqtt-selected').then(r=>r.json()).then(d=>{if(d.last)d.last.forEach(item=>{if(item)addPacket(JSON.stringify(item),true);});renderTables();}).catch(()=>{});});}
function othDelete(){const cbs=document.querySelectorAll('#othBody .selcb:checked');if(!cbs.length)return;const promises=[];cbs.forEach(cb=>{const id=cb.dataset.id;promises.push(fetch('/seen-delete?id='+id).then(r=>r.json()).then(res=>{if(res.ok){const k=Object.keys(otherDevices).find(k=>otherDevices[k].id===id);if(k)delete otherDevices[k];}}));});Promise.all(promises).then(()=>renderTables());}
function applyOokReg(addr,selId){const v=document.getElementById(selId).value;fetch('/ctrl?reg='+addr+'&val='+v).then(r=>r.json()).then(()=>loadOokRegs());}
function applyErtOpt(){const btn=document.getElementById('ertOptBtn');btn.innerHTML='&#9203; Applying...';fetch('/ert-optimize').then(r=>r.json()).then(d=>{btn.innerHTML=d.ok?'&#10004; Applied':'&#10006; '+(d.err||'failed');setTimeout(()=>{btn.innerHTML='&#9889; Apply ERT-Optimized';loadOokRegs();refreshHealth();},2000);}).catch(()=>{btn.innerHTML='&#9889; Apply ERT-Optimized';});}
function loadOokRegs(){fetch('/registers').then(r=>r.json()).then(d=>{const fmt=v=>'0x'+v.toString(16).toUpperCase().padStart(2,'0');const setRow=(key,livId,selId)=>{if(!d[key])return;const v=d[key].val;document.getElementById(livId).textContent=fmt(v);const s=document.getElementById(selId);for(const o of s.options)if(parseInt(o.value,16)===v){s.value=o.value;break;}};setRow('RegRssiThresh','livRssiThr','selRssiThr');setRow('RegOokPeak','livOokPeak','selOokPeak');setRow('RegOokFix','livOokFix','selOokFix');setRow('RegOokAvg','livOokAvg','selOokAvg');});}
function resetDefaults(){
  const btn=document.getElementById('resetBtn');
  btn.textContent='…';
  fetch('/reset-defaults').then(r=>r.json()).then(d=>{
    document.getElementById('freqIn').value='914.224';
    document.getElementById('sFreq').textContent='914.224 MHz';
    document.getElementById('gateSlider').value='-95';
    document.getElementById('gateDisp').textContent='-95 dBm';
    document.getElementById('sGate').textContent='-95 dBm';
    const bwSel=document.getElementById('bwSel');
    bwSel.value=String(d.bw);
    const bwText=bwSel.options[d.bw]?bwSel.options[d.bw].text:'—';
    document.getElementById('sBw').textContent=bwText;
    if(ookPanelVisible)loadOokRegs();
    btn.textContent='✓ Done';
    setTimeout(()=>{btn.innerHTML='&#x21BA; Reset Defaults';},2000);
  }).catch(()=>{btn.innerHTML='&#x21BA; Reset Defaults';});
}
let ookPanelVisible=false;
function toggleOokPanel(){
  const panel=document.getElementById('ookPanel');
  const btn=document.getElementById('ookToggleBtn');
  ookPanelVisible=!ookPanelVisible;
  panel.style.display=ookPanelVisible?'':'none';
  btn.innerHTML=ookPanelVisible?'&#9660; Hide OOK &amp; RSSI Registers':'&#9654; Show OOK &amp; RSSI Registers';
  if(ookPanelVisible) loadOokRegs();
}
document.getElementById('gateSlider').addEventListener('input',function(){document.getElementById('gateDisp').textContent=this.value+' dBm';});
function applyFreq(){const f=parseFloat(document.getElementById('freqIn').value);if(f<300||f>928)return;fetch('/ctrl?f='+f.toFixed(3)).then(r=>r.json()).then(()=>{document.getElementById('sFreq').textContent=f.toFixed(3)+' MHz';});}
function applyGate(){const g=parseInt(document.getElementById('gateSlider').value);fetch('/ctrl?gate='+g).then(r=>r.json()).then(()=>{document.getElementById('sGate').textContent=g+' dBm';});}
function applyBw(){const sel=document.getElementById('bwSel');const idx=parseInt(sel.value);fetch('/ctrl?bw='+idx).then(r=>r.json()).then(()=>{const sb=document.getElementById('sBw');if(sb)sb.textContent=sel.options[idx].text;});}
let espNameEditing=false;
function espNameToggle(){
  const inp=document.getElementById('espNameIn');const btn=document.getElementById('espNameBtn');
  if(!espNameEditing){espNameEditing=true;inp.removeAttribute('readonly');inp.style.color='#ffaa00';btn.innerHTML='&#10004; Apply';btn.style.borderColor='#00ff88';btn.style.color='#00ff88';inp.focus();}
  else{const n=inp.value.trim();fetch('/ctrl?espname='+encodeURIComponent(n)).then(r=>r.json()).then(()=>{espNameEditing=false;inp.setAttribute('readonly','');inp.style.color='#888';btn.innerHTML='&#9998; Edit';btn.style.borderColor='#00bcd4';btn.style.color='#00e5ff';document.getElementById('espNameNote').textContent='saved';setTimeout(()=>document.getElementById('espNameNote').textContent='',3000);}).catch(()=>{document.getElementById('espNameNote').textContent='error';});}
}
function pubDevice(id){const btn=document.getElementById('ppub_'+id);if(!btn)return;fetch('/mqtt-publish?id='+id).then(r=>r.json()).then(d=>{btn.innerHTML=d.ok?'&#10003; Sent':'&#10007; No data';setTimeout(()=>{btn.innerHTML='&#8679; Pub';},2000);}).catch(()=>{btn.innerHTML='&#10007; Err';setTimeout(()=>{btn.innerHTML='&#8679; Pub';},2000);});}
function devEdit(id){
  const nameEl=document.getElementById('dname_'+id);const unitEl=document.getElementById('dunit_'+id);const btnEl=document.getElementById('dedit_'+id);const scaleEl=document.getElementById('dscale_'+id);if(!nameEl)return;
  if(!devEditState[id]){devEditState[id]=true;nameEl.removeAttribute('readonly');nameEl.style.color='#ffaa00';unitEl.removeAttribute('readonly');unitEl.style.color='#ffaa00';scaleEl.removeAttribute('readonly');scaleEl.style.color='#ffaa00';btnEl.innerHTML='&#10004; Apply';btnEl.style.borderColor='#00ff88';btnEl.style.color='#00ff88';nameEl.focus();}
  else{devEditState[id]=false;const nm=nameEl.value.trim();const un=unitEl.value.trim();let sc=parseFloat(scaleEl.value);if(isNaN(sc)||sc<=0)sc=1;deviceNamesMap[id]=nm;deviceUnitsMap[id]=un;deviceScalesMap[id]=String(sc);scaleEl.value=String(sc);nameEl.setAttribute('readonly','');nameEl.style.color='#888';unitEl.setAttribute('readonly','');unitEl.style.color='#888';scaleEl.setAttribute('readonly','');scaleEl.style.color='#888';btnEl.innerHTML='&#9998; Edit';btnEl.style.borderColor='#00bcd4';btnEl.style.color='#00e5ff';fetch('/ctrl?devname='+id+'&name='+encodeURIComponent(nm)).catch(()=>{});fetch('/ctrl?devunit='+id+'&unit='+encodeURIComponent(un)).catch(()=>{});fetch('/ctrl?devscale='+id+'&scale='+sc).catch(()=>{});}
}
fetch('/status').then(r=>r.json()).then(d=>{
  document.getElementById('freqIn').value=d.freq.toFixed(3);
  document.getElementById('gateSlider').value=d.gate;
  document.getElementById('gateDisp').textContent=d.gate+' dBm';
  document.getElementById('sFreq').textContent=d.freq.toFixed(3)+' MHz';
  document.getElementById('sGate').textContent=d.gate+' dBm';
  document.getElementById('sGood').textContent=d.good;
  document.getElementById('sRssi').textContent=(d.rssi||'—')+' dBm';
  if(d.avgRssi!==undefined)document.getElementById('sAvgRssi').textContent=d.avgRssi+' dBm';
}).catch(()=>{});
function updateTopBarHealth(d){
  const b=document.getElementById('healthBadge');if(!b)return;
  const dot=c=>'<span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:'+c+';margin-left:4px;vertical-align:middle"></span>';
  const errs=[];if(!d.radio.ok)errs.push('Radio: '+d.radio.msg);if(!d.wifi.ok)errs.push('WiFi down');if(!d.mqtt.ok)errs.push('MQTT disconnected');
  if(!errs.length){b.style.borderColor='#00ff88';b.style.color='#00ff88';b.innerHTML='SYS'+dot('#00ff88');}
  else{b.style.borderColor='#ff4444';b.style.color='#ff4444';b.innerHTML=dot('#ff4444')+' '+errs.join(' | ');}
}
function refreshHealth(){
  fetch('/health').then(r=>r.json()).then(d=>{
    const nb=document.getElementById('nvsBadge');const ub=document.getElementById('uptimeBadge');
    if(nb)nb.textContent=d.nvs.devices+' device'+(d.nvs.devices!==1?'s':'');
    if(ub){const m=Math.floor(d.uptime/60),s=d.uptime%60;ub.textContent='Up '+m+'m'+(s?' '+s+'s':'');}
    const fb=document.getElementById('fwBadge');if(fb&&d.fw)fb.textContent=d.fw;
    const cb=document.getElementById('chipBadge');
    if(cb&&d.radio&&d.radio.ver){
      const ok=(d.radio.ver===d.radio.expected);
      cb.textContent='CHIP '+d.radio.ver+(ok?' OK':' FAULT');
      cb.style.color=ok?'#00ff88':'#ff4444';cb.style.borderColor=ok?'#00ff88':'#ff4444';
    }
    const ctr=document.getElementById('ctrBadge');
    if(ctr&&d.counters){
      const c=d.counters;
      ctr.textContent='RAW:'+c.raw+' / OK:'+c.good+(c.rejModel||c.rejRssi||c.rejPlaus?(' (drop M:'+c.rejModel+' R:'+c.rejRssi+' P:'+c.rejPlaus+')'):'');
    }
    if(d.bw!==undefined){const sel=document.getElementById('bwSel');if(sel){sel.value=String(d.bw);const bwText=sel.options[d.bw]?sel.options[d.bw].text:'—';const sb=document.getElementById('sBw');if(sb)sb.textContent=bwText;}}
    const errDot='<span style="display:inline-block;width:9px;height:9px;border-radius:50%;background:#ff4444;margin-right:8px;vertical-align:middle"></span>';
    const errs=[];
    if(!d.radio.ok)errs.push(errDot+'<b>Radio SX1276:</b> <span style="color:#ff4444">'+d.radio.msg+'</span>');
    if(!d.wifi.ok)errs.push(errDot+'<b>WiFi:</b> <span style="color:#ff4444">Not connected</span>');
    if(!d.mqtt.ok)errs.push(errDot+'<b>MQTT:</b> <span style="color:#ff4444">Disconnected ('+d.mqtt.broker+')</span>');
    const panel=document.getElementById('errorsPanel');
    if(panel){panel.style.display=errs.length?'':'none';if(errs.length)document.getElementById('errorsContent').innerHTML=errs.join('<br>');}
    updateTopBarHealth(d);
  }).catch(()=>{
    const panel=document.getElementById('errorsPanel');
    if(panel){panel.style.display='';document.getElementById('errorsContent').innerHTML='<span style="color:#ff4444">&#x26A0; Health check failed — server may be starting up</span>';}
  });
}
refreshHealth();setInterval(refreshHealth,10000);

// ── Pulse Monitor ──
let pmOn=false, pmTimer=null;
function pmToggle(){
  const url=pmOn?'/pulse-stop':'/pulse-start';
  fetch(url).then(r=>r.json()).then(d=>{
    pmOn=d.on;
    const b=document.getElementById('pmBtn');
    b.textContent=pmOn?'Stop Monitor':'Start Monitor';
    b.style.borderColor=pmOn?'#ff4444':'#ffaa00';b.style.color=pmOn?'#ff4444':'#ffaa00';
    if(pmOn&&!pmTimer){pmTimer=setInterval(pmDraw,200);}
    if(!pmOn&&pmTimer){clearInterval(pmTimer);pmTimer=null;document.getElementById('pmInfo').textContent='— disabled —';}
  });
}
function pmDraw(){
  fetch('/pulses').then(r=>r.json()).then(d=>{
    const c=document.getElementById('pmCanvas'),ctx=c.getContext('2d');
    const W=c.width,H=c.height,N=d.samples.length;
    ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
    // 3 horizontal lanes for DIO0/DIO2/DIO1
    const laneH=H/3, mid=12;
    const lanes=[{name:'DIO0',bit:0x01,col:'#0f0',y:laneH*0.5},{name:'DIO2 (OOK)',bit:0x02,col:'#0ff',y:laneH*1.5},{name:'DIO1',bit:0x04,col:'#fa0',y:laneH*2.5}];
    // Re-order buffer so head is the leftmost recent sample
    const head=d.head, samples=d.samples;
    lanes.forEach(l=>{
      ctx.strokeStyle=l.col;ctx.lineWidth=1;
      ctx.beginPath();
      let prev=-1;
      for(let i=0;i<N;i++){
        const idx=(head+i)%N;
        const bit=(samples[idx]&l.bit)?1:0;
        const x=(i/N)*W;
        const y=l.y+(bit?-mid:mid);
        if(prev===-1){ctx.moveTo(x,y);} else {ctx.lineTo(x,y);}
        prev=bit;
      }
      ctx.stroke();
      ctx.fillStyle=l.col;ctx.font='11px monospace';ctx.fillText(l.name,4,l.y-mid-2);
    });
    // Count edges as a quick stat per lane
    const counts=[0,0,0];
    for(let i=1;i<N;i++){
      const a=samples[(head+i-1)%N], b=samples[(head+i)%N];
      if((a&0x01)!==(b&0x01))counts[0]++;
      if((a&0x02)!==(b&0x02))counts[1]++;
      if((a&0x04)!==(b&0x04))counts[2]++;
    }
    document.getElementById('pmInfo').textContent='LIVE @ 20 kHz · 100 ms window · edges in window — DIO0:'+counts[0]+'  DIO2:'+counts[1]+'  DIO1:'+counts[2];
  }).catch(()=>{});
}
</script>
<style>
div.panel:has(#pmCanvas),#ookToggleBtn,#resetBtn,#ertOptBtn{display:none!important}</style>
</body></html>
)HTML";

static const uint8_t PAGE_GZ[] PROGMEM = {31,139,8,0,0,0,0,0,2,255,237,125,217,114,227,72,146,224,187,190,34,146,185,157,32,74,20,5,80,71,74,164,64,173,174,156,204,169,202,99,82,89,149,211,86,157,86,6,18,160,136,18,8,176,1,80,71,41,53,214,79,107,179,175,51,99,54,47,243,188,127,176,54,239,61,127,82,95,178,238,30,7,2,32,120,150,186,183,215,108,203,202,82,64,192,195,195,195,195,175,136,240,8,110,28,61,59,127,127,246,233,247,31,46,216,48,27,133,221,35,252,151,133,110,116,229,212,252,168,6,239,190,235,117,55,142,70,126,230,178,254,208,77,82,63,115,106,223,127,122,181,117,80,147,197,145,59,242,157,218,77,224,223,142,227,36,171,177,126,28,101,126,4,96,183,129,151,13,29,207,191,9,250,254,22,189,52,130,40,200,2,55,220,74,251,110,232,59,54,226,200,130,44,244,187,23,31,63,177,143,126,223,15,110,252,132,189,246,195,204,239,179,31,90,71,219,252,235,198,81,154,221,227,223,111,30,122,241,221,86,26,252,18,68,87,237,94,156,120,126,178,5,37,157,145,155,92,5,81,219,234,140,93,207,195,111,214,227,70,47,246,238,31,6,64,204,214,192,29,5,225,125,219,56,139,39,73,0,248,223,249,183,70,99,20,71,113,58,118,251,126,167,231,246,175,175,146,120,18,121,237,231,214,190,229,217,187,157,126,28,198,9,188,89,254,222,96,208,25,5,209,214,208,15,174,134,89,219,182,172,155,225,227,70,179,231,38,15,94,144,142,67,247,190,61,8,253,187,142,27,6,87,209,86,144,249,163,180,221,135,238,251,73,231,202,29,183,237,214,248,78,17,101,91,227,59,102,31,64,73,161,73,215,118,91,187,29,213,155,44,139,71,109,168,198,210,56,12,60,6,68,244,250,222,110,7,91,217,186,77,0,39,254,195,73,96,67,155,247,16,56,226,183,237,29,192,44,40,31,0,217,153,127,151,109,101,137,27,165,131,56,25,181,39,227,177,159,244,221,212,239,132,126,6,244,109,97,239,145,44,104,139,208,121,87,254,131,36,117,7,41,181,144,82,78,86,226,122,193,36,109,107,69,109,123,154,196,156,20,59,39,69,48,177,208,229,93,251,160,181,11,141,142,220,32,82,109,218,187,146,61,5,206,82,199,189,32,241,251,89,16,71,109,192,58,25,69,138,185,128,100,236,70,126,248,80,192,255,210,110,217,110,21,161,238,142,181,107,149,58,181,175,15,145,64,153,233,124,181,214,230,171,16,76,57,170,58,71,101,209,20,113,130,20,9,176,71,4,37,241,237,114,242,102,77,53,138,12,157,18,158,176,23,206,232,161,101,237,13,94,186,179,59,137,202,64,202,12,188,178,144,182,32,26,79,178,31,179,251,177,239,68,147,81,207,79,190,52,180,34,192,112,229,23,74,16,243,151,226,112,89,86,89,88,102,141,156,28,39,20,149,151,216,49,77,191,131,104,232,39,65,166,139,97,107,74,130,81,71,226,73,22,6,145,223,142,226,200,175,162,255,65,244,15,217,242,56,213,23,249,117,191,244,149,247,139,127,60,204,57,211,30,196,253,73,250,32,168,80,221,68,133,65,173,203,114,249,223,71,241,223,45,91,7,82,149,217,58,87,228,219,2,118,216,146,99,183,220,154,245,226,208,235,244,39,73,10,40,198,113,64,82,52,197,46,78,101,123,24,131,105,126,24,4,96,154,147,118,47,193,250,145,159,166,117,187,185,107,114,144,45,119,60,14,239,167,122,58,24,28,28,116,10,111,2,60,241,7,137,159,14,75,21,6,3,215,205,229,129,191,65,133,52,115,179,162,6,160,184,163,4,150,132,91,74,127,22,143,81,74,176,106,240,176,164,65,225,202,159,234,170,113,184,188,102,148,212,223,230,200,110,202,22,186,60,0,143,27,153,219,11,125,41,86,150,245,187,78,206,145,208,29,167,126,91,62,148,198,18,106,14,31,42,92,137,46,80,168,253,68,49,25,140,118,232,15,50,221,150,45,48,70,208,130,167,228,115,79,160,155,93,103,223,182,236,131,162,72,2,134,132,203,14,3,84,211,196,2,139,226,235,135,162,120,52,163,248,65,209,184,11,255,21,148,180,63,244,251,215,224,244,191,52,71,127,204,178,126,175,81,249,45,245,195,126,239,193,237,163,109,44,201,162,96,52,116,167,35,189,58,62,151,244,0,40,206,2,8,83,4,227,70,129,231,133,62,14,104,234,123,113,38,70,235,32,199,113,48,101,106,246,112,36,181,14,239,237,237,41,207,22,68,104,129,182,122,97,220,191,150,18,75,58,133,131,166,90,105,198,81,201,80,114,245,121,30,198,87,83,22,84,14,211,52,33,187,213,78,155,143,214,200,189,83,193,13,154,180,14,142,213,32,140,111,183,238,219,238,36,139,59,179,220,4,153,31,234,133,172,222,220,127,220,56,218,230,161,218,209,54,133,142,71,24,133,65,252,230,5,55,172,31,186,105,234,212,32,110,129,184,143,177,163,161,221,125,241,252,174,117,218,106,117,242,144,143,93,254,163,221,122,185,207,14,237,189,183,175,127,97,175,79,216,185,223,143,129,116,64,104,83,53,208,175,40,199,5,65,75,141,5,158,83,27,220,158,210,75,247,215,63,253,27,16,1,64,243,160,129,191,2,188,0,193,217,174,64,206,225,185,43,144,93,94,94,44,70,139,18,185,8,47,194,232,136,223,254,195,167,79,139,49,39,105,26,124,7,209,113,173,251,241,242,242,13,91,170,147,253,97,48,230,212,48,138,163,157,154,96,238,71,255,234,7,63,73,193,250,49,210,24,86,31,77,210,140,245,124,102,221,217,45,179,214,61,123,253,230,195,146,109,100,73,177,137,239,130,94,226,38,247,12,84,39,68,17,77,217,175,255,227,95,24,247,27,252,25,149,114,156,249,30,116,229,228,115,219,98,219,236,253,183,109,107,113,83,209,77,42,120,251,238,135,203,229,168,155,140,179,96,36,71,250,251,241,114,149,64,112,195,108,40,42,93,254,254,82,0,146,92,59,181,74,13,94,211,26,84,218,24,37,24,146,210,163,109,208,158,162,14,97,220,12,74,132,157,208,74,41,16,230,125,240,147,36,78,210,15,188,160,68,57,134,61,157,178,211,69,51,75,106,89,66,153,169,234,37,80,84,221,253,19,171,195,46,239,83,136,66,217,5,181,40,104,85,104,114,90,206,248,180,80,161,43,69,105,186,33,105,53,119,145,9,2,147,120,168,236,107,37,193,221,75,112,196,96,9,167,136,17,32,16,75,139,138,37,25,128,160,184,214,189,184,252,176,211,98,239,96,82,155,75,10,65,146,155,97,228,102,106,232,81,5,163,211,49,194,190,137,106,236,198,13,39,240,173,198,192,164,134,126,116,5,147,223,218,142,93,99,192,244,190,63,4,79,239,39,78,77,25,58,197,8,45,156,148,198,245,224,224,160,198,18,176,159,113,20,222,43,2,122,19,112,184,185,192,102,81,129,130,83,124,23,40,171,130,205,162,91,174,177,56,234,135,65,255,90,213,255,20,95,93,133,126,221,196,145,61,60,60,60,232,176,11,47,200,142,182,121,171,69,126,105,205,190,139,51,191,106,76,167,103,20,194,203,97,4,130,234,161,196,156,143,207,58,67,245,42,241,255,56,241,163,254,61,171,131,175,48,231,140,23,15,236,133,163,128,90,218,112,29,218,187,205,86,107,23,251,224,143,157,154,213,180,44,24,51,152,229,192,224,89,22,13,38,0,181,14,106,179,199,129,169,224,87,99,43,189,35,133,200,210,19,124,41,242,114,173,30,127,252,71,118,234,70,30,137,76,169,187,16,240,64,64,75,61,236,221,94,106,106,255,183,50,211,82,221,2,98,227,49,134,222,114,8,172,90,183,181,103,177,107,240,247,172,158,248,87,224,133,44,187,193,160,151,62,120,37,176,217,224,153,224,1,194,109,134,235,68,56,128,91,195,120,12,3,206,241,204,68,108,3,98,171,132,248,112,113,181,86,173,107,239,191,44,84,179,237,197,213,118,160,90,107,175,216,90,107,113,53,176,117,118,153,200,147,197,213,246,106,221,131,29,94,75,17,185,68,107,251,181,238,126,171,201,169,148,173,237,52,88,228,130,145,190,5,46,79,97,0,49,35,201,90,71,252,79,111,159,84,248,49,240,249,59,55,243,89,221,59,29,205,83,119,154,168,115,109,191,2,248,75,144,107,212,126,82,234,45,187,37,181,122,203,182,164,218,219,202,30,108,29,238,213,166,173,29,162,57,7,255,57,203,125,229,171,34,104,199,107,93,192,194,128,200,18,141,43,176,14,251,185,58,243,36,117,218,28,120,31,201,153,79,128,152,136,115,134,197,241,53,119,5,228,80,20,81,25,149,189,143,175,41,166,16,110,98,127,111,23,252,255,48,190,101,239,223,127,203,94,184,163,113,135,209,40,65,128,25,164,24,240,77,249,143,25,78,12,218,247,179,217,46,140,7,29,157,226,91,217,161,228,196,18,182,115,127,224,78,194,44,229,180,222,181,236,211,19,160,14,191,48,249,105,49,117,250,0,241,88,38,123,63,46,16,90,162,162,179,120,249,163,60,212,23,132,83,176,244,224,224,176,195,104,208,209,214,109,189,199,0,22,164,204,155,47,2,115,253,47,68,19,165,69,17,77,32,186,229,86,186,12,56,148,182,217,233,103,71,26,229,186,176,196,175,46,191,53,217,159,255,147,157,6,217,71,16,79,103,127,175,185,187,207,174,79,221,137,199,234,173,255,250,119,178,206,56,233,96,9,124,70,80,209,12,136,195,71,152,191,124,26,162,152,57,214,221,171,87,172,30,197,236,74,64,225,119,148,44,223,189,134,143,214,1,171,227,227,54,52,115,121,31,245,223,15,6,219,86,19,213,73,131,125,21,220,33,232,62,171,239,195,7,6,243,213,56,49,155,243,212,3,23,144,196,196,14,158,78,197,36,116,10,42,128,184,68,127,7,187,243,157,11,125,191,132,0,221,13,73,186,121,3,5,168,27,129,25,123,41,102,160,4,147,147,178,184,153,83,229,167,23,181,114,114,115,245,91,26,194,160,100,14,118,252,188,46,106,180,90,115,80,227,231,117,81,99,232,243,121,14,238,211,219,181,137,142,99,79,172,50,164,243,136,7,176,234,38,150,154,170,72,227,58,123,78,54,107,54,195,231,236,179,45,108,78,7,45,36,150,230,21,184,158,88,29,167,77,45,47,230,26,145,241,197,155,44,41,77,254,120,40,175,71,114,217,80,130,148,87,25,181,0,18,70,79,144,123,180,157,13,43,107,23,160,79,60,111,73,72,92,18,153,9,57,135,34,205,97,239,236,146,199,254,12,1,109,1,21,60,39,93,44,160,45,80,137,156,175,103,229,141,37,249,11,190,122,149,84,22,236,223,81,47,233,22,86,20,230,44,54,215,186,175,63,243,241,70,91,73,161,112,202,13,17,46,216,32,38,255,174,239,251,30,203,134,65,202,173,32,203,98,230,145,48,171,101,132,204,91,76,164,190,72,204,247,147,106,93,136,40,173,181,107,147,188,135,193,141,232,184,208,156,101,144,213,116,144,226,204,6,30,37,190,249,98,110,79,161,153,158,117,128,27,194,62,130,51,66,190,254,250,207,255,2,122,70,126,102,164,188,83,71,46,71,146,87,243,120,216,192,126,253,143,127,173,8,180,103,53,98,81,35,86,222,136,197,155,240,7,3,220,129,184,241,193,213,75,103,184,52,214,11,194,122,145,99,181,91,28,235,175,255,243,159,65,24,252,100,4,50,194,229,193,205,152,116,228,167,159,151,111,225,156,90,56,207,91,176,118,121,68,187,100,253,179,3,172,127,118,144,215,183,86,171,79,237,159,89,88,151,225,63,135,251,43,213,63,221,197,250,167,187,121,253,213,218,63,57,193,250,39,39,170,254,193,222,106,245,137,254,147,156,254,131,213,218,63,220,199,250,208,105,89,255,229,106,237,31,156,97,253,131,179,188,254,106,237,191,164,241,123,121,160,234,239,175,86,127,159,248,191,159,243,127,79,200,253,141,159,220,179,12,23,250,58,12,215,183,88,24,67,252,200,205,90,58,67,62,203,147,208,101,194,245,233,169,208,78,97,146,64,96,16,74,130,113,174,27,104,234,140,134,145,219,23,195,84,46,161,56,71,16,174,65,179,99,228,40,214,112,9,34,228,93,205,31,112,31,18,135,30,27,129,137,103,155,12,103,95,3,140,80,65,235,17,21,216,254,128,86,246,67,31,76,11,186,2,247,62,101,61,63,187,245,253,136,141,39,97,138,81,206,111,247,12,187,191,217,51,136,254,63,153,103,144,248,158,192,51,88,164,188,22,55,126,48,215,240,189,6,128,137,9,9,123,15,46,99,234,191,250,4,24,155,207,78,150,55,180,22,41,154,197,13,37,118,0,155,210,219,106,208,26,5,227,115,160,109,242,68,245,36,11,127,218,221,217,249,233,226,242,3,11,197,142,203,90,238,201,58,164,214,15,23,181,110,55,45,217,250,242,184,201,132,90,39,139,113,239,173,142,251,148,112,159,46,194,221,90,135,110,50,157,214,217,34,220,59,235,224,62,39,220,231,139,112,239,174,131,251,130,112,95,44,194,189,183,14,110,138,149,172,87,139,112,239,175,129,219,182,120,172,73,184,79,192,63,184,87,126,99,121,109,131,233,240,242,242,222,162,182,90,179,52,251,29,123,66,205,110,145,102,183,102,105,246,59,120,157,214,236,75,127,148,249,253,97,33,226,92,161,73,82,231,214,225,162,38,215,81,231,22,169,115,235,100,49,238,213,213,185,69,234,220,58,93,132,123,29,117,110,145,58,183,206,22,225,94,71,157,91,164,206,173,243,69,184,215,81,231,22,169,115,235,98,17,238,117,212,185,69,234,220,122,181,8,247,58,234,188,67,42,182,51,75,157,23,169,216,108,117,254,107,68,129,187,60,10,20,177,196,95,39,10,4,163,178,90,16,248,54,136,130,209,100,36,38,120,124,181,20,216,70,33,161,137,168,160,116,128,182,13,130,66,25,46,214,201,216,9,144,39,136,255,246,158,34,254,3,154,158,50,252,67,116,79,17,253,217,228,235,108,18,96,102,171,5,102,86,31,113,198,119,152,27,142,98,136,185,97,222,206,51,91,128,194,21,98,174,29,194,191,195,241,239,40,252,203,35,160,185,161,181,207,17,236,235,4,34,85,169,31,165,1,174,44,176,113,226,246,41,195,100,205,112,148,29,172,65,156,30,245,217,214,26,8,244,240,139,214,53,100,239,42,22,99,204,245,34,24,123,79,67,251,100,193,180,221,34,213,104,241,38,214,224,157,189,203,231,86,132,160,181,6,239,108,242,255,54,247,255,173,189,53,16,144,211,177,185,211,217,89,131,130,66,208,179,187,6,130,29,98,226,14,103,226,158,165,141,83,232,187,43,73,247,95,195,95,236,41,127,1,198,231,175,227,46,192,210,175,230,46,208,63,104,158,128,150,4,104,139,142,242,225,0,149,182,134,160,128,146,120,156,47,27,244,38,73,154,61,201,178,193,254,83,184,13,224,192,83,186,13,68,247,4,110,163,160,253,231,196,100,251,191,254,157,141,253,132,91,44,30,233,60,153,177,41,232,73,177,185,22,53,152,46,141,106,143,80,237,85,161,218,93,17,213,75,66,245,178,10,213,1,71,133,76,72,67,74,54,233,48,148,181,84,19,187,48,142,174,170,178,80,102,47,147,82,123,135,122,123,45,141,233,203,47,23,19,158,83,29,207,238,58,120,206,9,207,185,142,231,96,29,60,175,8,207,171,2,31,247,203,210,84,71,165,37,62,230,28,204,18,74,186,245,105,153,21,33,255,47,26,199,125,101,28,65,197,214,52,142,240,156,239,185,193,11,110,113,174,151,22,138,73,32,47,109,204,88,161,142,251,184,233,139,103,19,83,226,49,166,97,179,19,218,144,153,218,82,157,222,23,45,236,55,138,20,110,120,192,151,179,73,154,197,35,145,65,42,202,190,143,48,149,82,188,92,226,225,71,245,166,30,120,154,1,88,91,85,242,22,98,245,80,189,189,57,207,155,136,163,116,50,162,65,85,101,60,103,64,214,76,49,251,117,153,189,76,105,6,79,225,185,70,61,67,35,10,14,4,77,189,83,179,237,90,229,22,112,149,117,127,135,27,143,156,161,169,228,48,114,150,167,152,191,120,126,248,114,23,152,239,10,32,240,45,96,4,112,183,210,143,104,219,90,48,209,235,74,98,159,102,216,203,59,237,179,78,207,253,12,195,22,12,238,183,196,241,213,54,157,13,221,18,14,144,78,7,81,38,21,121,221,238,123,220,98,3,241,201,10,114,36,253,99,85,186,184,58,178,196,177,84,164,65,205,200,221,157,206,31,234,76,29,210,44,230,253,104,10,25,103,195,19,207,195,212,162,77,6,15,74,243,150,110,191,50,241,106,165,246,207,65,20,50,95,38,97,189,180,247,58,140,23,229,196,200,176,98,69,173,187,212,148,227,73,149,199,79,83,247,202,95,69,129,160,159,51,20,232,229,242,250,243,217,13,48,109,157,242,92,249,14,62,90,241,40,75,127,253,211,255,250,91,85,140,98,170,164,204,85,249,128,187,76,236,109,12,86,15,250,66,30,236,205,123,60,232,1,127,108,254,167,85,78,95,38,17,16,98,137,44,29,143,40,203,110,9,9,157,62,47,88,148,208,214,60,9,29,143,242,196,247,203,204,77,50,73,118,89,60,127,83,222,157,56,43,167,231,98,190,31,12,88,79,11,253,128,73,191,248,73,204,206,62,124,207,240,8,22,74,26,187,29,6,96,25,97,132,112,148,189,38,251,60,132,96,156,219,75,175,193,82,119,52,14,193,218,186,97,72,1,128,143,140,101,227,32,74,105,211,95,36,239,193,20,234,207,255,59,165,192,129,87,48,161,247,236,44,78,124,102,179,186,123,11,145,197,32,1,135,37,2,83,150,200,99,248,18,202,18,137,117,140,82,61,83,246,79,184,149,63,74,89,60,32,216,40,99,56,13,187,9,178,251,102,241,168,140,148,248,129,85,235,254,93,66,202,73,172,116,72,26,234,32,71,215,126,198,222,124,252,7,179,49,163,226,160,214,61,187,119,245,122,45,86,199,76,168,196,189,101,158,155,185,196,55,208,74,32,214,103,197,208,26,34,35,96,68,22,207,64,62,112,129,170,247,148,41,172,161,7,142,224,137,161,120,194,233,154,206,40,236,187,209,141,155,10,9,61,163,151,26,227,55,29,128,179,196,3,3,252,16,11,188,29,88,51,115,240,43,178,238,247,44,117,56,80,63,83,68,51,17,60,163,167,206,36,91,252,236,4,39,164,116,212,102,60,122,19,13,226,218,234,249,160,200,70,41,102,76,229,184,173,21,102,125,132,161,225,217,116,236,187,248,170,234,64,80,24,95,85,156,239,145,199,156,210,126,18,140,33,44,5,31,193,240,172,156,140,214,156,135,199,134,140,43,132,195,197,162,24,93,177,246,206,163,11,140,190,210,183,238,56,47,193,16,172,88,66,113,152,86,132,231,93,192,4,100,62,188,119,54,6,147,136,206,7,51,158,249,252,22,40,169,247,123,230,3,80,12,150,48,165,249,99,191,215,68,33,76,253,172,25,120,29,248,50,240,179,254,176,110,108,35,221,91,188,226,49,192,25,155,129,183,105,188,136,35,120,2,36,77,138,136,124,239,216,176,141,182,97,25,166,217,132,78,68,245,196,233,38,205,159,211,56,170,171,18,232,83,247,129,184,23,12,240,173,25,95,155,15,66,27,161,36,199,101,62,232,188,250,49,240,190,56,89,50,241,59,156,214,107,231,125,239,103,248,210,188,246,239,211,186,206,50,179,57,8,34,175,126,237,116,245,210,31,175,191,64,143,28,199,9,60,179,3,237,64,163,28,17,176,201,41,1,118,74,131,66,109,63,224,58,115,216,6,240,38,61,53,2,175,209,207,29,111,219,0,25,51,26,120,168,81,60,142,130,126,219,56,54,26,32,71,25,250,113,170,42,95,160,234,4,60,15,22,209,211,99,199,163,16,130,149,73,121,124,20,188,241,193,1,61,8,160,50,99,176,67,21,52,235,93,172,248,220,41,180,165,58,182,105,124,197,209,173,238,241,106,157,169,106,84,117,40,241,35,176,24,159,80,65,211,186,217,161,210,71,236,37,203,101,192,121,150,63,35,196,163,217,236,187,40,144,117,19,164,104,6,220,35,32,123,220,40,8,174,36,196,152,41,151,30,151,74,96,164,7,114,146,154,244,111,19,2,151,11,23,155,131,174,7,166,20,92,206,213,20,196,233,50,195,117,242,58,202,84,97,76,82,37,173,82,210,189,38,222,42,147,190,120,33,30,126,12,190,60,115,28,48,159,62,72,43,8,123,81,201,57,130,28,84,67,51,65,173,71,52,244,80,141,70,90,6,137,70,130,106,104,232,190,26,194,195,159,170,17,41,131,194,49,137,238,106,117,196,184,113,142,64,136,121,233,247,29,248,142,193,38,226,198,191,101,204,199,170,184,189,101,235,213,165,56,57,28,79,215,177,142,207,193,118,53,163,248,182,110,110,241,194,111,192,91,88,109,75,175,134,55,240,156,161,216,97,195,36,127,212,52,127,170,104,92,125,144,104,128,31,207,202,146,138,221,53,171,10,165,86,24,120,202,224,242,236,173,1,90,209,78,87,181,5,66,99,20,233,143,156,18,146,254,170,86,149,170,57,242,129,107,67,71,138,172,159,142,81,86,138,67,24,247,39,35,8,102,154,87,126,118,17,250,248,120,122,255,198,171,27,234,132,39,168,3,95,158,81,8,20,194,20,218,48,249,159,105,45,168,84,0,94,56,242,66,71,84,167,249,10,23,49,245,134,34,115,92,42,64,206,40,171,14,245,201,0,1,114,180,106,207,166,212,234,197,139,103,37,27,41,205,28,72,136,108,250,68,200,159,124,174,16,3,237,19,202,33,71,209,231,82,132,223,206,148,36,229,111,51,208,156,105,18,53,45,202,211,114,44,133,88,180,9,255,230,141,42,33,202,91,86,69,165,230,103,66,68,147,48,204,105,211,63,114,113,20,13,163,124,202,134,49,71,80,226,195,231,25,77,229,159,244,54,68,169,64,94,26,29,169,48,48,176,82,85,74,90,0,76,47,104,15,62,147,242,60,130,179,32,17,159,118,21,36,163,136,199,228,127,148,140,226,68,15,36,20,190,227,147,9,179,166,15,20,148,215,255,254,242,253,187,102,74,226,10,115,63,254,181,129,38,218,236,200,70,178,73,18,169,128,135,207,87,193,99,128,51,89,202,107,240,224,159,36,70,60,54,249,153,105,243,33,47,73,97,142,6,51,179,102,130,171,153,41,62,73,194,199,78,55,39,118,76,205,112,242,204,206,76,69,166,19,28,232,211,96,234,45,78,163,131,42,95,65,97,231,113,163,232,43,177,143,121,4,56,25,67,148,71,17,32,93,10,80,55,31,138,238,18,98,198,73,58,223,89,206,36,74,220,6,1,181,41,132,70,75,227,24,252,186,8,136,21,209,254,70,17,169,243,177,1,115,28,8,23,13,28,1,157,214,181,80,227,40,62,110,76,117,12,98,185,236,13,206,250,193,208,213,75,95,27,7,160,135,69,8,93,210,26,59,22,125,223,216,16,246,46,245,157,200,191,101,23,184,132,113,25,79,18,24,73,99,155,47,104,160,152,136,96,43,134,49,152,57,96,116,3,7,2,195,83,51,142,226,49,216,8,209,231,108,186,83,200,159,206,163,4,166,91,7,230,64,43,80,144,35,162,241,59,154,49,250,73,221,224,194,108,52,124,168,155,75,153,79,177,62,151,127,109,118,0,179,197,147,171,184,62,74,205,7,178,192,169,41,84,195,24,131,216,130,127,74,49,27,87,90,145,212,121,235,102,195,38,109,104,214,235,154,161,131,122,219,54,49,16,163,211,163,125,75,162,73,55,13,152,206,95,197,6,255,176,179,111,169,79,26,170,116,27,106,108,26,120,196,85,3,62,216,223,157,1,77,104,0,126,152,112,240,42,24,94,123,211,240,56,200,99,222,233,193,136,59,226,122,68,157,134,169,51,146,110,169,182,234,209,182,44,104,102,49,37,158,212,109,84,99,186,10,161,190,253,135,166,245,223,182,27,32,202,155,198,91,34,85,98,40,208,74,243,101,129,10,32,175,117,200,98,67,11,91,185,86,61,140,160,31,121,71,242,209,77,220,219,70,144,126,196,186,247,166,176,82,120,152,24,23,57,220,91,48,225,6,183,134,134,249,144,37,247,15,80,230,148,108,36,20,129,112,112,197,252,201,36,8,225,239,249,23,140,228,113,78,139,19,217,14,226,240,56,134,49,222,170,88,174,253,168,166,153,48,89,158,173,33,240,209,32,115,12,15,5,187,6,232,54,141,63,68,198,102,233,131,240,6,165,82,97,122,187,187,200,203,50,170,50,44,55,202,86,99,151,171,187,164,19,66,224,55,158,67,243,129,178,47,164,18,238,2,85,76,12,49,16,216,178,188,58,249,117,50,119,210,179,125,253,234,53,207,138,175,218,215,159,80,25,191,126,229,78,84,97,1,93,114,228,40,30,91,237,92,191,164,23,68,34,95,188,40,132,73,84,246,197,124,152,25,221,10,128,234,98,53,239,227,179,62,65,17,250,110,250,190,226,164,23,104,21,110,222,122,212,163,246,170,57,41,111,159,116,34,190,53,245,25,166,3,5,29,53,191,220,220,84,129,59,34,67,81,198,198,77,14,160,168,115,240,185,67,190,25,41,164,207,20,243,240,119,254,5,8,166,15,240,215,161,55,10,172,105,178,173,81,123,141,98,32,217,113,140,250,7,33,42,145,155,79,33,202,81,233,140,56,104,22,91,171,56,54,151,215,143,170,105,228,86,121,173,163,192,185,242,199,185,92,156,6,46,113,52,159,68,34,49,133,128,184,92,87,103,55,242,117,118,44,199,71,104,150,85,144,119,69,149,98,29,131,14,18,194,72,80,117,48,254,222,233,200,192,201,127,110,15,139,205,225,152,234,235,70,250,26,89,30,138,5,30,143,32,245,175,250,122,74,228,207,182,95,30,206,162,126,194,5,20,57,35,154,204,131,198,153,121,1,58,157,7,77,211,110,1,142,227,238,151,151,13,112,130,26,249,124,82,135,16,19,191,188,34,128,16,19,13,34,245,167,166,250,8,146,74,16,17,129,75,226,104,87,117,78,124,195,1,12,12,170,232,169,25,64,176,151,188,254,244,246,59,71,183,141,240,245,91,224,127,97,13,175,100,13,164,104,60,19,176,42,152,174,192,252,183,179,201,139,65,133,88,171,147,116,23,164,106,129,5,196,5,50,13,164,63,73,40,192,155,30,101,48,31,50,0,3,32,28,93,103,122,160,139,64,52,192,78,197,96,235,26,124,92,241,189,109,216,134,78,149,239,209,174,158,83,86,143,175,95,7,46,116,188,184,54,115,230,8,240,99,67,108,101,65,196,143,11,246,146,178,32,58,117,140,37,118,19,138,119,248,224,54,216,126,113,23,204,174,188,175,135,15,63,168,76,116,166,19,150,208,69,84,57,109,64,148,188,157,74,18,214,203,162,79,119,89,14,242,226,57,6,102,187,226,46,11,168,160,223,42,165,85,58,11,147,188,210,226,189,111,64,180,248,114,171,2,255,179,36,87,192,62,80,157,249,66,7,235,70,150,24,98,69,46,75,138,10,226,85,28,33,151,26,80,184,105,70,94,56,169,54,42,249,157,148,53,218,156,218,194,205,14,190,246,79,59,142,67,220,108,146,87,169,208,134,2,30,219,54,107,76,172,197,242,204,58,99,83,44,249,34,29,178,53,196,148,27,75,196,167,95,68,102,108,242,17,2,155,94,76,99,219,193,109,31,28,206,83,172,82,184,136,140,43,17,123,115,206,36,70,145,123,100,108,10,69,130,178,69,20,41,131,188,44,69,251,179,9,58,59,123,85,164,1,149,115,9,26,114,51,191,44,17,47,103,19,97,23,73,32,205,174,166,65,161,28,6,153,79,151,190,226,53,87,120,3,109,173,176,143,237,161,116,43,242,42,182,179,141,77,174,6,155,70,65,95,15,230,237,90,11,99,82,255,131,193,17,255,1,147,171,8,17,40,225,166,161,246,174,115,138,89,97,111,125,60,233,205,35,105,197,29,246,185,164,66,83,92,212,138,196,190,120,126,176,255,242,176,195,62,76,122,218,78,251,212,64,27,155,114,130,173,7,184,38,118,17,197,128,32,180,189,152,217,195,84,180,41,229,59,120,107,2,13,146,183,36,14,89,69,11,248,170,234,10,40,30,113,253,22,242,212,156,91,5,246,146,11,194,220,73,79,239,142,199,16,197,157,13,131,208,171,103,137,41,150,190,243,9,165,200,85,153,29,150,8,0,176,141,226,105,70,88,2,95,167,194,146,194,214,162,140,73,4,160,138,73,42,208,254,229,243,102,180,88,67,210,147,198,73,86,175,187,141,158,233,116,11,148,255,216,203,167,4,95,191,90,230,86,241,171,91,252,74,124,42,4,47,215,211,177,75,121,167,82,143,91,122,14,151,189,99,99,190,111,161,59,141,11,174,69,202,44,8,136,152,77,23,230,211,245,146,128,150,38,230,83,31,197,50,117,177,188,173,230,216,42,28,224,75,225,66,172,167,145,138,82,196,86,40,224,51,166,227,122,174,16,52,5,49,167,91,120,114,167,13,214,188,55,67,61,127,163,129,233,226,50,45,13,131,152,164,154,43,153,16,236,239,12,180,75,218,140,189,189,189,165,236,131,212,186,89,246,65,159,7,202,188,193,7,41,161,105,62,30,127,156,248,201,61,95,61,137,147,147,48,172,27,207,5,106,198,111,221,110,139,112,198,160,105,215,51,168,44,53,159,175,193,137,224,111,156,196,163,32,245,83,231,199,47,29,132,145,202,3,234,32,247,204,166,82,44,100,157,230,120,146,14,235,11,179,45,108,99,65,102,133,150,84,241,255,211,39,48,219,224,209,164,5,238,15,156,207,77,23,134,87,242,92,48,142,86,212,75,203,18,107,100,15,60,245,150,84,137,162,169,237,28,18,113,93,190,101,94,234,255,27,34,142,27,135,91,124,212,164,136,47,47,218,79,37,196,51,165,102,41,161,41,142,80,97,56,244,115,3,48,224,9,230,121,189,241,36,225,55,51,67,21,14,38,214,94,36,171,250,89,18,30,39,254,21,48,9,113,129,37,128,239,240,114,51,147,97,72,93,24,187,30,167,160,146,56,121,71,227,131,154,184,206,142,159,212,29,145,32,29,0,168,59,41,152,4,183,172,29,49,41,6,65,110,54,155,134,34,28,234,109,197,226,38,198,249,202,83,196,234,193,32,151,38,220,1,136,102,91,20,237,119,24,237,36,250,73,2,30,106,224,6,33,201,45,136,219,39,158,233,200,117,100,154,210,89,87,81,26,157,2,179,58,226,250,208,215,116,109,58,234,94,163,69,91,3,69,29,92,5,127,81,89,11,173,169,173,215,68,94,128,55,159,85,124,188,192,57,58,55,78,215,176,238,64,14,154,89,44,182,33,236,125,220,57,250,30,127,193,227,204,165,253,101,152,214,80,22,112,189,213,192,196,60,181,204,152,125,140,111,157,58,104,78,35,12,112,253,153,75,30,87,179,103,222,143,240,225,75,81,245,65,104,169,20,133,115,230,150,52,225,42,46,210,2,165,245,27,213,238,2,201,135,137,66,82,23,193,56,166,227,130,186,147,127,72,77,32,139,246,149,222,64,64,16,115,5,105,64,111,65,155,111,204,135,84,228,177,136,15,157,30,68,88,215,160,198,29,222,207,186,81,184,175,206,104,24,249,53,110,165,59,146,244,26,242,200,60,129,231,47,250,97,250,18,56,158,152,148,208,252,89,59,73,89,130,197,3,68,18,150,63,107,7,139,74,2,83,186,254,53,79,217,156,171,182,242,10,90,30,94,162,184,22,22,207,97,50,97,232,41,158,4,190,37,210,183,211,133,169,114,140,205,108,152,95,72,174,178,139,12,113,35,185,8,219,102,175,30,227,69,157,229,37,126,81,151,189,125,253,203,162,250,249,205,200,121,211,91,135,123,203,84,195,155,144,203,45,139,27,143,23,82,141,119,128,206,175,43,198,10,47,49,159,61,90,244,89,206,4,232,69,244,65,237,48,246,110,205,34,190,79,208,164,195,65,133,154,252,136,80,95,142,43,202,136,62,125,94,50,187,67,167,183,165,238,240,150,212,198,143,188,243,243,135,32,13,192,247,153,69,227,201,233,159,18,182,255,248,87,118,30,71,190,154,218,47,176,213,149,183,27,27,202,18,79,229,130,46,141,128,146,67,113,215,186,212,11,71,44,94,151,178,164,243,251,161,115,165,163,92,241,57,171,13,162,138,161,237,156,204,213,83,253,130,106,94,169,76,219,179,82,1,194,16,21,77,154,57,53,69,158,189,83,2,163,117,237,72,114,189,200,162,41,80,188,253,122,223,234,176,215,160,64,51,239,102,229,203,222,139,111,201,54,196,74,73,89,84,88,73,86,30,55,150,211,230,233,148,22,90,87,48,26,114,184,96,124,86,85,112,92,170,230,42,38,119,14,81,56,138,81,18,255,185,3,233,115,29,242,64,175,160,15,48,41,93,206,248,81,160,57,56,218,177,172,175,95,7,221,195,214,129,116,170,133,224,110,0,209,220,64,165,124,236,152,115,3,187,135,149,12,168,142,22,250,73,102,244,113,58,28,228,183,179,139,142,94,57,202,213,174,96,108,205,98,159,16,0,186,117,181,110,95,42,204,234,149,54,80,229,14,224,205,252,15,106,119,113,177,153,149,147,149,187,188,175,169,52,185,165,158,244,110,105,122,114,55,191,39,162,237,158,51,223,176,210,166,107,207,76,123,133,158,165,154,185,134,150,184,181,230,221,68,91,37,50,118,47,196,206,91,217,84,149,126,240,68,59,209,17,141,157,101,210,128,59,203,76,2,212,175,178,24,106,29,180,72,151,249,80,162,147,22,28,128,134,102,226,143,226,27,255,36,3,79,214,155,128,160,229,155,109,38,125,231,86,140,150,127,28,181,79,56,61,213,40,108,192,209,103,94,145,175,238,159,137,234,98,95,77,251,222,47,125,193,38,233,71,12,235,102,190,134,42,246,246,29,252,72,82,208,4,106,71,245,146,44,136,60,105,16,8,63,194,133,217,239,63,190,57,139,71,99,176,177,148,60,54,95,68,42,135,145,250,239,103,85,204,193,84,175,10,254,28,200,222,21,103,31,218,102,228,108,214,224,222,98,53,107,248,62,227,162,225,199,95,199,41,7,59,169,123,131,115,156,146,79,95,7,147,65,57,151,75,231,131,206,193,68,201,146,194,80,104,166,34,223,187,9,188,165,166,190,106,99,137,175,139,0,108,217,128,211,18,17,64,133,65,58,92,184,146,49,127,174,187,131,39,249,163,76,77,116,95,118,24,102,39,184,153,107,44,158,221,170,221,39,99,217,249,170,104,226,34,73,214,69,175,91,97,185,131,135,140,205,211,215,80,220,195,149,210,101,162,32,155,91,99,42,101,6,72,157,91,65,109,87,170,73,40,238,129,206,173,82,74,180,121,198,187,33,71,94,24,191,233,244,160,114,9,55,128,188,242,92,27,40,64,170,205,32,231,200,220,250,2,164,186,190,232,238,92,4,18,102,166,33,134,111,243,77,177,170,60,211,24,79,161,151,223,68,239,167,44,242,20,59,185,193,20,146,53,114,68,189,130,177,150,66,228,8,150,20,62,162,43,77,251,122,20,39,251,157,199,106,65,250,206,125,7,229,230,215,175,105,255,200,177,76,168,96,119,170,242,172,70,157,170,220,170,168,83,149,79,37,230,112,128,182,83,104,82,255,32,133,96,142,47,168,148,19,114,7,82,2,230,84,174,20,18,170,172,70,127,78,237,106,9,57,200,199,118,158,51,154,41,28,202,29,85,8,7,119,72,5,239,11,172,21,222,151,54,68,230,120,226,145,89,94,47,47,35,66,118,72,68,226,185,2,209,36,90,136,136,255,244,187,192,36,95,96,64,203,235,245,218,161,196,101,14,88,108,44,191,192,226,53,177,64,11,244,59,27,171,45,147,120,77,250,109,128,141,213,87,73,120,77,149,122,185,226,234,206,20,225,114,134,178,177,226,130,203,10,100,204,62,51,51,183,26,174,16,150,170,137,180,213,124,151,84,181,78,123,65,46,255,81,155,229,206,228,201,159,192,153,162,76,96,201,147,91,167,142,245,148,78,245,124,138,199,167,110,34,214,174,117,95,60,103,102,162,253,64,168,216,0,234,105,206,46,63,216,210,119,186,198,95,226,183,67,141,205,254,166,81,248,249,43,188,49,109,254,143,137,106,121,27,16,236,209,142,20,121,229,38,54,17,227,54,17,22,243,77,39,227,35,150,181,121,170,48,125,30,165,87,166,128,191,13,6,65,9,252,115,240,42,128,14,223,70,134,4,194,40,175,4,68,247,43,1,3,212,129,38,109,94,132,80,50,57,164,55,223,49,206,112,138,61,221,160,94,254,254,18,72,143,179,186,250,174,185,201,106,252,252,102,155,105,252,121,185,22,131,18,102,241,5,229,24,76,33,246,224,231,56,136,234,6,251,202,140,82,24,93,218,32,121,208,86,145,185,40,45,177,122,44,220,248,28,169,148,63,143,171,230,167,147,57,192,218,15,227,202,69,84,76,145,238,153,81,175,164,81,128,182,41,50,126,81,171,232,137,246,147,180,15,160,180,246,177,145,242,211,105,18,217,164,39,167,12,35,253,208,147,215,228,109,227,129,165,6,30,249,224,175,191,219,183,58,147,98,219,198,247,99,96,237,104,211,24,65,123,233,49,242,25,15,67,137,51,112,26,87,6,115,58,42,126,22,154,75,230,160,135,199,81,6,183,230,160,220,203,193,109,57,35,103,22,66,245,163,202,90,87,251,132,151,84,69,61,52,65,31,213,85,13,98,143,134,14,70,168,175,142,227,200,55,255,110,76,58,33,80,50,60,212,95,96,5,253,32,115,174,143,80,123,179,142,51,33,246,254,91,224,7,123,117,242,253,119,159,12,189,182,46,198,8,152,231,207,42,153,238,87,169,194,12,88,126,223,128,206,34,61,69,103,138,71,226,71,161,117,22,101,137,58,103,238,39,105,137,51,125,39,255,164,58,145,37,165,227,11,39,159,209,244,53,233,108,19,255,213,104,124,69,103,180,89,135,98,255,231,183,252,172,8,61,127,36,95,67,143,31,66,119,146,30,131,106,214,241,86,75,246,150,99,17,224,128,234,163,44,16,174,227,131,124,167,138,155,134,137,41,74,178,47,143,249,129,142,222,109,193,91,173,178,158,198,175,160,160,244,252,170,125,139,194,150,69,58,181,97,145,206,219,174,248,77,43,107,98,239,226,81,31,106,48,110,231,224,205,150,118,101,135,185,43,59,92,248,51,216,197,223,114,76,148,3,92,194,155,149,253,153,58,91,84,233,212,120,47,54,141,163,94,151,220,155,248,101,167,54,253,228,97,229,5,65,226,135,174,11,94,16,83,170,56,5,166,222,222,180,83,212,154,67,247,184,184,153,119,49,37,237,113,223,88,217,200,180,83,213,26,65,247,186,184,145,115,205,255,178,58,246,140,144,246,146,248,26,44,138,97,150,218,93,106,3,71,251,169,113,141,94,170,100,62,84,109,187,104,238,94,219,114,129,58,122,28,176,160,57,33,175,224,56,115,191,172,57,97,188,212,86,57,137,202,64,111,122,79,236,47,216,99,99,222,74,225,204,254,24,243,70,82,253,252,58,239,145,56,127,195,179,59,248,239,196,249,9,94,226,53,114,239,89,15,127,19,208,77,40,73,118,50,86,42,244,40,46,197,120,220,40,103,112,20,207,120,107,223,26,182,60,228,189,189,205,126,253,183,63,193,255,229,203,222,168,144,150,225,199,163,247,17,95,132,104,192,51,174,151,37,148,19,170,197,224,249,237,107,121,224,61,73,66,7,171,30,27,219,244,107,69,91,105,22,143,65,76,212,27,244,68,219,137,7,240,197,209,19,145,226,53,227,168,176,35,60,103,29,115,164,22,238,25,43,90,72,78,218,101,134,142,132,247,25,104,43,220,28,103,200,106,211,222,149,87,150,94,181,173,173,25,21,28,246,76,48,37,107,0,240,226,197,51,193,85,16,58,193,94,125,224,198,163,115,60,212,220,162,181,71,101,70,120,77,85,177,31,250,110,162,85,225,197,157,194,112,205,225,18,222,54,86,94,75,46,95,37,166,11,154,54,240,72,92,49,22,230,191,77,181,116,44,220,159,55,124,252,102,54,195,108,244,179,59,167,143,0,68,223,29,68,238,45,175,104,222,62,195,119,114,92,141,215,240,196,29,87,227,29,72,139,184,94,79,88,36,81,37,187,107,14,130,48,188,36,165,196,72,9,70,69,22,126,4,187,90,183,26,86,227,115,227,181,104,2,212,100,135,13,227,36,248,5,154,119,67,22,130,117,72,41,83,29,239,192,219,198,11,237,240,31,187,112,177,79,228,191,118,94,111,239,52,24,120,61,199,110,117,74,31,193,217,61,224,90,78,219,64,28,70,163,23,100,109,250,197,116,16,30,144,21,107,0,101,247,109,66,243,141,213,220,123,108,228,208,252,246,60,83,213,105,169,58,131,188,142,93,172,99,43,232,93,1,61,112,181,22,90,0,253,69,117,246,163,191,69,226,206,122,147,193,0,111,28,140,25,93,100,24,164,116,75,31,78,86,233,39,10,196,197,129,156,197,90,255,16,24,88,143,127,212,253,134,249,80,240,102,136,7,42,151,50,148,82,193,7,39,205,208,151,241,225,9,81,157,104,116,48,70,249,76,151,244,217,29,13,184,231,67,208,241,193,37,155,39,138,201,112,37,254,141,179,165,32,49,201,10,139,3,199,234,4,71,239,58,193,230,166,10,95,243,203,224,238,156,58,82,189,25,152,191,123,215,41,125,5,254,57,117,209,7,218,176,124,17,54,161,204,60,182,229,173,74,57,44,224,9,182,223,153,223,124,46,127,184,135,254,220,111,214,161,222,241,22,8,6,134,68,102,14,131,70,1,201,118,128,112,80,107,232,28,46,95,127,138,235,119,141,123,208,127,126,89,210,131,228,133,42,87,245,169,50,224,150,24,31,167,120,154,243,168,168,4,57,151,241,216,138,99,224,209,66,54,138,163,152,206,68,229,218,129,129,101,61,164,75,186,26,187,13,232,11,246,98,171,37,195,234,92,97,40,141,157,249,48,127,72,153,11,255,179,63,78,2,240,109,184,10,72,183,88,226,248,23,78,58,224,125,66,206,143,168,120,150,144,196,124,200,236,169,33,19,215,31,57,114,60,196,176,1,219,126,247,238,75,3,188,66,233,3,22,119,242,43,247,234,238,11,84,54,19,66,255,122,143,63,154,226,146,44,235,139,60,37,174,65,182,114,200,150,130,180,171,32,119,115,200,93,5,217,82,144,143,243,243,144,170,141,241,119,111,126,184,96,255,93,222,5,250,231,255,100,226,238,206,219,32,242,226,91,44,224,124,14,34,89,36,111,107,197,57,144,234,150,193,232,206,214,188,204,150,101,118,94,214,250,50,29,84,113,179,15,33,135,184,212,241,136,220,92,119,251,27,118,243,178,105,183,105,60,39,16,170,80,179,67,204,165,193,43,76,195,224,198,223,186,9,252,91,86,39,167,128,226,132,158,213,100,155,76,230,125,226,56,130,92,134,105,135,93,251,254,152,225,210,232,246,233,231,109,250,161,220,77,121,254,152,174,199,77,217,55,219,27,94,112,211,164,216,172,61,116,211,250,115,233,34,204,198,115,61,161,168,241,92,166,1,54,158,171,60,222,7,253,87,154,159,5,163,113,156,100,110,148,61,66,183,168,55,208,63,186,146,247,104,123,152,141,194,238,198,255,1,79,123,168,223,86,149,0,0};
static const size_t PAGE_GZ_LEN = 9886;
