# Launch No Man's Sky with vrinject as an explicit Vulkan layer.
#
# Must run UNELEVATED. The Vulkan loader ignores path-type env vars (VK_LAYER_PATH) for
# elevated processes while still honouring name-type ones (VK_INSTANCE_LAYERS), which
# produces the misleading "layer was not found but was requested". NMS.exe asks for
# elevation in its manifest, so __COMPAT_LAYER=RunAsInvoker is what keeps us unelevated.

$ErrorActionPreference = 'Stop'

$bin      = "C:\Games\No Man's Sky\Binaries"
$exe      = Join-Path $bin "NMS.exe"
$logDir   = Join-Path $env:LOCALAPPDATA "VRInject"
$log      = Join-Path $logDir "vrinject.log"
$loaderLog = Join-Path $env:TEMP "vk_loader_debug.txt"

if (-not (Test-Path $exe)) { throw "NMS.exe not found at $exe" }

# Archive the previous log so the next read is unambiguously this run.
if (Test-Path $log) {
    $stamp = (Get-Item $log).LastWriteTime.ToString("yyyyMMdd-HHmmss")
    #Move-Item $log (Join-Path $logDir "vrinject-$stamp.log") -Force -ErrorAction SilentlyContinue
    Write-Host "Archived previous log as vrinject-$stamp.log"
}
if (Test-Path $loaderLog) { Remove-Item $loaderLog -Force }

$env:__COMPAT_LAYER     = "RunAsInvoker"
$env:VK_LAYER_PATH      = $bin
$env:VK_INSTANCE_LAYERS = "VK_LAYER_VRINJECT_StereoPipeline"
$env:VK_LOADER_DEBUG    = "layer,error,warn"

Write-Host "VK_LAYER_PATH      = $($env:VK_LAYER_PATH)"
Write-Host "VK_INSTANCE_LAYERS = $($env:VK_INSTANCE_LAYERS)"
Write-Host "Launching NMS.exe (unelevated)..."

$p = Start-Process -FilePath $exe -WorkingDirectory $bin -PassThru `
                   -RedirectStandardError $loaderLog

Write-Host "PID $($p.Id) started. Loader debug -> $loaderLog"
Write-Host "DLL log -> $log"
