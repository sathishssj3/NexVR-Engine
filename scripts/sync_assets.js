// ==============================================================================
// sync_assets.js - Single-Source-of-Truth Asset Synchronization Pipeline
// Synchronizes shaders and profiles across dev, release, OTA staging, and docs
// Recomputes SHA-256 cryptographic hashes for updates/manifest.json automatically
// ==============================================================================

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const rootDir = path.resolve(__dirname, '..');
const clientDir = path.join(rootDir, 'nexvr-client');
const launcherPkgFile = path.join(clientDir, 'launcher', 'package.json');
const manifestFile = path.join(rootDir, 'updates', 'manifest.json');
const docsManifestFile = path.join(rootDir, 'nexvr-docs', 'landing-page', 'public', 'updates', 'manifest.json');

console.log('=== NexVR Engine Asset Synchronization Pipeline ===');

// 1. Read current version from package.json
let currentVersion = '0.1.58';
try {
  if (fs.existsSync(launcherPkgFile)) {
    const pkg = JSON.parse(fs.readFileSync(launcherPkgFile, 'utf-8'));
    if (pkg.version) currentVersion = pkg.version;
  }
} catch (e) {
  console.warn('Could not read launcher package.json version:', e.message);
}
console.log(`[+] Current Engine Version: v${currentVersion}`);

// 1.5 Synchronize native build binaries if present
const binDir = path.join(rootDir, 'build', 'bin');
const updatesDir = path.join(rootDir, 'updates');
const docsUpdatesDir = path.join(rootDir, 'nexvr-docs', 'landing-page', 'public', 'updates');
if (fs.existsSync(binDir)) {
  for (const binName of ['vrinject.dll', 'vr-inject-cli.exe']) {
    const srcBin = path.join(binDir, binName);
    const dstBin = path.join(updatesDir, binName);
    if (fs.existsSync(srcBin)) {
      fs.copyFileSync(srcBin, dstBin);
      console.log(`[+] Synced binary ${binName} -> updates/${binName}`);
      if (fs.existsSync(path.dirname(docsUpdatesDir))) {
        if (!fs.existsSync(docsUpdatesDir)) fs.mkdirSync(docsUpdatesDir, { recursive: true });
        fs.copyFileSync(srcBin, path.join(docsUpdatesDir, binName));
        console.log(`[+] Synced binary ${binName} -> docs/updates/${binName}`);
      }
    }
  }
}

// 2. Target Directories for Shaders
const srcShadersDir = path.join(clientDir, 'shaders');
const shaderDestinations = [
  path.join(rootDir, 'updates', 'shaders'),
  path.join(rootDir, 'build', 'bin', 'shaders'),
  path.join(rootDir, 'build', 'bin', 'vrinject_shaders'),
  path.join(rootDir, 'nexvr-docs', 'landing-page', 'public', 'updates', 'shaders'),
];

if (fs.existsSync(srcShadersDir)) {
  const shaderFiles = fs.readdirSync(srcShadersDir).filter(f => f.endsWith('.hlsl'));
  for (const destDir of shaderDestinations) {
    if (fs.existsSync(path.dirname(destDir))) {
      if (!fs.existsSync(destDir)) fs.mkdirSync(destDir, { recursive: true });
      for (const sf of shaderFiles) {
        fs.copyFileSync(path.join(srcShadersDir, sf), path.join(destDir, sf));
      }
      console.log(`[+] Synced ${shaderFiles.length} shaders -> ${path.relative(rootDir, destDir)}`);
    }
  }
}

// 3. Target Directories for Profiles
const srcProfilesDir = path.join(clientDir, 'profiles');
const profileDestinations = [
  path.join(rootDir, 'updates', 'profiles'),
  path.join(rootDir, 'nexvr-docs', 'landing-page', 'public', 'updates', 'profiles'),
];

if (fs.existsSync(srcProfilesDir)) {
  const profileFiles = fs.readdirSync(srcProfilesDir).filter(f => f.endsWith('.json'));
  for (const destDir of profileDestinations) {
    if (fs.existsSync(path.dirname(destDir))) {
      if (!fs.existsSync(destDir)) fs.mkdirSync(destDir, { recursive: true });
      for (const pf of profileFiles) {
        fs.copyFileSync(path.join(srcProfilesDir, pf), path.join(destDir, pf));
      }
      console.log(`[+] Synced ${profileFiles.length} profiles -> ${path.relative(rootDir, destDir)}`);
    }
  }
}

// 4. Recompute SHA-256 hashes and update manifest.json
if (fs.existsSync(manifestFile)) {
  try {
    const manifest = JSON.parse(fs.readFileSync(manifestFile, 'utf-8'));
    manifest.engineVersion = currentVersion;
    manifest.timestamp = Date.now();
    manifest.buildDate = new Date().toISOString().split('T')[0];

    const updatesDir = path.join(rootDir, 'updates');
    const hashes = {};

    if (Array.isArray(manifest.files)) {
      for (const relFile of manifest.files) {
        const fullPath = path.join(updatesDir, relFile);
        if (fs.existsSync(fullPath) && fs.statSync(fullPath).isFile()) {
          const buf = fs.readFileSync(fullPath);
          hashes[relFile] = crypto.createHash('sha256').update(buf).digest('hex');
        } else if (manifest.hashes && manifest.hashes[relFile]) {
          hashes[relFile] = manifest.hashes[relFile];
        }
      }
      manifest.hashes = hashes;
    }

    const jsonStr = JSON.stringify(manifest, null, 2);
    fs.writeFileSync(manifestFile, jsonStr, 'utf-8');
    console.log(`[+] Recomputed hashes and updated: ${path.relative(rootDir, manifestFile)}`);

    if (fs.existsSync(path.dirname(docsManifestFile))) {
      fs.writeFileSync(docsManifestFile, jsonStr, 'utf-8');
      console.log(`[+] Synced manifest -> ${path.relative(rootDir, docsManifestFile)}`);
    }
  } catch (e) {
    console.error('Error updating manifest:', e);
  }
}

console.log('=== Asset Synchronization Complete! ===');
