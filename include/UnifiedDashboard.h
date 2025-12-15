#ifndef UNIFIED_DASHBOARD_H
#define UNIFIED_DASHBOARD_H

const char UNIFIED_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        .card {
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.1);
            overflow: hidden;
            margin-bottom: 20px;
        }

        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }

        .header h1 {
            font-size: 28px;
            margin-bottom: 10px;
        }

        .header .subtitle {
            opacity: 0.9;
            font-size: 14px;
        }

        .status-bar {
            display: flex;
            justify-content: space-around;
            padding: 20px;
            background: #f8f9fa;
            border-bottom: 1px solid #e9ecef;
            flex-wrap: wrap;
            gap: 15px;
        }

        .status-item {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #dc3545;
            box-shadow: 0 0 0 3px rgba(220, 53, 69, 0.2);
            animation: pulse 2s infinite;
        }

        .status-indicator.connected {
            background: #28a745;
            box-shadow: 0 0 0 3px rgba(40, 167, 69, 0.2);
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .status-label {
            font-size: 13px;
            color: #6c757d;
        }

        .status-value {
            font-weight: 600;
            color: #212529;
        }

        .tabs {
            display: flex;
            border-bottom: 2px solid #e9ecef;
            background: #f8f9fa;
        }

        .tab {
            flex: 1;
            padding: 18px 20px;
            text-align: center;
            cursor: pointer;
            border: none;
            background: transparent;
            font-size: 15px;
            font-weight: 500;
            color: #6c757d;
            transition: all 0.3s ease;
            position: relative;
        }

        .tab:hover {
            background: rgba(102, 126, 234, 0.05);
            color: #667eea;
        }

        .tab.active {
            color: #667eea;
            background: white;
        }

        .tab.active::after {
            content: '';
            position: absolute;
            bottom: -2px;
            left: 0;
            right: 0;
            height: 2px;
            background: #667eea;
        }

        .tab-content {
            display: none;
            padding: 30px;
            animation: fadeIn 0.3s;
        }

        .tab-content.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* MONITOR TAB */
        .controls {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }

        button {
            padding: 10px 20px;
            background: #667eea;
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
            transition: all 0.2s;
        }

        button:hover {
            background: #5568d3;
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.3);
        }

        button:active {
            transform: translateY(0);
        }

        button.secondary {
            background: #6c757d;
        }

        button.secondary:hover {
            background: #5a6268;
        }

        button.danger {
            background: #dc3545;
        }

        button.danger:hover {
            background: #c82333;
        }

        #logContainer {
            background: #1e1e1e;
            border-radius: 8px;
            padding: 20px;
            height: 500px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 13px;
        }

        .log-entry {
            padding: 6px 0;
            border-bottom: 1px solid #2d2d30;
            line-height: 1.6;
            color: #d4d4d4;
        }

        .log-entry.system {
            color: #4ec9b0;
            font-weight: bold;
        }

        .log-timestamp {
            color: #858585;
            margin-right: 12px;
        }

        .empty-state {
            text-align: center;
            padding: 80px 20px;
            color: #858585;
        }

        #logContainer::-webkit-scrollbar {
            width: 8px;
        }

        #logContainer::-webkit-scrollbar-track {
            background: #1e1e1e;
        }

        #logContainer::-webkit-scrollbar-thumb {
            background: #424242;
            border-radius: 4px;
        }

        /* OTA TAB */
        .ota-section {
            max-width: 600px;
            margin: 0 auto;
        }

        .upload-area {
            border: 3px dashed #dee2e6;
            border-radius: 12px;
            padding: 60px 40px;
            text-align: center;
            transition: all 0.3s;
            cursor: pointer;
            background: #f8f9fa;
        }

        .upload-area:hover {
            border-color: #667eea;
            background: rgba(102, 126, 234, 0.05);
        }

        .upload-area.dragging {
            border-color: #667eea;
            background: rgba(102, 126, 234, 0.1);
        }

        .upload-icon {
            font-size: 64px;
            margin-bottom: 20px;
        }

        .upload-area h3 {
            color: #212529;
            margin-bottom: 10px;
            font-size: 20px;
        }

        .upload-area p {
            color: #6c757d;
            font-size: 14px;
        }

        #fileInput {
            display: none;
        }

        .file-info {
            margin-top: 20px;
            padding: 20px;
            background: #e7f3ff;
            border-radius: 8px;
            display: none;
        }

        .file-info.show {
            display: block;
        }

        .file-name {
            font-weight: 600;
            color: #212529;
            margin-bottom: 5px;
        }

        .file-size {
            color: #6c757d;
            font-size: 13px;
        }

        .progress-container {
            margin-top: 20px;
            display: none;
        }

        .progress-container.show {
            display: block;
        }

        .progress-bar-bg {
            background: #e9ecef;
            border-radius: 10px;
            height: 24px;
            overflow: hidden;
            position: relative;
        }

        .progress-bar {
            background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
            height: 100%;
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: 12px;
            font-weight: 600;
        }

        .upload-btn {
            margin-top: 20px;
            width: 100%;
            padding: 14px;
            font-size: 16px;
        }

        .upload-status {
            margin-top: 15px;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
            display: none;
        }

        .upload-status.show {
            display: block;
        }

        .upload-status.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }

        .upload-status.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }

        /* INFO TAB */
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }

        .info-card {
            background: #f8f9fa;
            padding: 25px;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }

        .info-card h3 {
            color: #667eea;
            font-size: 14px;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .info-card .value {
            font-size: 24px;
            font-weight: 600;
            color: #212529;
            margin-bottom: 5px;
        }

        .info-card .label {
            color: #6c757d;
            font-size: 13px;
        }

        .footer {
            text-align: center;
            padding: 20px;
            color: white;
            font-size: 13px;
        }

        @media (max-width: 768px) {
            .status-bar {
                flex-direction: column;
                align-items: flex-start;
            }

            .tabs {
                overflow-x: auto;
            }

            .tab {
                white-space: nowrap;
            }

            .upload-area {
                padding: 40px 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <div class="header">
                <h1>ESP32 Dashboard</h1>
                <div class="subtitle">Sistema di Monitoraggio e Aggiornamento OTA</div>
            </div>

            <div class="status-bar">
                <div class="status-item">
                    <div id="wsStatusDot" class="status-indicator"></div>
                    <div>
                        <div class="status-label">WebSocket</div>
                        <div class="status-value" id="wsStatus">Disconnesso</div>
                    </div>
                </div>
                <div class="status-item">
                    <div>
                        <div class="status-label">IP Address</div>
                        <div class="status-value" id="ipAddress">-</div>
                    </div>
                </div>
                <div class="status-item">
                    <div>
                        <div class="status-label">Uptime</div>
                        <div class="status-value" id="uptime">-</div>
                    </div>
                </div>
                <div class="status-item">
                    <div>
                        <div class="status-label">Free Heap</div>
                        <div class="status-value" id="freeHeap">-</div>
                    </div>
                </div>
            </div>

            <div class="tabs">
                <button class="tab active" onclick="switchTab('monitor')">Monitor Log</button>
                <button class="tab" onclick="switchTab('ota')">Aggiornamento OTA</button>
                <button class="tab" onclick="switchTab('info')">Informazioni Sistema</button>
            </div>

            <div id="monitor" class="tab-content active">
                <div class="controls">
                    <button onclick="clearLogs()">Pulisci Log</button>
                    <button id="scrollBtn" onclick="toggleAutoScroll()" class="secondary">Auto-scroll: ON</button>
                    <button id="reconnectBtn" onclick="reconnectWebSocket()" class="danger" style="display:none;">Riconnetti WebSocket</button>
                </div>
                <div id="logContainer">
                    <div class="empty-state">In attesa di connessione WebSocket...</div>
                </div>
            </div>

            <div id="ota" class="tab-content">
                <div class="ota-section">
                    <div class="upload-area" id="uploadArea" onclick="document.getElementById('fileInput').click()">
                        <div class="upload-icon">📦</div>
                        <h3>Carica Firmware</h3>
                        <p>Clicca per selezionare o trascina qui il file .bin</p>
                        <input type="file" id="fileInput" accept=".bin" onchange="handleFileSelect(event)">
                    </div>

                    <div id="fileInfo" class="file-info">
                        <div class="file-name" id="fileName"></div>
                        <div class="file-size" id="fileSize"></div>
                    </div>

                    <div id="progressContainer" class="progress-container">
                        <div class="progress-bar-bg">
                            <div id="progressBar" class="progress-bar">0%</div>
                        </div>
                    </div>

                    <button id="uploadBtn" class="upload-btn" onclick="uploadFirmware()" style="display:none;">
                        Avvia Aggiornamento
                    </button>

                    <div id="uploadStatus" class="upload-status"></div>
                </div>
            </div>

            <div id="info" class="tab-content">
                <div class="info-grid">
                    <div class="info-card">
                        <h3>Chip Model</h3>
                        <div class="value" id="chipModel">ESP32</div>
                        <div class="label">Modello processore</div>
                    </div>
                    <div class="info-card">
                        <h3>WiFi SSID</h3>
                        <div class="value" id="wifiSSID">-</div>
                        <div class="label">Rete connessa</div>
                    </div>
                    <div class="info-card">
                        <h3>Signal Strength</h3>
                        <div class="value" id="rssi">-</div>
                        <div class="label">Potenza segnale WiFi</div>
                    </div>
                    <div class="info-card">
                        <h3>Firmware Version</h3>
                        <div class="value" id="fwVersion">2.0</div>
                        <div class="label">Versione corrente</div>
                    </div>
                </div>
            </div>
        </div>

        <div class="footer">
            ESP32 Unified Dashboard - Powered by WebSockets & AsyncWebServer
        </div>
    </div>

    <script>
        let ws;
        let autoScroll = true;
        let selectedFile = null;

        // Tab Switching
        function switchTab(tabName) {
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

            event.target.classList.add('active');
            document.getElementById(tabName).classList.add('active');
        }

        // WebSocket Connection
        function connectWebSocket() {
            const wsUrl = `ws://${window.location.hostname}/ws`;
            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                console.log('WebSocket Connected');
                document.getElementById('wsStatusDot').classList.add('connected');
                document.getElementById('wsStatus').textContent = 'Connesso';
                document.getElementById('reconnectBtn').style.display = 'none';
                document.getElementById('logContainer').innerHTML = '';
                updateSystemInfo();
            };

            ws.onmessage = (event) => {
                addLogEntry(event.data);
            };

            ws.onclose = () => {
                console.log('WebSocket Disconnected');
                document.getElementById('wsStatusDot').classList.remove('connected');
                document.getElementById('wsStatus').textContent = 'Disconnesso';
                document.getElementById('reconnectBtn').style.display = 'inline-block';
                addLogEntry('[System] Connessione WebSocket persa', true);
            };

            ws.onerror = (error) => {
                console.error('WebSocket Error:', error);
            };
        }

        function reconnectWebSocket() {
            connectWebSocket();
        }

        function addLogEntry(message, isSystem = false) {
            const entry = document.createElement('div');
            entry.className = 'log-entry' + (isSystem ? ' system' : '');

            const timestampMatch = message.match(/^\[(\d+ms)\]\s(.+)/);
            if (timestampMatch) {
                entry.innerHTML = `<span class="log-timestamp">${timestampMatch[1]}</span><span>${escapeHtml(timestampMatch[2])}</span>`;
            } else {
                entry.innerHTML = `<span>${escapeHtml(message)}</span>`;
            }

            const container = document.getElementById('logContainer');
            container.appendChild(entry);

            if (autoScroll) {
                container.scrollTop = container.scrollHeight;
            }
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        function clearLogs() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('clear');
                document.getElementById('logContainer').innerHTML = '';
                addLogEntry('[System] Log puliti', true);
            }
        }

        function toggleAutoScroll() {
            autoScroll = !autoScroll;
            document.getElementById('scrollBtn').textContent = `Auto-scroll: ${autoScroll ? 'ON' : 'OFF'}`;
        }

        // OTA Upload
        const uploadArea = document.getElementById('uploadArea');

        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, preventDefaults, false);
        });

        function preventDefaults(e) {
            e.preventDefault();
            e.stopPropagation();
        }

        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => uploadArea.classList.add('dragging'), false);
        });

        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => uploadArea.classList.remove('dragging'), false);
        });

        uploadArea.addEventListener('drop', (e) => {
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                handleFile(files[0]);
            }
        });

        function handleFileSelect(event) {
            const file = event.target.files[0];
            if (file) {
                handleFile(file);
            }
        }

        function handleFile(file) {
            if (!file.name.endsWith('.bin')) {
                alert('Per favore seleziona un file .bin valido');
                return;
            }

            selectedFile = file;
            document.getElementById('fileName').textContent = file.name;
            document.getElementById('fileSize').textContent = `Dimensione: ${(file.size / 1024).toFixed(2)} KB`;
            document.getElementById('fileInfo').classList.add('show');
            document.getElementById('uploadBtn').style.display = 'block';
            document.getElementById('uploadStatus').classList.remove('show');
        }

        async function uploadFirmware() {
            if (!selectedFile) return;

            const formData = new FormData();
            formData.append('update', selectedFile);

            const progressContainer = document.getElementById('progressContainer');
            const progressBar = document.getElementById('progressBar');
            const uploadStatus = document.getElementById('uploadStatus');
            const uploadBtn = document.getElementById('uploadBtn');

            progressContainer.classList.add('show');
            uploadBtn.disabled = true;
            uploadStatus.classList.remove('show');

            const xhr = new XMLHttpRequest();

            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percent + '%';
                    progressBar.textContent = percent + '%';
                }
            });

            xhr.addEventListener('load', () => {
                if (xhr.status === 200) {
                    uploadStatus.className = 'upload-status show success';
                    uploadStatus.textContent = 'Aggiornamento completato! Il dispositivo si sta riavviando...';
                    setTimeout(() => {
                        location.reload();
                    }, 5000);
                } else {
                    uploadStatus.className = 'upload-status show error';
                    uploadStatus.textContent = 'Errore durante l\'aggiornamento. Riprova.';
                    uploadBtn.disabled = false;
                }
            });

            xhr.addEventListener('error', () => {
                uploadStatus.className = 'upload-status show error';
                uploadStatus.textContent = 'Errore di connessione. Verifica la rete.';
                uploadBtn.disabled = false;
            });

            xhr.open('POST', '/update');
            xhr.send(formData);
        }

        // System Info Update
        function updateSystemInfo() {
            document.getElementById('ipAddress').textContent = window.location.hostname;

            // Simula aggiornamento periodico uptime e heap
            setInterval(() => {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    // Questi valori andrebbero richiesti al dispositivo via WebSocket
                    // Per ora mostriamo placeholder
                }
            }, 5000);
        }

        // Initialize
        connectWebSocket();
        updateSystemInfo();
    </script>
</body>
</html>
)rawliteral";

#endif
