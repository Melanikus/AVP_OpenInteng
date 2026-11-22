<#
vt-programs.ps1: which files of the release package go to VirusTotal on their own. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng,
the owner's ruling for alpha.5 through Bishop 2: "can we not scan the txts on virus total... just the binaries?", which is his written rule: a
release ships only at ZERO findings on the zip AND on every program inside it. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

WHAT A PROGRAM IS: a file whose first two bytes are "MZ" (0x4D 0x5A), how every Windows program starts (an .exe, a .dll, any other PE), read from
the file itself and never from its name (AvP Tao 3: a program renamed readme.txt or settings.ini is still a program, and a text named .dll is not).
Each program is sent alone; every other file (a licence, a note, the settings, the sums) is covered by the zip's own scan only.

Dot-source it, then Get-VtSendPlan <the unpacked package's folder> gives one entry per file, in a fixed order (by its full name): Path (the file),
Shown (its place in the package, / between folders, only letters, digits and . _ - / kept, every other character as _, so nothing in a name can
start a line of its own or move the cursor), and Alone (true for a program). Files whose names start with a dot are included (-Force: Linux would
leave them out). Its refusals are shown by tools/vt-programs-test.ps1. It works in Windows PowerShell and PowerShell 7.
#>

# True when the file's first two bytes are "MZ". Only those two bytes are read: the file is from outside this script, and nothing else of it is used.
function Test-VtProgram([string]$path)
{
    $stream = [System.IO.File]::OpenRead($path)
    try
    {
        $head = New-Object byte[] 2
        $read = $stream.Read($head, 0, 2)
        return ($read -eq 2 -and $head[0] -eq 0x4D -and $head[1] -eq 0x5A)
    }
    finally
    {
        $stream.Dispose()
    }
}

function Get-VtSendPlan([string]$inside)
{
    $root = (Resolve-Path -LiteralPath $inside).ProviderPath.TrimEnd('\', '/')
    $plan = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $root -Recurse -File -Force | Sort-Object FullName))
    {
        if (-not $file.FullName.StartsWith($root, [System.StringComparison]::Ordinal)) { throw "$($file.FullName) is not inside $root" }
        $shown = ($file.FullName.Substring($root.Length + 1) -replace '\\', '/') -replace '[^A-Za-z0-9._/-]', '_'
        $plan += [pscustomobject]@{ Path = $file.FullName; Shown = $shown; Alone = (Test-VtProgram $file.FullName) }
    }
    return ,$plan
}
