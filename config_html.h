#pragma once

const char CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html>
<head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weather LEDs Config</title>
<style>
body{font-family:sans-serif;max-width:500px;margin:2em auto;padding:0 1em;background:#1a1a1a;color:#e0e0e0}
h1{font-size:1.3em}
label{display:block;margin-top:1em;font-size:.9em;color:#aaa}
input[type=number],input[type=text]{width:100%%;box-sizing:border-box;padding:.4em;font-size:1em;background:#2a2a2a;color:#e0e0e0;border:1px solid #444;border-radius:4px}
.btn{margin-top:1.2em;padding:.5em 1.2em;font-size:1em;cursor:pointer}
.save{background:#1e7a4a;color:#fff;border:none;border-radius:4px}
.poll{background:#4a3a7a;color:#fff;border:none;border-radius:4px}
.footer{margin-top:2em;font-size:.8em;color:#555}
.locrow{display:flex;gap:.5em;margin-top:.5em}
.locrow input{flex:1;margin-top:0}
.loc{background:#3a5a7a;color:#fff;border:none;border-radius:4px;white-space:nowrap;padding:.4em .8em;font-size:1em;cursor:pointer}
.locstatus{font-size:.8em;color:#aaa;margin-top:.4em;min-height:1.2em}
</style>
</head><body>
<h1>Weather LEDs Config</h1>
<form method="POST" action="/save">
<label>Brightness (0-255)</label>
<input type="number" name="brightness" min="0" max="255" value="%d">
<label>Poll interval (minutes)</label>
<input type="number" name="poll_min" min="1" max="1440" value="%d">
<label>Cold temperature &deg;F (blue end)</label>
<input type="number" name="cold_temp" step="0.1" value="%.1f">
<label>Hot temperature &deg;F (red end)</label>
<input type="number" name="hot_temp" step="0.1" value="%.1f">
<label>Location</label>
<div class="locrow">
<input type="text" id="zipInput" placeholder="ZIP code" maxlength="10">
<button type="button" class="btn loc" onclick="lookupZip()">Look up</button>
</div>
<div id="locStatus" class="locstatus"></div>
<label>Latitude</label>
<input type="text" id="latInput" name="latitude" maxlength="12" value="%s">
<label>Longitude</label>
<input type="text" id="lonInput" name="longitude" maxlength="12" value="%s">
<label>Freeze threshold &deg;F</label>
<input type="number" name="freeze_thr" step="0.1" value="%.1f">
<label>Heat threshold &deg;F</label>
<input type="number" name="heat_thr" step="0.1" value="%.1f">
<label>Precipitation threshold %%</label>
<input type="number" name="precip_thr" step="0.1" min="0" max="100" value="%.1f">
<br>
<button class="btn save" type="submit">Save</button>
</form>
<form method="POST" action="/poll">
<button class="btn poll" type="submit">Poll Now</button>
</form>
<div class="footer">Device IP: %s</div>
<script>
function setLocStatus(m){document.getElementById('locStatus').textContent=m;}
function lookupZip(){
  var z=document.getElementById('zipInput').value.trim();
  if(!/^\d{5}$/.test(z)){setLocStatus('Enter a 5-digit ZIP');return;}
  setLocStatus('Looking up...');
  fetch('https://api.zippopotam.us/us/'+z)
    .then(function(r){if(!r.ok)throw 0;return r.json();})
    .then(function(d){
      var p=d.places[0];
      document.getElementById('latInput').value=parseFloat(p.latitude).toFixed(4);
      document.getElementById('lonInput').value=parseFloat(p.longitude).toFixed(4);
      setLocStatus(p['place name']+', '+p['state abbreviation']);
    })
    .catch(function(){setLocStatus('ZIP not found');});
}
</script>
</body></html>)rawhtml";
