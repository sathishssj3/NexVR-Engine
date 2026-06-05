module.exports = {
  appId:       'dev.antigravity.launcher',
  productName: 'Antigravity',
  copyright:   'Copyright © 2025 VOXIRA.VR',

  directories: {
    output: 'dist-electron',
    buildResources: 'assets'
  },

  files: [
    'dist/**/*',
    'electron-dist/**/*',
    '!**/*.map',
    '!**/*.ts',
  ],

  win: {
    target: [{ target: 'nsis', arch: ['x64'] }],
    // requestedExecutionLevel:  'requireAdministrator',
  },

  nsis: {
    oneClick:                           false,
    allowToChangeInstallationDirectory: true,
    installerIcon:   'assets/icon.ico',
    uninstallerIcon: 'assets/icon.ico',
    installerHeaderIcon: 'assets/icon.ico',
    createDesktopShortcut: true,
    createStartMenuShortcut: true,
    shortcutName: 'Antigravity VR',
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
