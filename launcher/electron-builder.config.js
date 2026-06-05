module.exports = {
  appId:       'dev.nexvr.engine',
  productName: 'NexVR Engine',
  copyright:   'Copyright © 2026 NexVR',
  asar:        false,

  directories: {
    output: 'dist-electron',
    buildResources: 'assets'
  },

  files: [
    'frontend-dist/**/*',
    'electron-dist/**/*',
    'package.json'
  ],
  asarUnpack: [
    'frontend-dist/**/*'
  ],

  win: {
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
      from: '../build/vrinject.dll',
      to:   'vrinject.dll'
    },
    {
      from: '../build/vr-inject-cli.exe',
      to:   'vr-inject-cli.exe'
    },
  ],

  publish: null,  // No auto-update for v1.0 — add in v1.1
}
