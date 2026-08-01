param($DllPath, $OutPath)
try {
    $stream = [System.IO.File]::OpenRead($DllPath)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $hashBytes = $sha256.ComputeHash($stream)
    $stream.Close()
    $hashString = [System.BitConverter]::ToString($hashBytes).Replace('-', '')
    Set-Content -Path $OutPath -Value "constexpr wchar_t EXPECTED_DLL_HASH[] = L`"$hashString`";"
    exit 0
} catch {
    exit 0
}
