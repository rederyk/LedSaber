#ifndef MONITOR_PAGE_H
#define MONITOR_PAGE_H

const char MONITOR_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Serial Monitor</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Courier New', monospace;
            background: #1e1e1e;
            color: #d4d4d4;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }

        header {
            background: #2d2d30;
            padding: 15px 20px;
            border-bottom: 2px solid #007acc;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        h1 {
            font-size: 20px;
            color: #007acc;
        }

        .status {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #f48771;
            box-shadow: 0 0 10px rgba(244, 135, 113, 0.5);
        }

        .status-indicator.connected {
            background: #89d185;
            box-shadow: 0 0 10px rgba(137, 209, 133, 0.5);
        }

        .controls {
            padding: 10px 20px;
            background: #252526;
            display: flex;
            gap: 10px;
            border-bottom: 1px solid #3e3e42;
        }

        button {
            padding: 8px 16px;
            background: #0e639c;
            color: white;
            border: none;
            border-radius: 3px;
            cursor: pointer;
            font-family: inherit;
            font-size: 13px;
            transition: background 0.2s;
        }

        button:hover {
            background: #1177bb;
        }

        button.danger {
            background: #a1260d;
        }

        button.danger:hover {
            background: #c72e0d;
        }

        #logContainer {
            flex: 1;
            overflow-y: auto;
            padding: 15px 20px;
            background: #1e1e1e;
        }

        .log-entry {
            padding: 4px 0;
            border-bottom: 1px solid #2d2d30;
            font-size: 13px;
            line-height: 1.5;
        }

        .log-entry.system {
            color: #4ec9b0;
            font-weight: bold;
        }

        .log-timestamp {
            color: #858585;
            margin-right: 10px;
        }

        .log-message {
            color: #d4d4d4;
        }

        #logContainer::-webkit-scrollbar {
            width: 10px;
        }

        #logContainer::-webkit-scrollbar-track {
            background: #1e1e1e;
        }

        #logContainer::-webkit-scrollbar-thumb {
            background: #424242;
            border-radius: 5px;
        }

        #logContainer::-webkit-scrollbar-thumb:hover {
            background: #4e4e4e;
        }

        .empty-state {
            text-align: center;
            padding: 50px 20px;
            color: #858585;
        }

        footer {
            background: #007acc;
            color: white;
            padding: 8px 20px;
            text-align: center;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <header>
        <h1>ESP32 Serial Monitor</h1>
        <div class="status">
            <span id="statusText">Disconnesso</span>
            <div id="statusDot" class="status-indicator"></div>
        </div>
    </header>

    <div class="controls">
        <button id="clearBtn">Pulisci Log</button>
        <button id="scrollBtn">Auto-scroll: ON</button>
        <button id="reconnectBtn" style="display:none;" class="danger">Riconnetti</button>
    </div>

    <div id="logContainer">
        <div class="empty-state">In attesa di connessione WebSocket...</div>
    </div>

    <footer>
        ESP32 Web Monitor - Powered by WebSockets
    </footer>

    <script>
        let ws;
        let autoScroll = true;
        const logContainer = document.getElementById('logContainer');
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        const clearBtn = document.getElementById('clearBtn');
        const scrollBtn = document.getElementById('scrollBtn');
        const reconnectBtn = document.getElementById('reconnectBtn');

        function connect() {
            const wsUrl = `ws://${window.location.hostname}/ws`;
            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                console.log('WebSocket Connected');
                statusDot.classList.add('connected');
                statusText.textContent = 'Connesso';
                reconnectBtn.style.display = 'none';
                logContainer.innerHTML = '';
            };

            ws.onmessage = (event) => {
                addLogEntry(event.data);
            };

            ws.onclose = () => {
                console.log('WebSocket Disconnected');
                statusDot.classList.remove('connected');
                statusText.textContent = 'Disconnesso';
                reconnectBtn.style.display = 'inline-block';
                addLogEntry('[System] Connessione persa', true);
            };

            ws.onerror = (error) => {
                console.error('WebSocket Error:', error);
                addLogEntry('[System] Errore di connessione', true);
            };
        }

        function addLogEntry(message, isSystem = false) {
            const entry = document.createElement('div');
            entry.className = 'log-entry' + (isSystem ? ' system' : '');

            // Estrai timestamp se presente
            const timestampMatch = message.match(/^\[(\d+ms)\]\s(.+)/);
            if (timestampMatch) {
                entry.innerHTML = `<span class="log-timestamp">${timestampMatch[1]}</span><span class="log-message">${escapeHtml(timestampMatch[2])}</span>`;
            } else {
                entry.innerHTML = `<span class="log-message">${escapeHtml(message)}</span>`;
            }

            logContainer.appendChild(entry);

            // Auto-scroll
            if (autoScroll) {
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        clearBtn.addEventListener('click', () => {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('clear');
                logContainer.innerHTML = '';
                addLogEntry('[System] Log puliti', true);
            }
        });

        scrollBtn.addEventListener('click', () => {
            autoScroll = !autoScroll;
            scrollBtn.textContent = `Auto-scroll: ${autoScroll ? 'ON' : 'OFF'}`;
        });

        reconnectBtn.addEventListener('click', () => {
            connect();
        });

        // Connetti all'avvio
        connect();
    </script>
</body>
</html>
)rawliteral";

#endif
