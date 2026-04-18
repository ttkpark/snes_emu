/**
 * SNES Emulator Web Server
 * Spawns snes_emu_complete.exe --headless and streams frames via WebSocket.
 *
 * Usage:
 *   node server.js [rom_path] [exe_path] [port]
 *   node server.js "../SNES Test Program.sfc" ../snes_emu_complete.exe 3000
 */

'use strict';

const http   = require('http');
const fs     = require('fs');
const path   = require('path');
const { spawn } = require('child_process');
const { WebSocketServer, WebSocket } = require('ws');

// Config from args / env
const ROOT     = path.join(__dirname, '..');
const ROM_PATH = process.argv[2] || path.join(ROOT, 'spctest.sfc');
const EXE_PATH = process.argv[3] || path.join(ROOT, 'snes_emu_complete.exe');
const PORT     = parseInt(process.argv[4] || process.env.PORT || '3000', 10);

// Frame protocol constants
const MAGIC          = Buffer.from('FRME');
const FRAME_HDR_SIZE = 12;   // magic(4) + frame_num(4) + apu_ports(4)
const FRAME_W        = 256;
const FRAME_H        = 224;
const RGBA_SIZE      = FRAME_W * FRAME_H * 4; // 229376
const FRAME_TOTAL    = FRAME_HDR_SIZE + RGBA_SIZE; // 229388

// ── Available ROMs ────────────────────────────────────────────────────────────
function scanROMs() {
    const roms = [];
    try {
        const files = fs.readdirSync(ROOT);
        for (const f of files) {
            if (f.toLowerCase().endsWith('.sfc') || f.toLowerCase().endsWith('.smc')) {
                roms.push({ name: f, path: path.join(ROOT, f) });
            }
        }
    } catch(e) {}
    return roms;
}

console.log(`[SERVER] ROM  : ${ROM_PATH}`);
console.log(`[SERVER] EXE  : ${EXE_PATH}`);
console.log(`[SERVER] PORT : ${PORT}`);

// ── Emulator process ──────────────────────────────────────────────────────────
let emu        = null;
let emuRunning = false;
let currentROM = ROM_PATH;

function startEmulator(romPath) {
    if (emu) {
        emu.removeAllListeners();
        try { emu.kill(); } catch(e) {}
        emu = null;
        emuRunning = false;
    }

    if (!fs.existsSync(EXE_PATH)) {
        console.error(`[SERVER] ERROR: exe not found: ${EXE_PATH}`);
        return;
    }
    if (!fs.existsSync(romPath)) {
        console.error(`[SERVER] ERROR: ROM not found: ${romPath}`);
        return;
    }

    currentROM = romPath;
    g_frameNum  = 0;
    g_frameRGBA = null;
    g_apuPorts  = [0, 0, 0, 0];
    g_frameCount = 0;

    console.log(`[SERVER] Spawning emulator with ROM: ${path.basename(romPath)}`);
    emu = spawn(EXE_PATH, [romPath, '--headless'], {
        stdio: ['pipe', 'pipe', 'inherit']
    });
    emuRunning = true;

    emu.on('error', (err) => {
        console.error('[SERVER] Emulator spawn error:', err.message);
        emuRunning = false;
    });

    emu.on('exit', (code, sig) => {
        console.log(`[SERVER] Emulator exited (code=${code}, sig=${sig})`);
        emuRunning = false;
    });

    // ── Frame parsing ─────────────────────────────────────────────────────────
    let buf = Buffer.alloc(0);

    emu.stdout.on('data', (chunk) => {
        buf = Buffer.concat([buf, chunk]);

        while (buf.length >= FRAME_TOTAL) {
            const magicIdx = buf.indexOf(MAGIC);
            if (magicIdx === -1) { buf = Buffer.alloc(0); break; }
            if (magicIdx > 0)    { buf = buf.slice(magicIdx); }
            if (buf.length < FRAME_TOTAL) break;

            const frameNum  = buf.readUInt32LE(4);
            const apuPort0  = buf[8];
            const apuPort1  = buf[9];
            const apuPort2  = buf[10];
            const apuPort3  = buf[11];
            const rgba      = buf.slice(FRAME_HDR_SIZE, FRAME_TOTAL);
            buf = buf.slice(FRAME_TOTAL);

            g_frameNum  = frameNum;
            g_frameRGBA = rgba;
            g_apuPorts  = [apuPort0, apuPort1, apuPort2, apuPort3];
            g_frameCount++;

            // Broadcast: frame_num(4,LE) + apu_ports(4) + rgba(229376)
            const msg = Buffer.alloc(8 + RGBA_SIZE);
            msg.writeUInt32LE(frameNum, 0);
            msg[4] = apuPort0; msg[5] = apuPort1;
            msg[6] = apuPort2; msg[7] = apuPort3;
            rgba.copy(msg, 8);

            for (const ws of wss.clients) {
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(msg);
                }
            }
        }
    });

    // Notify all clients of ROM change
    const romMsg = JSON.stringify({ type: 'rom_changed', rom: path.basename(romPath) });
    for (const ws of wss.clients) {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(romMsg);
        }
    }
}

// ── Global state ──────────────────────────────────────────────────────────────
let g_frameNum   = 0;
let g_frameRGBA  = null;
let g_apuPorts   = [0, 0, 0, 0];
let g_frameCount = 0;

