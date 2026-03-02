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
.save{background:#1e7a4a;color:#fff;border:none;border-radius:4px;transition:background .2s}
.save.dirty{background:#b06000}
.poll{background:#4a3a7a;color:#fff;border:none;border-radius:4px}
.footer{margin-top:2em;font-size:.8em;color:#555}
select{background:#2a2a2a;color:#e0e0e0;border:1px solid #444;border-radius:4px;padding:.4em;font-size:1em}
.locrow{display:grid;grid-template-columns:auto 1fr auto;gap:.5em;margin-top:.5em;align-items:center}
.locrow input,.locrow .btn{margin-top:0}
.loc{background:#3a5a7a;color:#fff;border:none;border-radius:4px;white-space:nowrap;padding:.4em .8em;font-size:1em;cursor:pointer}
.locstatus{font-size:.8em;color:#aaa;margin-top:.4em;min-height:1.2em}
</style>
</head><body>
<h1>Weather LEDs Config</h1>
<div style="display:%s;background:#1a2a3a;border:1px solid #2a5a8a;border-radius:4px;padding:.8em;margin-bottom:1em;color:#6aaddf">&#9432; Setup mode &mdash; connect to <strong>ESP32-Weather</strong>, then enter your WiFi credentials below and save.</div>
<form method="POST" action="/save">
<label>WiFi Network (SSID)</label>
<div class="locrow">
<input type="text" name="wifi_ssid" id="ssidInput" maxlength="63" value="%s" autocomplete="off" list="ssidList" style="grid-column:1/3">
<datalist id="ssidList"></datalist>
<button type="button" class="btn loc" onclick="scanWifi()">Scan</button>
</div>
<div id="scanStatus" class="locstatus"></div>
<label>WiFi Password</label>
<input type="password" name="wifi_pass" maxlength="63" placeholder="leave blank to keep current" autocomplete="new-password">
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
<select id="countrySelect">
<option value="AD">AD – Andorra</option>
<option value="AS">AS – American Samoa</option>
<option value="AR">AR – Argentina</option>
<option value="AU">AU – Australia</option>
<option value="AT">AT – Austria</option>
<option value="BD">BD – Bangladesh</option>
<option value="BE">BE – Belgium</option>
<option value="BG">BG – Bulgaria</option>
<option value="BR">BR – Brazil</option>
<option value="CA">CA – Canada</option>
<option value="HR">HR – Croatia</option>
<option value="CZ">CZ – Czechia</option>
<option value="DK">DK – Denmark</option>
<option value="DO">DO – Dominican Republic</option>
<option value="FO">FO – Faroe Islands</option>
<option value="FI">FI – Finland</option>
<option value="FR">FR – France</option>
<option value="GF">GF – French Guiana</option>
<option value="DE">DE – Germany</option>
<option value="GL">GL – Greenland</option>
<option value="GP">GP – Guadeloupe</option>
<option value="GG">GG – Guernsey</option>
<option value="GU">GU – Guam</option>
<option value="GT">GT – Guatemala</option>
<option value="HU">HU – Hungary</option>
<option value="IS">IS – Iceland</option>
<option value="IN">IN – India</option>
<option value="IE">IE – Ireland</option>
<option value="IM">IM – Isle of Man</option>
<option value="IT">IT – Italy</option>
<option value="JP">JP – Japan</option>
<option value="JE">JE – Jersey</option>
<option value="LV">LV – Latvia</option>
<option value="LI">LI – Liechtenstein</option>
<option value="LT">LT – Lithuania</option>
<option value="LU">LU – Luxembourg</option>
<option value="MW">MW – Malawi</option>
<option value="MY">MY – Malaysia</option>
<option value="MH">MH – Marshall Islands</option>
<option value="MQ">MQ – Martinique</option>
<option value="MT">MT – Malta</option>
<option value="YT">YT – Mayotte</option>
<option value="MX">MX – Mexico</option>
<option value="MD">MD – Moldova</option>
<option value="MC">MC – Monaco</option>
<option value="NL">NL – Netherlands</option>
<option value="NC">NC – New Caledonia</option>
<option value="NZ">NZ – New Zealand</option>
<option value="MK">MK – North Macedonia</option>
<option value="MP">MP – Northern Mariana Islands</option>
<option value="NO">NO – Norway</option>
<option value="PK">PK – Pakistan</option>
<option value="PH">PH – Philippines</option>
<option value="PL">PL – Poland</option>
<option value="PT">PT – Portugal</option>
<option value="PR">PR – Puerto Rico</option>
<option value="RE">RE – Réunion</option>
<option value="RO">RO – Romania</option>
<option value="RU">RU – Russia</option>
<option value="PM">PM – Saint Pierre and Miquelon</option>
<option value="SM">SM – San Marino</option>
<option value="SK">SK – Slovakia</option>
<option value="SI">SI – Slovenia</option>
<option value="ZA">ZA – South Africa</option>
<option value="KR">KR – South Korea</option>
<option value="ES">ES – Spain</option>
<option value="LK">LK – Sri Lanka</option>
<option value="SJ">SJ – Svalbard and Jan Mayen</option>
<option value="SE">SE – Sweden</option>
<option value="CH">CH – Switzerland</option>
<option value="TH">TH – Thailand</option>
<option value="TR">TR – Turkey</option>
<option value="UA">UA – Ukraine</option>
<option value="GB">GB – United Kingdom</option>
<option value="US" selected>US – United States</option>
<option value="VA">VA – Vatican City</option>
<option value="VI">VI – U.S. Virgin Islands</option>
<option value="WF">WF – Wallis and Futuna</option>
</select>
<input type="text" id="zipInput" placeholder="Postal code">
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
<button id="saveBtn" class="btn save" type="submit">Save</button>
</form>
<form method="POST" action="/poll">
<button class="btn poll" type="submit">Poll Now</button>
</form>
<div class="footer">Device IP: %s</div>
<script>
var initVals={};
(function(){
  var form=document.querySelector('form[action="/save"]');
  Array.prototype.forEach.call(form.elements,function(el){
    if(el.name)initVals[el.name]=el.value;
  });
  form.addEventListener('input',function(){
    var dirty=Array.prototype.some.call(form.elements,function(el){
      return el.name&&el.value!==initVals[el.name];
    });
    document.getElementById('saveBtn').classList.toggle('dirty',dirty);
  });
})();
function setLocStatus(m){document.getElementById('locStatus').textContent=m;}
function scanWifi(){
  var st=document.getElementById('scanStatus');
  st.textContent='Scanning\u2026';
  fetch('/scan')
    .then(function(r){if(!r.ok)throw 0;return r.json();})
    .then(function(nets){
      var dl=document.getElementById('ssidList');
      dl.innerHTML='';
      if(!nets.length){st.textContent='No networks found';return;}
      nets.forEach(function(n){
        var o=document.createElement('option');
        o.value=n.ssid;
        o.label=n.ssid+' ('+n.rssi+' dBm)';
        dl.appendChild(o);
      });
      st.textContent='Found '+nets.length+' network'+(nets.length===1?'':'s');
    })
    .catch(function(){st.textContent='Scan failed';});
}
function lookupZip(){
  var cc=document.getElementById('countrySelect').value;
  var z=document.getElementById('zipInput').value.trim();
  if(!z){setLocStatus('Enter a postal code');return;}
  setLocStatus('Looking up...');
  fetch('https://api.zippopotam.us/'+cc+'/'+encodeURIComponent(z))
    .then(function(r){if(!r.ok)throw 0;return r.json();})
    .then(function(d){
      var p=d.places[0];
      document.getElementById('latInput').value=parseFloat(p.latitude).toFixed(4);
      document.getElementById('lonInput').value=parseFloat(p.longitude).toFixed(4);
      document.getElementById('saveBtn').classList.add('dirty');
      setLocStatus(p['place name']+', '+(p['state abbreviation']||cc));
    })
    .catch(function(){setLocStatus('Postal code not found');});
}
</script>
</body></html>)rawhtml";
