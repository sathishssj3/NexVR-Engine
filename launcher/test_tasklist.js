const cp = require('child_process'); 
const out = cp.execSync('tasklist /fo csv /nh', { encoding: 'utf-8' }); 
const lines = out.split('\n');
console.log("Raw line 0:", JSON.stringify(lines[0])); 
const parts = lines[0].split('","'); 
console.log("Parts 0:", parts);