// ── HTTP server ───────────────────────────────────────────────────────────────
const server = http.createServer((req, res) => {
    const url = req.url.split('?')[0];

    if (url === '/' || url === '/index.html') {
        const html = fs.readFileSync(path.join(__dirname, 'index.html'));
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(html);

    } else if (url === '/status') {
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        });
        res.end(JSON.stringify({
            frameNum:   g_frameNum,
            frameCount: g_frameCount,
            apuPorts:   g_apuPorts,
            emuRunning: emuRunning,
            emuPID:     emu ? emu.pid : null,
            currentROM: path.basename(currentROM),
        }));

    } else if (url === '/roms') {
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        });
        const roms = scanROMs();
        res.end(JSON.stringify({ roms: roms.map(r => r.name), current: path.basename(currentROM) }));

    } else if (url === '/load' && req.method === 'POST') {
        let body = '';
        req.on('data', d => body += d);
        req.on('end', () => {
            try {
                const { rom } = JSON.parse(body);
                const romPath = path.join(ROOT, path.basename(rom)); // basename prevents path traversal
                if (!fs.existsSync(romPath)) {
                    res.writeHead(404, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'ROM not found' }));
                    return;
                }
                startEmulator(romPath);
                res.writeHead(200, {
                    'Content-Type': 'application/json',
                    'Access-Control-Allow-Origin': '*'
                });
                res.end(JSON.stringify({ ok: true, rom: path.basename(romPath) }));
            } catch(e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });

    } else if (url === '/reset' && req.method === 'POST') {
        startEmulator(currentROM);
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        });
        res.end(JSON.stringify({ ok: true }));

    } else if (url === '/screenshot') {
        if (!g_frameRGBA) {
            res.writeHead(503); res.end('No frame yet'); return;
        }
        res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'X-Frame-Width':  String(FRAME_W),
            'X-Frame-Height': String(FRAME_H),
            'X-Frame-Num':    String(g_frameNum),
            'Access-Control-Allow-Origin': '*'
        });
        res.end(g_frameRGBA);

    } else if (url === '/upload' && req.method === 'POST') {
        const ct = req.headers['content-type'] || '';
        const boundaryMatch = ct.match(/boundary=(.+)/);
        if (!boundaryMatch) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Missing boundary' }));
            return;
        }
        const boundary = Buffer.from('--' + boundaryMatch[1]);
        const chunks = [];
        req.on('data', d => chunks.push(d));
        req.on('end', () => {
            try {
                const body = Buffer.concat(chunks);
                // Find Content-Disposition header to extract filename
                const headerEnd = body.indexOf('\r\n\r\n');
                if (headerEnd === -1) throw new Error('Invalid multipart body');
                const header = body.slice(0, headerEnd).toString();
                const fnMatch = header.match(/filename="([^"]+)"/);
                if (!fnMatch) throw new Error('No filename in upload');
                const originalName = path.basename(fnMatch[1]);
                if (!originalName.match(/\.(sfc|smc)$/i)) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Only .sfc/.smc files allowed' }));
                    return;
                }
                // Extract file data (after \r\n\r\n, before trailing boundary)
                const fileStart = headerEnd + 4;
                const trailBoundary = Buffer.from('\r\n--' + boundaryMatch[1]);
                const trailIdx = body.indexOf(trailBoundary, fileStart);
                const fileData = trailIdx !== -1 ? body.slice(fileStart, trailIdx) : body.slice(fileStart);
                const savePath = path.join(ROOT, originalName);
                fs.writeFileSync(savePath, fileData);
                console.log(`[SERVER] Uploaded ROM: ${originalName} (${fileData.length} bytes)`);
                startEmulator(savePath);
                res.writeHead(200, {
                    'Content-Type': 'application/json',
                    'Access-Control-Allow-Origin': '*'
                });
                res.end(JSON.stringify({ ok: true, rom: originalName }));
            } catch(e) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: e.message }));
            }
        });

    } else {
        res.writeHead(404); res.end('Not found');
    }
});

// ── WebSocket server ──────────────────────────────────────────────────────────
const wss = new WebSocketServer({ server });

wss.on('connection', (ws, req) => {
    console.log(`[WS] Client connected from ${req.socket.remoteAddress}`);

    // Send current ROM info
    ws.send(JSON.stringify({ type: 'rom_changed', rom: path.basename(currentROM) }));

    // Send latest frame immediately
    if (g_frameRGBA) {
        const msg = Buffer.alloc(8 + RGBA_SIZE);
        msg.writeUInt32LE(g_frameNum, 0);
        msg[4] = g_apuPorts[0]; msg[5] = g_apuPorts[1];
        msg[6] = g_apuPorts[2]; msg[7] = g_apuPorts[3];
        g_frameRGBA.copy(msg, 8);
        ws.send(msg);
    }

    ws.on('message', (data) => {
        if (typeof data === 'string' || data instanceof Buffer) {
            const cmd = data.toString().trim();
            // BTN commands forwarded to emulator stdin
            if (cmd.startsWith('BTN ') && emu && emuRunning) {
                emu.stdin.write(cmd + '\n');
            }
        }
    });

    ws.on('close', () => console.log('[WS] Client disconnected'));
    ws.on('error', (err) => console.error('[WS] Client error:', err.message));
});

// ── Start ─────────────────────────────────────────────────────────────────────
startEmulator(ROM_PATH);

server.listen(PORT, () => {
    console.log(`[SERVER] Listening at http://localhost:${PORT}`);
    console.log(`[SERVER] WebSocket  at ws://localhost:${PORT}`);
    console.log('[SERVER] Press Ctrl+C to stop');
});

process.on('SIGINT', () => {
    console.log('\n[SERVER] Shutting down...');
    if (emu) emu.kill();
    server.close();
    process.exit(0);
});
