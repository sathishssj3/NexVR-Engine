module.exports = {
  appId:       'dev.nexvr.engine',
  productName: 'NexVR Engine',
  copyright:   'Copyright © 2026 NexVR Engine',
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
    target: [
      { target: 'nsis', arch: ['x64'] },
      { target: 'portable', arch: ['x64'] },
      { target: 'zip', arch: ['x64'] }
    ],
    cscLink: process.env.SIGN_CERT_PATH,
    cscKeyPassword: process.env.SIGN_CERT_PASS,
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
    shortcutName: 'NexVR Engine',
  },

  portable: {
    artifactName: 'NexVR Engine Portable ${version}.${ext}',
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
    repo: 'NexVR-Engine-Releases',
    releaseType: 'release',
  },
};
