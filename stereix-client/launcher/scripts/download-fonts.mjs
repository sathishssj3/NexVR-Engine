import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const fontsDir = path.join(__dirname, '..', 'public', 'fonts');

if (!fs.existsSync(fontsDir)) {
  fs.mkdirSync(fontsDir, { recursive: true });
}

const fonts = [
  // Chakra Petch
  { name: 'chakra-petch-400.woff2', url: 'https://fonts.gstatic.com/s/chakrapetch/v13/cIf6MapbsEk7TDLdtEz1BwkWn6pg.woff2' },
  { name: 'chakra-petch-500.woff2', url: 'https://fonts.gstatic.com/s/chakrapetch/v13/cIflMapbsEk7TDLdtEz1BwkebIl1R5_F.woff2' },
  { name: 'chakra-petch-600.woff2', url: 'https://fonts.gstatic.com/s/chakrapetch/v13/cIflMapbsEk7TDLdtEz1BwkeQI51R5_F.woff2' },
  { name: 'chakra-petch-700.woff2', url: 'https://fonts.gstatic.com/s/chakrapetch/v13/cIflMapbsEk7TDLdtEz1BwkeJI91R5_F.woff2' },

  // Inter
  { name: 'inter-400.woff2', url: 'https://fonts.gstatic.com/s/inter/v20/UcC73FwrK3iLTeHuS_nVMrMxCp50SjIa1ZL7.woff2' },
  { name: 'inter-500.woff2', url: 'https://fonts.gstatic.com/s/inter/v20/UcC73FwrK3iLTeHuS_nVMrMxCp50SjIa1ZL7.woff2' },
  { name: 'inter-600.woff2', url: 'https://fonts.gstatic.com/s/inter/v20/UcC73FwrK3iLTeHuS_nVMrMxCp50SjIa1ZL7.woff2' },
  { name: 'inter-700.woff2', url: 'https://fonts.gstatic.com/s/inter/v20/UcC73FwrK3iLTeHuS_nVMrMxCp50SjIa1ZL7.woff2' },

  // JetBrains Mono
  { name: 'jetbrains-mono-400.woff2', url: 'https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbv2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8yKwBNntkaToggR7BYRbKPxDcwg.woff2' },
  { name: 'jetbrains-mono-500.woff2', url: 'https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbv2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8yKwBNntkaToggR7BYRbKPxDcwg.woff2' },
  { name: 'jetbrains-mono-600.woff2', url: 'https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbv2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8yKwBNntkaToggR7BYRbKPxDcwg.woff2' },
  { name: 'jetbrains-mono-700.woff2', url: 'https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbv2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8yKwBNntkaToggR7BYRbKPxDcwg.woff2' },
];

async function downloadAll() {
  console.log(`Downloading ${fonts.length} fonts into ${fontsDir}...`);
  for (const f of fonts) {
    const dest = path.join(fontsDir, f.name);
    if (fs.existsSync(dest) && fs.statSync(dest).size > 1000) {
      console.log(`  ✓ ${f.name} already exists`);
      continue;
    }
    const res = await fetch(f.url);
    if (!res.ok) {
      console.error(`Failed to download ${f.name}: ${res.status}`);
      continue;
    }
    const buf = Buffer.from(await res.arrayBuffer());
    fs.writeFileSync(dest, buf);
    console.log(`  ✓ Downloaded ${f.name} (${buf.length} bytes)`);
  }
  console.log('Done!');
}

downloadAll().catch(err => {
  console.error(err);
  process.exit(1);
});
