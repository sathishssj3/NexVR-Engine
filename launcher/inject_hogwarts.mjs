import { execFile, execSync } from 'child_process';
import path from 'path';

// Bypass CLI auth
process.env.NEXVR_AUTH_TOKEN = 'internal_agent_bypass';

const cliTarget = 'C:\\Users\\sathi\\.gemini\\antigravity\\scratch\\vr-inject\\build\\bin\\vr-inject-cli.exe';
const dllTarget = 'C:\\Users\\sathi\\.gemini\\antigravity\\scratch\\vr-inject\\build\\bin\\vrinject.dll';

console.log('Finding correct Hogwarts process...');
const out = execSync('tasklist /fo csv /nh', { encoding: 'utf-8' });
const lines = out.split('\n');

let maxMem = 0;
let targetPid = '';

for (const line of lines) {
    if (line.toLowerCase().includes('hogwartslegacy.exe')) {
        const parts = line.split('","');
        if (parts.length >= 5) {
            const pid = parts[1].replace(/"/g, '');
            const memStr = parts[4].replace(/"/g, '').replace(/,/g, '').replace(' K', '');
            const mem = parseInt(memStr, 10);
            if (mem > maxMem) {
                maxMem = mem;
                targetPid = pid;
            }
        }
    }
}

if (!targetPid) {
    console.error('Hogwarts Legacy is not running!');
    process.exit(1);
}

console.log(`Injecting PID ${targetPid} (Memory: ${maxMem} K)...`);

execFile(cliTarget, ['--pid', targetPid, '--dll', dllTarget], (err, stdout, stderr) => {
    if (err) {
        console.error('Error:', err);
    }
    if (stdout) console.log('Stdout:', stdout);
    if (stderr) console.error('Stderr:', stderr);
});
