module.exports = {
  appId:       'dev.nexvr.engine',
  productName: 'Stereix Engine',
  copyright:   'Copyright © 2026 Mesmeran Lab',
  asar:        true,

  directories: {
    output: 'dist-electron',
    buildResources: 'assets'
  },

  files: [
    'frontend-dist/**/*',
    'electron-dist/**/*',
    'assets/icon.ico',
    'package.json'
  ],

  win: {
    icon: 'assets/icon.ico',
    signtoolOptions: {
      publisherName: 'Mesmeran Lab'
    },
    target: [
      { target: 'nsis', arch: ['x64'] },
      { target: 'portable', arch: ['x64'] }
    ],
    cscLink: process.env.SIGN_CERT_PATH || undefined,
    cscKeyPassword: process.env.SIGN_CERT_PASS || undefined,
  },

  nsis: {
    oneClick:                           true,
    perMachine:                         false,
    allowToChangeInstallationDirectory: false,
    installerIcon:   'assets/icon.ico',
    uninstallerIcon: 'assets/icon.ico',
    installerHeaderIcon: 'assets/icon.ico',
    createDesktopShortcut: true,
    createStartMenuShortcut: true,
    shortcutName: 'Stereix Engine',
    artifactName: 'Stereix-Engine-Setup-${version}.${ext}',
  },

  portable: {
    artifactName: 'Stereix-Engine-Portable-${version}.${ext}',
  },

  extraResources: [
    {
      from: '../build/bin/vrinject.dll',
      to:   'vrinject.dll'
    },
    {
      from: '../build/bin/vr-inject-cli.exe',
      to:   'vr-inject-cli.exe'
    },
    {
      from: '../build/bin/onnxruntime.dll',
      to:   'onnxruntime.dll'
    },
    {
      from: '../build/bin/DirectML.dll',
      to:   'DirectML.dll'
    },
    {
      from: '../build/bin/shaders',
      to:   'shaders'
    },
    {
      from: '../models',
      to:   'models'
    },
    {
      from: '../profiles',
      to:   'profiles'
    },
  ],

  publish: {
    provider: 'github',
    owner: 'sathishssj3',
    repo: 'NexVR-Engine',
    releaseType: 'release',
  },
};
