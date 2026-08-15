param(
    [Parameter(Mandatory = $true)]
    [string] $Executable
)

$nativeCode = @'
using System;
using System.Runtime.InteropServices;

public static class GearheadsWindowTest {
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    public static extern IntPtr GetWindowLongPtr(IntPtr window, int index);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
}
'@

Add-Type -TypeDefinition $nativeCode

$process = Start-Process -FilePath $Executable -PassThru
try {
    try { $null = $process.WaitForInputIdle(10000) } catch {}
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100; $attempt++) {
        $process.Refresh()
        $window = $process.MainWindowHandle
        if ($window -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 50
    }
    if ($window -eq [IntPtr]::Zero) { throw 'Gearheads did not create its main window' }

    $gwlStyle = -16
    $wmSysKeyDown = 0x0104
    $vkReturn = 0x0D
    $altContext = 0x20000000
    $overlappedWindow = 0x00CF0000

    $before = [GearheadsWindowTest]::GetWindowLongPtr($window, $gwlStyle).ToInt64()
    if (($before -band $overlappedWindow) -eq 0) {
        throw 'The initial window is not in windowed mode'
    }

    $null = [GearheadsWindowTest]::SendMessage(
        $window, $wmSysKeyDown, [IntPtr]$vkReturn, [IntPtr]$altContext
    )
    $fullscreen = [GearheadsWindowTest]::GetWindowLongPtr($window, $gwlStyle).ToInt64()
    if (($fullscreen -band $overlappedWindow) -ne 0) {
        throw 'Alt+Enter did not enter borderless fullscreen'
    }

    $null = [GearheadsWindowTest]::SendMessage(
        $window, $wmSysKeyDown, [IntPtr]$vkReturn, [IntPtr]$altContext
    )
    $restored = [GearheadsWindowTest]::GetWindowLongPtr($window, $gwlStyle).ToInt64()
    if (($restored -band $overlappedWindow) -ne ($before -band $overlappedWindow)) {
        throw 'Alt+Enter did not restore the windowed style'
    }

    Write-Output 'Alt+Enter entered fullscreen and restored windowed mode'
} finally {
    if (!$process.HasExited) {
        $null = $process.CloseMainWindow()
        if (!$process.WaitForExit(3000)) { $process.Kill() }
    }
    $process.Dispose()
}
