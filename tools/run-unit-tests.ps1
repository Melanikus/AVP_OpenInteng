<#
Builds and runs every unit test in tests/, each from its tests/<name>.args the way the repository says (cl @tests\<name>.args from the
repository's root, 32-bit, the same as the editor). GitHub runs this (.github/workflows/build.yml), and it works the same on any machine
with Visual Studio's C++ tools, in Windows PowerShell or PowerShell 7.

A test passes when it builds, its program gives back 0 within its time, and it prints no FAILED line. A test that says it was given
nothing to test fails.

A test whose tests/<name>.runargs asks for the game's own levels or models ({LEVELS}, {MODELS}, {LEVEL:Name}) needs Rebellion's files,
which a public machine must never hold. It is not run here and never counted as passed: it is listed by name, as one that runs on a
machine where the game is installed (the team runs every one of them there before each hand-over).

Usage: tools/run-unit-tests.ps1 [-OutFolder <folder for the objects and programs>] [-Only <test name>]
Written 2026-11-17 by Melanikus for AVP_OpenInteng.
#>
param(
    [string]$OutFolder = (Join-Path ([IO.Path]::GetTempPath()) 'avp-openinteng-tests'),
    [string]$Only = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = Split-Path -Parent $PSScriptRoot

# The compiler isn't on the path until Visual Studio has been set up, so find Visual Studio. Its own PowerShell module sets the compiler up
# in this window (nothing is written to disk to run it). vswhere goes on the path first, because the setup looks for it there.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=amd64 -no_logo' | Out-Null
Set-Location -LiteralPath $repository

New-Item -ItemType Directory -Force $OutFolder | Out-Null
$noInput = Join-Path $OutFolder 'no-input.txt'
Set-Content -LiteralPath $noInput -Value '' -NoNewline

$passed = New-Object System.Collections.Generic.List[string]
$failed = New-Object System.Collections.Generic.List[string]
$needsGame = New-Object System.Collections.Generic.List[string]
$tests = @(Get-ChildItem -LiteralPath (Join-Path $repository 'tests') -Filter '*.args' | Where-Object { -not $Only -or $_.BaseName -ceq $Only } | Sort-Object Name)
if ($tests.Count -eq 0) { throw "No test found in tests/ (named '$Only')" }

foreach ($test in $tests) {
    $name = $test.BaseName
    if ($name -cnotmatch '^[A-Za-z0-9_]+$') { $failed.Add("$name (a test's name is letters, digits and _ only)"); continue }

    # The time a test gets, and whether it needs the game's own files: its tests/<name>.runargs says
    $seconds = 120
    $runargs = Join-Path $repository "tests\$name.runargs"
    if (Test-Path -LiteralPath $runargs) {
        $lines = @(Get-Content -LiteralPath $runargs)
        $words = @($lines | Where-Object { $_ -notmatch '^#' } | Select-Object -First 1)
        if ($words.Count -gt 0 -and "$($words[0])" -match '\{LEVELS\}|\{MODELS\}|\{LEVEL:') { $needsGame.Add($name); continue }
        # Any other argument isn't known here, and a test run without what it asks for would test nothing, so it fails
        if ($words.Count -gt 0 -and "$($words[0])".Trim()) { $failed.Add("$name (its tests/$name.runargs asks for arguments this runner doesn't know)"); continue }
        $timeout = @($lines | Where-Object { $_ -match '^# timeout ([0-9]+)\s*$' } | Select-Object -First 1)
        if ($timeout.Count -gt 0) { $seconds = [int]($timeout[0] -replace '^# timeout ([0-9]+)\s*$', '$1') }
    }

    $folder = Join-Path $OutFolder $name
    New-Item -ItemType Directory -Force $folder | Out-Null
    $program = Join-Path $folder "$name.exe"
    if (Test-Path -LiteralPath $program) { Remove-Item -LiteralPath $program -Force }

    # The objects go to the test's own folder. A forward slash ends the folder's name, so no backslash sits before a closing quote
    # (which Windows would read as a quote).
    $buildLog = Join-Path $folder 'build.txt'
    & cl /nologo "@tests\$name.args" "/Fe:$program" "/Fo:$folder/" > $buildLog 2>&1
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $program)) {
        $failed.Add("$name (it didn't build)")
        "BUILD FAILED  $name"
        Get-Content -LiteralPath $buildLog | Select-String -Pattern 'error' | Select-Object -First 8 | ForEach-Object { "    $($_.Line)" }
        continue
    }

    $says = Join-Path $folder 'run.txt'
    $saysAsError = Join-Path $folder 'run-errors.txt'
    $process = Start-Process -FilePath $program -WorkingDirectory $repository -NoNewWindow -PassThru `
        -RedirectStandardInput $noInput -RedirectStandardOutput $says -RedirectStandardError $saysAsError
    $null = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    if (-not $process.WaitForExit($seconds * 1000)) {
        & taskkill /T /F /PID $process.Id | Out-Null
        $failed.Add("$name (still running after $seconds seconds)")
        "FAILED        $name (still running after $seconds seconds)"
        continue
    }
    $process.WaitForExit()
    $code = $process.ExitCode
    $output = @(Get-Content -LiteralPath $says)
    $oks = @($output | Where-Object { $_ -match '^\s*ok\s' }).Count
    $bads = @($output | Where-Object { $_ -match '^\s*FAILED\s' })
    if (@($output | Where-Object { $_ -match 'nothing was tested|nothing was checked|nothing to run|nothing to check|^usage:|no level was named' }).Count -gt 0) {
        $failed.Add("$name (it tested nothing)")
        "FAILED        $name (it says it was given nothing to test)"
        continue
    }
    if ($code -eq 0 -and $bads.Count -eq 0) {
        $passed.Add($name)
        "passed        $name ($oks ok)"
    } else {
        $failed.Add("$name (exit $code, $($bads.Count) FAILED)")
        "FAILED        $name (exit $code, $($bads.Count) FAILED, $oks ok)"
        $bads | Select-Object -First 8 | ForEach-Object { "    $_" }
    }
}

''
"Run on a machine with the game's own files ($($needsGame.Count), not counted as passed): $($needsGame -join ', ')"
"$($passed.Count) passed, $($failed.Count) failed"
foreach ($line in $failed) { "::error::$line" }
if ($failed.Count -gt 0 -or $passed.Count -eq 0) { exit 1 }
exit 0
