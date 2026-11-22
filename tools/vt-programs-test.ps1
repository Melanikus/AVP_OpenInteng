<#
vt-programs-test.ps1: the test of tools/vt-programs.ps1, which picks the release package's files that VirusTotal is sent on their own (the programs,
by their first two bytes "MZ"; every other file is covered by the zip's scan). Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng.
AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

It makes a package of its own in a new folder under the system's temporary folder, and deletes it after: a program named as a text, a text named
as a program, a real program's start, a file with "MZ" in the wrong case, a file of one byte, an empty file, one whose name starts with a dot, and
one in a folder. Run it from anywhere: powershell -NoProfile -File tools/vt-programs-test.ps1 (or pwsh). It answers 0 when every check passed.
#>
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'vt-programs.ps1')

$failed = 0
function Check([bool]$good, [string]$what)
{
    Write-Host ('  ' + $(if ($good) { 'ok' } else { 'FAILED' }) + '  ' + $what)
    if (-not $good) { $script:failed++ }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('vt-programs-test-' + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $work 'licences') -Force | Out-Null
try
{
    # A program's first bytes: "MZ" and the rest of a DOS header (the bytes after MZ don't matter to the pick)
    $pe = [byte[]](@(0x4D, 0x5A, 0x90, 0x00) + @(0) * 60)
    [System.IO.File]::WriteAllBytes((Join-Path $work 'readme.txt'), $pe)                              # a program named as a text
    [System.IO.File]::WriteAllBytes((Join-Path $work 'tool.exe'), $pe)                                # a program named as one
    [System.IO.File]::WriteAllBytes((Join-Path $work 'licences/inside.ini'), $pe)                     # a program in a folder, named as settings
    [System.IO.File]::WriteAllBytes((Join-Path $work '.hidden'), $pe)                                 # a program whose name starts with a dot
    [System.IO.File]::WriteAllText((Join-Path $work 'notes.txt'), "Plain words, nothing more.`n")    # a plain text
    [System.IO.File]::WriteAllText((Join-Path $work 'fake.dll'), "Not a program, only named as one`n") # a text named as a program
    [System.IO.File]::WriteAllText((Join-Path $work 'lower.bin'), "mz in the wrong case`n")          # "mz", not "MZ"
    [System.IO.File]::WriteAllBytes((Join-Path $work 'one.bin'), [byte[]]@(0x4D))                     # "M" alone: too short to be a program
    [System.IO.File]::WriteAllBytes((Join-Path $work 'empty.ini'), [byte[]]@())                       # nothing at all

    $plan = Get-VtSendPlan $work
    $alone = @{}
    foreach ($entry in $plan) { $alone[$entry.Shown] = $entry.Alone }

    Check ($plan.Count -eq 9) "every file is in the plan, the one whose name starts with a dot and the one in a folder too (it has $($plan.Count))"
    Check ($alone['readme.txt'] -eq $true) 'a program named readme.txt is sent alone (by its first bytes, never its name)'
    Check ($alone['tool.exe'] -eq $true) 'a program named tool.exe is sent alone'
    Check ($alone['licences/inside.ini'] -eq $true) 'a program in a folder, named inside.ini, is sent alone, and its place is shown with /'
    Check ($alone['.hidden'] -eq $true) 'a program whose name starts with a dot is sent alone'
    Check ($alone['notes.txt'] -eq $false) 'a plain .txt is NOT sent alone: the zip covers it'
    Check ($alone['fake.dll'] -eq $false) 'a text named fake.dll is NOT sent alone: it is no program'
    Check ($alone['lower.bin'] -eq $false) '"mz" in the wrong case is no program'
    Check ($alone['one.bin'] -eq $false) 'a file of one byte, "M", is no program'
    Check ($alone['empty.ini'] -eq $false) 'an empty file is no program'
    $again = Get-VtSendPlan $work
    Check ((@($plan | ForEach-Object { $_.Shown }) -join '|') -ceq (@($again | ForEach-Object { $_.Shown }) -join '|')) 'two plans of one folder list its files in the same order'
}
finally
{
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failed) { Write-Host "SOME CHECKS FAILED ($failed)"; exit 1 }
Write-Host 'All checks passed'
exit 0
