module.exports = {
  appId:       'dev.antigravity.launcher',
  productName: 'Antigravity',
  directories: { output: 'dist-electron' },
  files: [
    'dist/**/*',
    'electron/**/*'
  ],
  win: {
    target:                   'nsis',
    icon:                     'assets/icon.ico',
    requestedExecutionLevel:  'requireAdministrator'
  },
  nsis: {
    oneClick:                          false,
    allowToChangeInstallationDirectory: true,
    installerIcon:                     'assets/icon.ico',
    uninstallerIcon:                   'assets/icon.ico'
  },
  extraResources: [
    {
      from: '../../build/vrinject.dll',
      to:   'vrinject.dll'
    },
    {
      from: '../../build/vr-inject-cli.exe',
      to:   'vr-inject-cli.exe'
    }
  ]
}
