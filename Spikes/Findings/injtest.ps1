# Runs inside the child session (started via RunOnce). Tests Windows.UI.Input.Preview.Injection.InputInjector, unpackaged.
$ErrorActionPreference = 'Stop'
$outDir = 'C:\Users\Axodox\AppData\Local\Temp\claude\h--work-Windows-Simulator-16-0\b5eeca30-5c5e-4ed2-aa32-a3a09ea9bf1b\scratchpad\sscapture\out'
$logPath = Join-Path $outDir 'loop.log'
function Log($m) { Add-Content $logPath ("{0:HH:mm:ss.fff} {1}" -f (Get-Date), $m) }

# Session guard: do nothing unless this is the child session the host expects
$mySession = (Get-Process -Id $PID).SessionId
$expectFile = Join-Path $outDir 'expected_session.txt'
$expected = if (Test-Path $expectFile) { (Get-Content $expectFile -Raw).Trim() } else { 'missing' }
if ("$mySession" -ne $expected) { Log "GUARD: refusing to run in session $mySession (expected $expected)"; exit }
Log "START session=$mySession (InputInjector test, unpackaged PowerShell)"

try {
    Add-Type -Namespace W -Name U -MemberDefinition @'
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
[DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
[DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
public struct POINT { public int X, Y; }
'@
    [void][W.U]::SetProcessDPIAware()
    function ForegroundTitle { $sb = New-Object System.Text.StringBuilder 256; [void][W.U]::GetWindowText([W.U]::GetForegroundWindow(), $sb, 256); $sb.ToString() }

    $null = [Windows.UI.Input.Preview.Injection.InputInjector, Windows.UI.Input.Preview, ContentType = WindowsRuntime]
    $inj = [Windows.UI.Input.Preview.Injection.InputInjector]::TryCreate()
    if (-not $inj) { Log "InputInjector.TryCreate returned null"; Log "END"; exit }
    Log "InputInjector.TryCreate: OK"

    function Key([int]$vk, [bool]$up) {
        $k = New-Object Windows.UI.Input.Preview.Injection.InjectedInputKeyboardInfo
        $k.VirtualKey = $vk
        if ($up) { $k.KeyOptions = [Windows.UI.Input.Preview.Injection.InjectedInputKeyOptions]::KeyUp }
        $inj.InjectKeyboardInput([Windows.UI.Input.Preview.Injection.InjectedInputKeyboardInfo[]]@($k))
        Start-Sleep -Milliseconds 30
    }
    function TypeText([string]$text) {
        foreach ($ch in $text.ToCharArray()) {
            foreach ($up in $false, $true) {
                $k = New-Object Windows.UI.Input.Preview.Injection.InjectedInputKeyboardInfo
                $k.ScanCode = [uint16][char]$ch
                $opts = [Windows.UI.Input.Preview.Injection.InjectedInputKeyOptions]::Unicode
                if ($up) { $opts = $opts -bor [Windows.UI.Input.Preview.Injection.InjectedInputKeyOptions]::KeyUp }
                $k.KeyOptions = $opts
                $inj.InjectKeyboardInput([Windows.UI.Input.Preview.Injection.InjectedInputKeyboardInfo[]]@($k))
            }
            Start-Sleep -Milliseconds 20
        }
    }

    # Notepad with a scratch file
    $file = Join-Path $outDir 'injtest.txt'
    Set-Content $file '' -NoNewline
    Start-Process notepad.exe -ArgumentList "`"$file`""
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ((ForegroundTitle) -notmatch 'injtest' -and $sw.Elapsed.TotalSeconds -lt 15) { Start-Sleep -Milliseconds 250 }
    $title = ForegroundTitle
    Log "Foreground window: '$title'"
    if ($title -notmatch 'injtest') { Log "Notepad with the test file is not in the foreground; not typing"; Log "END"; exit }
    Start-Sleep -Milliseconds 500

    # Keyboard: Unicode text, then Ctrl+S
    $text = 'Hello world from InputInjector! ÁÉŐŰ'
    TypeText $text
    Start-Sleep -Milliseconds 300
    Key 0x11 $false; Key 0x53 $false; Key 0x53 $true; Key 0x11 $true   # Ctrl+S
    Start-Sleep -Seconds 2
    $saved = Get-Content $file -Raw -Encoding UTF8
    Log ("Keyboard: typed '{0}', file contains '{1}' => {2}" -f $text, $saved, $(if ($saved -eq $text) { 'MATCH' } else { 'DIFFERENT' }))

    # Mouse: absolute move
    $w = [W.U]::GetSystemMetrics(0); $h = [W.U]::GetSystemMetrics(1)
    $tx = 300; $ty = 250
    $m = New-Object Windows.UI.Input.Preview.Injection.InjectedInputMouseInfo
    $m.DeltaX = [int]($tx * 65535 / ($w - 1)); $m.DeltaY = [int]($ty * 65535 / ($h - 1))
    $m.MouseOptions = [Windows.UI.Input.Preview.Injection.InjectedInputMouseOptions]::Move -bor [Windows.UI.Input.Preview.Injection.InjectedInputMouseOptions]::Absolute
    $inj.InjectMouseInput([Windows.UI.Input.Preview.Injection.InjectedInputMouseInfo[]]@($m))
    Start-Sleep -Milliseconds 200
    $p = New-Object W.U+POINT; [void][W.U]::GetCursorPos([ref]$p)
    Log ("Mouse: target ({0},{1}), cursor ({2},{3}) => {4}" -f $tx, $ty, $p.X, $p.Y, $(if ([math]::Abs($p.X - $tx) -le 1 -and [math]::Abs($p.Y - $ty) -le 1) { 'OK' } else { 'MISMATCH' }))

    # Close the Notepad tab so it doesn't linger in Notepad's saved session state
    if ((ForegroundTitle) -match 'injtest') { Key 0x11 $false; Key 0x57 $false; Key 0x57 $true; Key 0x11 $true }   # Ctrl+W
    Start-Sleep -Seconds 1
    Log "After Ctrl+W, foreground: '$(ForegroundTitle)'"
}
catch { Log ("ERROR " + $_.Exception.GetType().Name + ": " + $_.Exception.Message) }
Log "END"
