param(
    [string]$RunId = "33895614346"
)

$cred = @"
protocol=https
host=github.com
"@ | git credential fill

$token = ""
foreach ($l in ($cred -split "`n")) {
    if ($l -match '^password=(.*)$') {
        $token = $matches[1].Trim()
    }
}

$headers = @{
    "Authorization" = "Bearer $token"
    "User-Agent" = "PowerShell"
    "Accept" = "application/vnd.github.v3+json"
}

$artifacts = Invoke-RestMethod -Uri "https://api.github.com/repos/sathishssj3/NexVR-Engine/actions/runs/$RunId/artifacts" -Headers $headers
foreach ($cfg in @("Debug", "Release")) {
    $art = $artifacts.artifacts | Where-Object { $_.name -eq "test-results-$cfg" }
    if ($art) {
        Write-Host "Downloading $($art.name)..."
        Invoke-WebRequest -Uri $art.archive_download_url -Headers $headers -OutFile "test-results-$cfg.zip"
        Expand-Archive -Path "test-results-$cfg.zip" -DestinationPath "test-results-$cfg" -Force
        $xmlFiles = Get-ChildItem -Path "test-results-$cfg" -Recurse -Filter "*.xml"
        foreach ($xf in $xmlFiles) {
            $xml = [xml](Get-Content $xf.FullName)
            $failures = $xml.SelectNodes("//testcase[failure]")
            Write-Host "$($art.name) -> $($xf.Name): Total failures: $($failures.Count)"
            foreach ($f in $failures) {
                Write-Host "  FAILED: $($f.name)"
                Write-Host "    $($f.failure.InnerText)"
            }
        }
    }
}
