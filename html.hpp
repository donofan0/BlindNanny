#pragma once

// --- HTML UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>BlindNanny</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
    <style>
        body { background-color: #0f172a; color: #e2e8f0; font-family: sans-serif; overflow: hidden; touch-action: none; }
        .window-frame { position: relative; width: 300px; height: 400px; background: #1e293b; border: 12px solid #334155; border-radius: 8px; box-shadow: inset 0 0 20px rgba(0,0,0,0.5); margin: 0 auto; overflow: hidden; touch-action: none; }
        .window-bg { position: absolute; inset: 0; background: linear-gradient(180deg, #38bdf8 0%, #bae6fd 100%); z-index: 0; }
        .blind-container { position: absolute; top: 0; left: 0; width: 100%; height: 0%; background-color: rgba(0,0,0,0.1); z-index: 10; border-bottom: 4px solid #94a3b8; transition: height 0.05s linear; }
        .slats { width: 100%; height: 100%; background-image: repeating-linear-gradient(to bottom, #f1f5f9 0px, #f1f5f9 20px, #cbd5e1 20px, #cbd5e1 22px); box-shadow: 0 4px 6px rgba(0,0,0,0.3); pointer-events: none; }
        .handle { position: absolute; bottom: -25px; left: 50%; transform: translateX(-50%); width: 50px; height: 50px; background: #fff; border-radius: 50%; box-shadow: 0 4px 10px rgba(0,0,0,0.5); display: flex; align-items: center; justify-content: center; cursor: grab; z-index: 50; color: #3b82f6; touch-action: none; }
        .handle:active { cursor: grabbing; background: #eff6ff; }
        .cord { position: absolute; bottom: 0; left: 50%; width: 2px; height: 25px; background: white; transform: translateX(-50%); }
        .btn { background: #1e293b; border: 1px solid #334155; padding: 10px 20px; border-radius: 8px; font-size: 14px; font-weight: 600; color: #94a3b8; transition: all 0.2s; }
        .btn:active { transform: scale(0.95); background: #334155; color: white; }
        #settings-modal, #diag-modal { transition: opacity 0.3s ease; }
        .hidden { display: none; opacity: 0; pointer-events: none; }
        .input-group { margin-bottom: 8px; }
        .input-group label { display: block; font-size: 0.75rem; color: #94a3b8; margin-bottom: 2px; }
        .input-group input, .input-group select { width: 100%; background: #334155; border: none; padding: 6px; border-radius: 4px; color: white; font-size: 0.9rem; }
        .stat-row { display: flex; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid #334155; font-size: 0.85rem; }
        .stat-label { color: #94a3b8; }
        .stat-val { font-family: monospace; color: #38bdf8; }
    </style>
</head>
<body class="flex flex-col h-screen items-center justify-center space-y-6">
    <div class="flex justify-between w-[300px] items-end">
        <div>
            <h1 class="text-xl font-bold tracking-wider">BlindNanny</h1>
            <div id="status" class="text-xs text-blue-400 font-mono">Connecting...</div>
        </div>
        <div class="flex gap-2">
            <button onclick="toggleDiag()" class="text-gray-400 hover:text-white"><i class="fa-solid fa-microchip text-xl"></i></button>
            <button onclick="toggleSettings()" class="text-gray-400 hover:text-white"><i class="fa-solid fa-gear text-xl"></i></button>
        </div>
    </div>

    <div class="window-frame" id="window">
        <div class="window-bg"><i class="fa-solid fa-cloud text-white/40 text-4xl absolute top-10 left-10"></i><i class="fa-solid fa-sun text-yellow-400/80 text-6xl absolute top-6 right-6"></i></div>
        <div class="blind-container" id="blind"><div class="slats"></div><div class="cord"></div><div class="handle" id="handle"><i class="fa-solid fa-grip-lines text-xl"></i></div></div>
    </div>
    
    <div class="text-center">
        <p class="text-3xl font-bold" id="pct-display">0%</p>
        <p class="text-xs text-gray-500 font-mono" id="meter-display">0.00m</p>
        <div id="auto-badge" class="hidden mt-1 inline-block px-2 py-0.5 bg-yellow-500/20 text-yellow-500 text-xs rounded border border-yellow-500/50">AUTO MODE</div>
    </div>

    <div class="flex gap-4">
        <button class="btn" onclick="sendPosition(0)">Open</button>
        <button class="btn text-blue-400 border-blue-900/50" onclick="triggerHome()"><i class="fa-solid fa-arrows-rotate"></i></button>
        <button class="btn" onclick="sendPosition(100)">Close</button>
    </div>

    <div id="settings-modal" class="fixed inset-0 bg-black/80 z-[100] hidden flex items-center justify-center">
        <div class="bg-slate-800 p-6 rounded-lg w-[340px] border border-slate-600 shadow-2xl max-h-[90vh] overflow-y-auto">
            <h2 class="text-lg font-bold mb-4 text-white flex justify-between">
               Configuration <i onclick="toggleSettings()" class="fa-solid fa-xmark cursor-pointer"></i>
            </h2>
            
            <div class="p-2 mb-3 bg-slate-700/30 rounded border border-slate-600">
                <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase">Physical Setup</h3>
                <div class="grid grid-cols-2 gap-3">
                     <div class="input-group"><label>M1 Max (m)</label><input type="number" id="inp_m1_max" step="0.1"></div>
                     <div class="input-group"><label>M2 Max (m)</label><input type="number" id="inp_m2_max" step="0.1"></div>
                </div>
                <div class="grid grid-cols-2 gap-3">
                     <div class="input-group"><label>Motor Count</label>
                        <select id="inp_motors">
                            <option value="1">1 Motor</option>
                            <option value="2">2 Motors</option>
                        </select>
                     </div>
                     <div class="input-group"><label>Steps/CM</label><input type="number" id="inp_spcm" step="1"></div>
                </div>
            </div>

            <div class="p-2 mb-3 bg-slate-700/30 rounded border border-slate-600">
                <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase">Motor Settings</h3>
                <div class="input-group"><label>Speed (steps/s)</label><input type="number" id="inp_speed"></div>
                <div class="grid grid-cols-2 gap-3">
                    <div class="input-group"><label>M1 Current (mA)</label><input type="number" id="inp_m1_curr"></div>
                    <div class="input-group"><label>M2 Current (mA)</label><input type="number" id="inp_m2_curr"></div>
                </div>
                <div class="grid grid-cols-2 gap-3">
                    <div class="input-group"><label>M1 Stall (0-255)</label><input type="number" id="inp_m1_stall"></div>
                    <div class="input-group"><label>M2 Stall (0-255)</label><input type="number" id="inp_m2_stall"></div>
                </div>
            </div>

            <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase">Sun Tracking</h3>
            <div class="grid grid-cols-2 gap-3">
                <div class="input-group"><label>Latitude</label><input type="number" id="inp_lat" step="0.0001"></div>
                <div class="input-group"><label>Longitude</label><input type="number" id="inp_lon" step="0.0001"></div>
            </div>
            <div class="input-group"><label>Window Direction (0-360)</label><input type="number" id="inp_az" placeholder="180"></div>
            <div class="grid grid-cols-2 gap-3">
                <div class="input-group"><label>Win Top (m)</label><input type="number" id="inp_top" step="0.1"></div>
                <div class="input-group"><label>Eye H (m)</label><input type="number" id="inp_eye" step="0.1"></div>
            </div>
            <div class="grid grid-cols-2 gap-3">
                <div class="input-group"><label>Dist (m)</label><input type="number" id="inp_dist" step="0.1"></div>
                <div class="input-group"><label>UTC Off</label><input type="number" id="inp_gmt" step="0.5"></div>
            </div>
          
            <div class="flex items-center justify-between mt-4 p-3 bg-slate-700/50 rounded">
                <span class="text-sm font-bold">Auto Sun-Block</span>
                <label class="relative inline-flex items-center cursor-pointer">
                    <input type="checkbox" id="inp_auto" class="sr-only peer">
                    <div class="w-11 h-6 bg-slate-600 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
                </label>
            </div>
            <button onclick="saveSettings()" class="w-full mt-6 bg-blue-600 hover:bg-blue-500 text-white font-bold py-2 rounded">Save & Restart</button>
        </div>
    </div>

    <div id="diag-modal" class="fixed inset-0 bg-black/80 z-[100] hidden flex items-center justify-center">
        <div class="bg-slate-800 p-6 rounded-lg w-[340px] border border-slate-600 shadow-2xl max-h-[90vh] overflow-y-auto">
            <h2 class="text-lg font-bold mb-4 text-white flex justify-between">
               Diagnostics <i onclick="toggleDiag()" class="fa-solid fa-xmark cursor-pointer"></i>
            </h2>
            
            <div class="p-3 mb-4 bg-slate-700/30 rounded border border-slate-600">
                <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase border-b border-blue-400/20 pb-1">System</h3>
                <div class="stat-row"><span class="stat-label">Uptime</span><span class="stat-val" id="d-uptime">0s</span></div>
                <div class="stat-row"><span class="stat-label">WiFi Signal</span><span class="stat-val" id="d-rssi">0 dBm</span></div>
                <div class="stat-row"><span class="stat-label">Heap Free</span><span class="stat-val" id="d-heap">0 KB</span></div>
                <div class="stat-row"><span class="stat-label">IP Address</span><span class="stat-val" id="d-ip">...</span></div>
                <div class="stat-row"><span class="stat-label">MAC Address</span><span class="stat-val" id="d-mac">...</span></div>
                <div class="stat-row"><span class="stat-label">MQTT</span><span class="stat-val" id="d-mqtt">Disconnected</span></div>
            </div>

            <div class="p-3 mb-4 bg-slate-700/30 rounded border border-slate-600">
                <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase border-b border-blue-400/20 pb-1">Motors</h3>
                <div class="grid grid-cols-2 gap-4">
                    <div>
                        <div class="text-[10px] text-gray-400 uppercase tracking-widest text-center mb-1">Motor 1</div>
                        <div class="stat-row"><span class="stat-label">Pos (Steps)</span><span class="stat-val" id="d-m1-pos">0</span></div>
                        <div class="stat-row"><span class="stat-label">Current (mA)</span><span class="stat-val" id="d-m1-current">0</span></div>
                    </div>
                    <div>
                        <div class="text-[10px] text-gray-400 uppercase tracking-widest text-center mb-1">Motor 2</div>
                        <div class="stat-row"><span class="stat-label">Pos (Steps)</span><span class="stat-val" id="d-m2-pos">0</span></div>
                        <div class="stat-row"><span class="stat-label">Current (mA)</span><span class="stat-val" id="d-m2-current">0</span></div>
                    </div>
                </div>
                <div class="stat-row mt-2"><span class="stat-label">Driver Enabled</span><span class="stat-val" id="d-en">NO</span></div>
            </div>

            <div class="p-3 bg-slate-700/30 rounded border border-slate-600">
                <h3 class="text-xs font-bold text-blue-400 mb-2 uppercase border-b border-blue-400/20 pb-1">Sun Logic</h3>
                <div class="stat-row"><span class="stat-label">Azimuth</span><span class="stat-val" id="d-az">0°</span></div>
                <div class="stat-row"><span class="stat-label">Elevation</span><span class="stat-val" id="d-el">0°</span></div>
                <div class="stat-row"><span class="stat-label">Target %</span><span class="stat-val" id="d-targ">0%</span></div>
            </div>
        </div>
    </div>

    )rawliteral" R"rawliteral(
    <script>
        const blind = document.getElementById('blind');
        const handle = document.getElementById('handle');
        const windowFrame = document.getElementById('window');
        const display = document.getElementById('pct-display');
        const displayMeters = document.getElementById('meter-display');
        const connectionStatus = document.getElementById('status');
        let MAX_METERS = 2.0; 
        let isDragging = false;
        let lastSent = 0;
        let diagInterval = null;

        function toggleSettings() {
            const m = document.getElementById('settings-modal');
            m.classList.toggle('hidden');
            if(!m.classList.contains('hidden')) fetchSettings();
        }

        function toggleDiag() {
            const m = document.getElementById('diag-modal');
            m.classList.toggle('hidden');
            if(!m.classList.contains('hidden')) {
                fetchDiag();
                diagInterval = setInterval(fetchDiag, 1000);
            } else {
                clearInterval(diagInterval);
            }
        }

        function fetchDiag() {
            fetch('/diag').then(r => r.json()).then(d => {
                document.getElementById('d-uptime').innerText = Math.floor(d.uptime/1000) + 's';
                document.getElementById('d-rssi').innerText = d.rssi + ' dBm';
                document.getElementById('d-heap').innerText = Math.floor(d.heap/1024) + ' KB';
                document.getElementById('d-ip').innerText = d.ip;
                document.getElementById('d-mac').innerText = d.mac;
                document.getElementById('d-mqtt').innerText = d.mqtt ? "Connected" : "Disconnected";
                
                document.getElementById('d-m1-pos').innerText = d.m1.pos;
                document.getElementById('d-m1-current').innerText = d.m1.current;
                document.getElementById('d-m2-pos').innerText = d.m2.pos;
                document.getElementById('d-m2-current').innerText = d.m2.current;
                document.getElementById('d-en').innerText = d.enabled ? "YES" : "NO";
                
                document.getElementById('d-az').innerText = d.sun.az.toFixed(1) + "°";
                document.getElementById('d-el').innerText = d.sun.el.toFixed(1) + "°";
                document.getElementById('d-targ').innerText = d.sun.targ + "%";
            });
        }

        function fetchSettings() {
            fetch('/get_cfg').then(r => r.json()).then(d => {
                document.getElementById('inp_lat').value = d.lat;
                document.getElementById('inp_lon').value = d.lon;
                document.getElementById('inp_az').value = d.az;
                document.getElementById('inp_top').value = d.top;
                document.getElementById('inp_eye').value = d.eye;
                document.getElementById('inp_dist').value = d.dist;
                document.getElementById('inp_gmt').value = d.gmt;
                document.getElementById('inp_auto').checked = d.auto;
                
                // Physical Setup
                document.getElementById('inp_m1_max').value = d.m1_max;
                document.getElementById('inp_m2_max').value = d.m2_max;
                document.getElementById('inp_spcm').value = d.spcm;
                document.getElementById('inp_motors').value = d.cnt;
                
                // Motors
                document.getElementById('inp_speed').value = d.speed;
                document.getElementById('inp_m1_curr').value = d.m1_curr;
                document.getElementById('inp_m2_curr').value = d.m2_curr;
                document.getElementById('inp_m1_stall').value = d.m1_stall;
                document.getElementById('inp_m2_stall').value = d.m2_stall;
                
                MAX_METERS = d.m1_max; // UI purely reflects M1 size
            });
        }

        function saveSettings() {
            const data = {
                lat: document.getElementById('inp_lat').value,
                lon: document.getElementById('inp_lon').value,
                az: document.getElementById('inp_az').value,
                top: document.getElementById('inp_top').value,
                eye: document.getElementById('inp_eye').value,
                dist: document.getElementById('inp_dist').value,
                gmt: document.getElementById('inp_gmt').value,
                auto: document.getElementById('inp_auto').checked ? 1 : 0,
                
                // Send new separate heights
                m1_max: document.getElementById('inp_m1_max').value,
                m2_max: document.getElementById('inp_m2_max').value,
                spcm: document.getElementById('inp_spcm').value,
                cnt: document.getElementById('inp_motors').value,
                
                // Send new separate motor configurations
                speed: document.getElementById('inp_speed').value,
                m1_curr: document.getElementById('inp_m1_curr').value,
                m2_curr: document.getElementById('inp_m2_curr').value,
                m1_stall: document.getElementById('inp_m1_stall').value,
                m2_stall: document.getElementById('inp_m2_stall').value
            };
            const params = new URLSearchParams(data).toString();
            fetch('/save_cfg?' + params).then(r => r.text()).then(() => { 
                toggleSettings(); 
                alert("Saved. Settings applied.");
                location.reload(); 
            });
        }

        function startDrag(e) { isDragging = true; connectionStatus.innerText = "Dragging..."; if(e.cancelable) e.preventDefault(); }
        function endDrag() { if(isDragging) { isDragging = false; sendPosition(); } }
        function drag(e) {
            if (!isDragging) return;
            e.preventDefault(); 
            const rect = windowFrame.getBoundingClientRect();
            let clientY = e.type.includes('touch') ? e.touches[0].clientY : e.clientY;
            let y = clientY - rect.top;
            if (y < 0) y = 0; if (y > rect.height) y = rect.height;
            const percentage = Math.round((y / rect.height) * 100);
            updateVisuals(percentage);
            if (Math.abs(percentage - lastSent) > 5) { sendPosition(percentage); }
        }
        function updateVisuals(pct) {
            blind.style.height = pct + '%';
            display.innerText = pct + '%';
            let m = (pct / 100.0) * MAX_METERS;
            displayMeters.innerText = m.toFixed(2) + 'm';
        }
        function sendPosition(pctOverride) {
            const val = pctOverride !== undefined ? pctOverride : parseInt(display.innerText);
            lastSent = val;
            updateVisuals(val); 
            connectionStatus.innerText = "Moving to " + val + "%...";
            fetch('/set?pos=' + val).then(r => r.text()).then(() => connectionStatus.innerText = "Done").catch(() => connectionStatus.innerText = "Error");
        }
        function triggerHome() {
            connectionStatus.innerText = "Homing...";
            fetch('/home').then(r => r.text()).then(() => {
                connectionStatus.innerText = "Calibrating...";
                setTimeout(() => { updateVisuals(0); }, 3000);
            });
        }
        
        handle.addEventListener('mousedown', startDrag);
        handle.addEventListener('touchstart', startDrag, {passive: false});
        document.addEventListener('mousemove', drag);
        document.addEventListener('touchmove', drag, {passive: false});
        document.addEventListener('mouseup', endDrag);
        document.addEventListener('touchend', endDrag);
        
        fetch('/get_cfg').then(r => r.json()).then(d => { MAX_METERS = d.m1_max; });
        setInterval(() => {
            if (!isDragging) {
                fetch('/status').then(r => r.json()).then(data => {
                    if(Math.abs(data.pos - parseInt(display.innerText)) > 2) { updateVisuals(data.pos); }
                    if(data.auto) document.getElementById('auto-badge').classList.remove('hidden');
                    else document.getElementById('auto-badge').classList.add('hidden');
                    if(connectionStatus.innerText === "Connecting...") connectionStatus.innerText = "Ready";
                }).catch(e => {});
            }
        }, 3000);
    </script>
</body>
</html>
)rawliteral";
