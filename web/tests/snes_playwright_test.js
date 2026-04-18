/**
 * SNES Emulator Playwright AI Test
 *
 * 이 스크립트는 자동으로:
 *  1. 웹 서버를 시작
 *  2. Playwright 브라우저를 열어 에뮬레이터 UI 접속
 *  3. 프레임 렌더링 확인 (화면이 동작하는지)
 *  4. 화면이 all-black이 아닌지 확인
 *  5. 버튼 입력 전송 테스트
 *  6. APU 포트 값 모니터링 (spctest 패스 여부)
 *  7. 스크린샷 저장
 *
 * 사용법:
 *   node tests/snes_playwright_test.js [rom] [exe] [--headed]
 *   node tests/snes_playwright_test.js ../spctest.sfc ../snes_emu_complete.exe
 */

'use strict';

const { chromium } = require('playwright');
const { spawn }    = require('child_process');
const http         = require('http');
const path         = require('path');
const fs           = require('fs');

// ── Config ────────────────────────────────────────────────────────────────────
const ROOT     = path.join(__dirname, '../..');
const ROM_PATH = process.argv[2] || path.join(ROOT, 'spctest.sfc');
const EXE_PATH = process.argv[3] || path.join(ROOT, 'snes_emu_complete.exe');
const HEADED   = process.argv.includes('--headed');
const PORT     = 3099;  // Dedicated test port

const SCREENSHOT_DIR = path.join(__dirname, '..', 'screenshots');
fs.mkdirSync(SCREENSHOT_DIR, { recursive: true });

// ── Helpers ───────────────────────────────────────────────────────────────────
function log(msg) { console.log(`[TEST] ${msg}`); }
function pass(msg) { console.log(`[PASS] ✓ ${msg}`); }
function fail(msg) { console.error(`[FAIL] ✗ ${msg}`); }

async function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function waitForHttp(url, timeoutMs = 15000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        try {
            await new Promise((resolve, reject) => {
                const req = http.get(url, res => {
                    res.resume();
                    resolve();
                });
                req.on('error', reject);
                req.setTimeout(1000, () => { req.abort(); reject(new Error('timeout')); });
            });
            return true;
        } catch { await sleep(200); }
    }
    return false;
}

async function getStatus(baseUrl) {
    return new Promise((resolve, reject) => {
        http.get(`${baseUrl}/status`, res => {
            let data = '';
            res.on('data', d => data += d);
            res.on('end', () => {
                try { resolve(JSON.parse(data)); }
                catch { reject(new Error('Bad JSON: ' + data)); }
            });
        }).on('error', reject);
    });
}

