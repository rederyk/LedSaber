#ifndef MINIMAL_DASHBOARD_H
#define MINIMAL_DASHBOARD_H

const char MINIMAL_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32</title>
<style>
body{font-family:Arial;margin:20px;background:#f0f0f0}
.card{background:#fff;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
.status{display:flex;gap:20px;flex-wrap:wrap;margin-bottom:20px}
.status-item{flex:1;min-width:150px}
.indicator{width:10px;height:10px;border-radius:50%;background:#dc3545;display:inline-block}
.indicator.on{background:#28a745}
button{padding:10px 20px;margin:5px;background:#007bff;color:#fff;border:none;border-radius:4px;cursor:pointer}
button:hover{background:#0056b3}
#log{background:#1e1e1e;color:#d4d4d4;padding:15px;height:400px;overflow-y:auto;font-family:monospace;font-size:12px}
.log-entry{padding:4px 0;border-bottom:1px solid #333}
</style>
</head>
<body>
<div class="card">
<h1>ESP32 Dashboard</h1>
<div class="status">
<div class="status-item">
<span id="wsDot" class="indicator"></span>
<span id="wsStatus">Disconnesso</span>
</div>
<div class="status-item">IP: <span id="ip">-</span></div>
</div>
</div>

<div class="card">
<h2>Monitor</h2>
<button onclick="clearLog()">Pulisci</button>
<button onclick="reconnect()">Riconnetti</button>
<div id="log"></div>
</div>

<div class="card">
<h2>OTA Update</h2>
<input type="file" id="file" accept=".bin">
<button onclick="upload()">Carica Firmware</button>
<div id="progress"></div>
</div>

<script>
let ws;
function connect(){
ws=new WebSocket('ws://'+location.hostname+'/ws');
ws.onopen=()=>{
document.getElementById('wsDot').classList.add('on');
document.getElementById('wsStatus').textContent='Connesso';
document.getElementById('ip').textContent=location.hostname;
document.getElementById('log').innerHTML='';
};
ws.onmessage=(e)=>{
let d=document.createElement('div');
d.className='log-entry';
d.textContent=e.data;
document.getElementById('log').appendChild(d);
document.getElementById('log').scrollTop=document.getElementById('log').scrollHeight;
};
ws.onclose=()=>{
document.getElementById('wsDot').classList.remove('on');
document.getElementById('wsStatus').textContent='Disconnesso';
};
}
function reconnect(){connect();}
function clearLog(){
if(ws&&ws.readyState===WebSocket.OPEN){
ws.send('clear');
document.getElementById('log').innerHTML='';
}
}
function upload(){
let f=document.getElementById('file').files[0];
if(!f){alert('Seleziona un file');return;}
let fd=new FormData();
fd.append('update',f);
let xhr=new XMLHttpRequest();
xhr.upload.onprogress=(e)=>{
if(e.lengthComputable){
let p=Math.round((e.loaded/e.total)*100);
document.getElementById('progress').textContent=p+'%';
}
};
xhr.onload=()=>{
if(xhr.status===200){
document.getElementById('progress').textContent='OK! Riavvio...';
setTimeout(()=>location.reload(),5000);
}else{
document.getElementById('progress').textContent='ERRORE';
}
};
xhr.open('POST','/update');
xhr.send(fd);
}
connect();
</script>
</body>
</html>
)rawliteral";

#endif
