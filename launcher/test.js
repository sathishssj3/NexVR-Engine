const cp = require('child_process');
const out = cp.execSync('reg query "HKLM\\SOFTWARE\\Khronos\\OpenXR\\1" /v "ActiveRuntime"', { encoding: 'utf-8' });
console.log("OUT:", JSON.stringify(out));
const match = out.match(/ActiveRuntime\s+REG_(?:EXPAND_)?SZ\s+(.+)/i);
console.log("MATCH:", match);
