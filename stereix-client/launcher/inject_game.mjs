import { execFile, execSync } from 'child_process';
import path from 'path';

import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Bypass CLI auth
process.env.NEXVR_AUTH_TOKEN = 'internal_agent_bypass';

const cliTarget = path.join(__dirname, '..', 'build', 'bin', 'vr-inject-cli.exe');
const dllTarget = path.join(__dirname, '..', 'build', 'bin', 'vrinject.dll');

const targetProcess = process.argv[2];
if (!targetProcess) {
    console.error('Usage: node inject_game.mjs <process_name.exe>');
    process.exit(1);
}
const processNameLower = targetProcess.toLowerCase();

console.log(`Waiting for ${targetProcess} to start (polling every 2s)...`);

function findAndInject() {
    let out;
    try {
        out = execSync('tasklist /fo csv /nh', { encoding: 'utf-8' });
    } catch (e) {
        setTimeout(findAndInject, 2000);
        return;
    }

    const lines = out.split('\n');

    let maxMem = 0;
    let targetPid = '';

    for (const line of lines) {
        if (line.toLowerCase().includes(processNameLower)) {
            const parts = line.split('","');
            if (parts.length >= 5) {
                const pid = parts[1].replace(/"/g, '');
                // Strip all non-digit characters to handle different locales (spaces, commas, periods) and \r
                const memStr = parts[4].replace(/\D/g, '');
                const mem = parseInt(memStr, 10);
                if (mem > maxMem) {
                    maxMem = mem;
                    targetPid = pid;
                }
            }
        }
    }

    // Must be at least 15MB (15360 K) to match CLI logic and avoid launcher
    if (!targetPid || maxMem < 15000) {
        setTimeout(findAndInject, 2000);
        return;
    }

    console.log(`Found ${targetProcess}! Injecting PID ${targetPid} (Memory: ${maxMem} K)...`);

    execFile(cliTarget, ['--pid', targetPid, '--dll', dllTarget], (err, stdout, stderr) => {
        if (err) {
            console.error('Error:', err);
        }
        if (stdout) console.log('Stdout:', stdout);
        if (stderr) console.error('Stderr:', stderr);
    });
}

findAndInject();
