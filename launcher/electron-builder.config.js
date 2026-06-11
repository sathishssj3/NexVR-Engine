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
    'package.json'
  ],


  win: {
    icon: 'assets/icon.ico',
    target: [{ target: 'nsis', arch: ['x64'] }],
  },

  nsis: {
    oneClick:                           false,
    allowToChangeInstallationDirectory: true,
    installerIcon:   'assets/icon.ico',
    uninstallerIcon: 'assets/icon.ico',
    installerHeaderIcon: 'assets/icon.ico',
    createDesktopShortcut: true,
    createStartMenuShortcut: true,
    shortcutName: 'NexVR Engine',
    license: 'assets/LICENSE.txt',
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
      from: '../build/bin/openxr_loader.dll',
      to:   'openxr_loader.dll'
    },
    {
      from: '../build/bin/shaders',
      to:   'shaders'
    },
    {
      from: '../build/bin/models',
      to:   'models'
    },
  ],

  publish: null,  // No auto-update for v1.0 — add in v1.1
}