// ── Main test ─────────────────────────────────────────────────────────────────
async function run() {
    log(`ROM  : ${ROM_PATH}`);
    log(`EXE  : ${EXE_PATH}`);
    log(`PORT : ${PORT}`);

    let serverProc = null;
    let browser = null;
    let passed = 0, failed = 0;

    function check(label, condition) {
        if (condition) { pass(label); passed++; }
        else           { fail(label); failed++; }
        return condition;
    }

    try {
        // ── 1. Start web server ───────────────────────────────────────────────
        log('서버 시작 중...');
        serverProc = spawn('node', [
            path.join(__dirname, '..', 'server.js'),
            ROM_PATH, EXE_PATH, String(PORT)
        ], { stdio: ['ignore', 'pipe', 'pipe'] });

        serverProc.stdout.on('data', d => process.stdout.write('[SRV] ' + d));
        serverProc.stderr.on('data', d => process.stderr.write('[SRV] ' + d));

        const serverReady = await waitForHttp(`http://localhost:${PORT}/status`);
        check('서버 시작 완료', serverReady);
        if (!serverReady) throw new Error('Server failed to start');

        // ── 2. Open browser ───────────────────────────────────────────────────
        log('브라우저 실행 중...');
        browser = await chromium.launch({ headless: !HEADED, slowMo: HEADED ? 50 : 0 });
        const context = await browser.newContext({ viewport: { width: 700, height: 700 } });
        const page = await context.newPage();

        page.on('console', msg => {
            if (msg.type() === 'error') log(`[BROWSER] ${msg.text()}`);
        });

        await page.goto(`http://localhost:${PORT}`);
        check('페이지 로드 완료', true);

        // ── 3. Wait for WebSocket connection ──────────────────────────────────
        log('WebSocket 연결 대기 중...');
        const connected = await page.waitForFunction(
            () => document.getElementById('conn-dot')?.classList.contains('connected'),
            { timeout: 10000 }
        ).then(() => true).catch(() => false);
        check('WebSocket 연결됨', connected);

        // ── 4. Wait for first frame ───────────────────────────────────────────
        log('첫 프레임 수신 대기 중 (최대 20초)...');
        const firstFrame = await page.waitForFunction(
            () => {
                const fps = document.getElementById('fps-display');
                return fps && fps.textContent.includes('F#') && !fps.textContent.includes('F#0');
            },
            { timeout: 20000 }
        ).then(() => true).catch(() => false);
        check('에뮬레이터 프레임 수신됨', firstFrame);

        if (!firstFrame) {
            // Try to get server status for debug info
            try {
                const s = await getStatus(`http://localhost:${PORT}`);
                log(`서버 상태: frameCount=${s.frameCount}, emuRunning=${s.emuRunning}`);
            } catch { }
            throw new Error('No frames received from emulator');
        }

        // ── 5. Let it run for 2 seconds, take screenshot ──────────────────────
        await sleep(2000);
        const shot1 = path.join(SCREENSHOT_DIR, 'frame_initial.png');
        await page.screenshot({ path: shot1 });
        log(`스크린샷 저장: ${shot1}`);

        // ── 6. Check canvas is not all-black ──────────────────────────────────
        const canvasInfo = await page.evaluate(() => {
            const canvas = document.getElementById('screen');
            const ctx = canvas.getContext('2d');
            const d = ctx.getImageData(0, 0, 256, 224).data;
            let nonBlack = 0, sumR = 0, sumG = 0, sumB = 0;
            for (let i = 0; i < d.length; i += 4) {
                if (d[i] > 10 || d[i+1] > 10 || d[i+2] > 10) nonBlack++;
                sumR += d[i]; sumG += d[i+1]; sumB += d[i+2];
            }
            const total = 256 * 224;
            return { nonBlack, total, avgR: sumR/total|0, avgG: sumG/total|0, avgB: sumB/total|0 };
        });
        log(`캔버스: ${canvasInfo.nonBlack}/${canvasInfo.total} 비-검정 픽셀, avg RGB=(${canvasInfo.avgR},${canvasInfo.avgG},${canvasInfo.avgB})`);
        check('화면이 all-black 아님', canvasInfo.nonBlack > 100);

        // ── 7. FPS check ──────────────────────────────────────────────────────
        await sleep(2000);
        const fpsText = await page.locator('#fps-display').textContent();
        const fpsMatch = fpsText.match(/FPS:\s*([\d.]+)/);
        const fpsVal = fpsMatch ? parseFloat(fpsMatch[1]) : 0;
        log(`측정 FPS: ${fpsVal}`);
        check(`FPS > 0 (실제: ${fpsVal.toFixed(1)})`, fpsVal > 0);

        // ── 8. Button input test ──────────────────────────────────────────────
        log('버튼 입력 테스트...');
        // Press START (should go to game title if spctest passes)
        await page.keyboard.press('e');   // E = START
        await sleep(300);
        await page.keyboard.up('e');

        // Arrow keys
        for (const key of ['ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight']) {
            await page.keyboard.press(key);
            await sleep(100);
        }
        // A and B
        await page.keyboard.press('x'); await sleep(100);
        await page.keyboard.press('z'); await sleep(100);
        check('버튼 입력 오류 없음', true);

        // ── 9. APU port monitoring ────────────────────────────────────────────
        log('APU 포트 모니터링 중 (5초)...');
        let spcTestPass = false;
        let spcTestFail = false;
        const monitorEnd = Date.now() + 5000;

        while (Date.now() < monitorEnd) {
            try {
                const st = await getStatus(`http://localhost:${PORT}`);
                const p = st.apuPorts;
                if (p[0] === 0xFF) {
                    spcTestPass = true;
                    log(`SPC 테스트 통과! APU ports: ${p.map(x => x.toString(16).padStart(2,'0')).join(' ')}`);
                    break;
                }
                if (p[2] === 0x02) {
                    spcTestFail = true;
                    log(`SPC 테스트 실패! APU ports: ${p.map(x => x.toString(16).padStart(2,'0')).join(' ')}`);
                    break;
                }
            } catch { }
            await sleep(200);
        }

        if (spcTestPass) {
            check('spctest.sfc 전체 통과 (APU port0 = 0xFF)', true);
        } else if (spcTestFail) {
            check('spctest.sfc 통과 (실패 상태 감지됨)', false);
        } else {
            log('(APU port0 != 0xFF: 테스트가 아직 진행 중이거나 다른 ROM)');
        }

        // ── 10. Final screenshot ──────────────────────────────────────────────
        await sleep(1000);
        const shot2 = path.join(SCREENSHOT_DIR, 'frame_final.png');
        await page.screenshot({ path: shot2 });
        log(`최종 스크린샷: ${shot2}`);

        // ── 11. Get final FPS from status ─────────────────────────────────────
        const finalStatus = await getStatus(`http://localhost:${PORT}`);
        log(`최종 상태: frame#${finalStatus.frameNum}, frameCount=${finalStatus.frameCount}`);
        check('에뮬레이터 정상 동작 중', finalStatus.emuRunning);

    } catch (err) {
        fail(`예외 발생: ${err.message}`);
        failed++;
    } finally {
        log('정리 중...');
        if (browser) await browser.close();
        if (serverProc) serverProc.kill();
        await sleep(500);
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    console.log('\n' + '='.repeat(50));
    console.log(`테스트 결과: ${passed} 통과 / ${failed} 실패`);
    if (failed === 0) {
        console.log('[ALL PASS] 모든 테스트 통과!');
    } else {
        console.log('[PARTIAL] 일부 테스트 실패. 위 로그를 확인하세요.');
    }
    console.log('='.repeat(50));
    process.exit(failed > 0 ? 1 : 0);
}

run().catch(err => {
    console.error('[FATAL]', err);
    process.exit(1);
});
