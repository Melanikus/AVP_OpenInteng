<#
sca-gate-test.ps1: the test of tools/sca-gate.ps1, the release's gate on AvPx's library scan of the pin. Written 2026-11-22 by Melanikus (Vasquez)
for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

It asks GitHub nothing. Here "gh" is a PowerShell function of this test's own, which PowerShell finds before the real program, so the gate's
questions get made-up answers: a good run, and each way a run can fail to prove the scan (none, too old, a failed newest one, a skipped job...),
each refused for its own reason, named. It also checks that release.yml asks the gate twice: before anything is built, and in the draft's job
before the draft is made. Run it from anywhere: powershell -NoProfile -File tools/sca-gate-test.ps1 (or pwsh). It answers 0 when every check
passed.
#>
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'sca-gate.ps1')

$failed = 0
function Check([bool]$good, [string]$what)
{
    Write-Host ('  ' + $(if ($good) { 'ok' } else { 'FAILED' }) + '  ' + $what)
    if (-not $good) { $script:failed++ }
}

$repository = 'Melanikus/AVP_OpenInteng'
$commit = '1234567890abcdef1234567890abcdef12345678'
$pin = '0de2af869a20100857978649fec7fabe28b2c015'
$otherSha = 'ffffffffffffffffffffffffffffffffffffffff'
$T = "`t"

# A time GitHub's way, so many hours ago (a negative number is in the future)
function Ago([double]$hours)
{
    return [DateTimeOffset]::UtcNow.AddHours(-$hours).ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [System.Globalization.CultureInfo]::InvariantCulture)
}

# One run as the gate's question gets it back: id, head_sha, event, status, conclusion, started, path, repository, page
function Run([string]$id, [double]$hoursAgo, [string]$conclusion = 'success', [string]$status = 'completed', [string]$sha = $pin, [string]$event = 'workflow_dispatch',
    [string]$path = '.github/workflows/snyk.yml', [string]$from = 'Melanikus/AvP', [string]$page = '')
{
    if (-not $page) { $page = "https://github.com/Melanikus/AvP/actions/runs/$id" }
    return (@($id, $sha, $event, $status, $conclusion, (Ago $hoursAgo), $path, $from, $page) -join $T)
}

function Job([string]$name, [string]$conclusion = 'success', [string]$status = 'completed') { return (@($name, $status, $conclusion) -join $T) }

# The jobs of a full scan, as AvPx's snyk.yml has them at the pin
$fullJobs = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)'), (Job 'Did the libraries change?'), (Job 'Build the vcpkg libraries'), (Job 'Snyk on the vcpkg libraries'))

# A made-up GitHub: each question the gate may ask, and its answer (lines; $null: GitHub won't answer). $script:Asked keeps each question asked
function World([object]$pinAnswer = @("submodule${T}${pin}${T}https://github.com/Melanikus/AvP.git"), [object]$runs = @((Run '111' 2)), [hashtable]$jobs = @{ '111' = $fullJobs })
{
    return @{ Pin = $pinAnswer; Runs = $runs; Jobs = $jobs }
}

# ONLY READS (Dietrich, 11-22: in the draft's job the gate holds a token that can write). A question is let through only in the one form a
# plain read has: gh api, one repos/... path, --paginate, and --jq with its filter. gh api turns to POST once it is given a field, and anything
# else (-X/--method, -f/-F/--field/--raw-field, --input, -H/--header, or a word this doesn't know) is refused and kept in $script:NotReads,
# which no run of the gate, in any case below, may ever add to
$script:NotReads = @()
function Test-OnlyReads([object[]]$words)
{
    if ($words.Count -lt 1 -or [string]$words[0] -cne 'api') { return $false }
    $pathsSeen = 0; $filters = 0; $i = 1

    while ($i -lt $words.Count)
    {
        $word = [string]$words[$i]
        if ($word -ceq '--paginate') { $i++ }
        elseif ($word -ceq '--jq' -and $i + 1 -lt $words.Count) { $filters++; $i += 2 }
        elseif ($word -clike 'repos/*' -and $pathsSeen -eq 0) { $pathsSeen++; $i++ }
        else { return $false }
    }

    return ($pathsSeen -eq 1 -and $filters -eq 1)
}

function gh
{
    $words = @($args)
    $paths = @($words | Where-Object { $_ -is [string] -and $_ -like 'repos/*' })
    $script:Asked += , ($words -join ' ')
    $answer = $null
    $known = $false

    if (-not (Test-OnlyReads $words))
    {
        $script:NotReads += , ($words -join ' ')
        $global:LASTEXITCODE = 1
        return
    }

    if ($words[0] -ceq 'api' -and $paths.Count -eq 1)
    {
        $path = $paths[0]

        if ($path -ceq "repos/$repository/contents/avpx?ref=$commit") { $answer = $script:World.Pin; $known = $true }
        elseif ($path -ceq "repos/Melanikus/AvP/actions/workflows/snyk.yml/runs?head_sha=$pin&event=workflow_dispatch&per_page=100") { $answer = $script:World.Runs; $known = $true }
        elseif ($path -cmatch '^repos/Melanikus/AvP/actions/runs/([0-9]+)/jobs\?filter=latest&per_page=100\z') { $answer = $script:World.Jobs[$Matches[1]]; $known = $true }
    }

    if (-not $known) { $script:Unexpected += , ($words -join ' ') }

    if ($null -eq $answer)
    {
        $global:LASTEXITCODE = 1
        return
    }

    $global:LASTEXITCODE = 0
    return $answer
}

# The gate on one made-up GitHub: ok, the reason it refused, and what it gave back
function Gate([hashtable]$world, [string]$repo = $repository, [string]$at = $commit)
{
    $script:World = $world
    $script:Asked = @()
    $script:Unexpected = @()

    try
    {
        $result = Assert-AvpxLibraryScan -Repository $repo -Commit $at 6> $null
        return @{ Ok = $true; Why = ''; Result = $result }
    }
    catch
    {
        return @{ Ok = $false; Why = $_.Exception.Message; Result = $null }
    }
}

# A refusal for the reason named: refused, the reason has the words, and no question was one the made-up GitHub doesn't know
function Refused([hashtable]$answer, [string]$words, [string]$what)
{
    Check ((-not $answer.Ok) -and $answer.Why.StartsWith('REFUSED: ') -and $answer.Why.Contains($words) -and $script:Unexpected.Count -eq 0) "$what (says: $($answer.Why))"
}

Write-Host 'A good run'
$good = Gate (World)
Check ($good.Ok -and $good.Result.Run -ceq '111' -and $good.Result.Pin -ceq $pin -and $script:Unexpected.Count -eq 0) "a full, passing run on the pin, 2 hours old: let through, and it names run 111 and the pin ($($good.Why))"
Check ($script:Asked.Count -eq 3) "it asks GitHub three questions: the pin, the runs, the newest run's jobs ($($script:Asked.Count))"
Check (@($script:Asked | Where-Object { $_.Contains("runs?head_sha=$pin&event=workflow_dispatch") }).Count -eq 1) 'the runs are asked for on the pin read from the commit, started by hand only'
Check (@($script:Asked | Where-Object { $_.Contains('actions/runs/111/jobs?filter=latest') }).Count -eq 1) "the jobs are asked for of the newest run, its latest attempt"
Check ((Gate (World -runs @((Run '111' 23.5)))).Ok) 'a run 23.5 hours old: let through'
$both = Gate (World -runs @((Run '111' 5 'failure'), (Run '222' 1)) -jobs @{ '111' = $fullJobs; '222' = $fullJobs })
Check ($both.Ok -and $both.Result.Run -ceq '222') "an older run failed and the newest passed: the newest counts, so let through (run $($both.Result.Run))"
$order = Gate (World -runs @((Run '333' 1), (Run '222' 3)) -jobs @{ '333' = $fullJobs; '222' = $fullJobs })
Check ($order.Ok -and $order.Result.Run -ceq '333') "the newest is taken by its start time, whatever order GitHub lists them in (run $($order.Result.Run))"
Check ((Gate (World -pinAnswer @("submodule${T}${pin}${T}https://github.com/Melanikus/AvP"))).Ok) "avpx/'s address without .git: still AvPx's own"

Write-Host 'No run that proves a full scan of the pin'
Refused (Gate (World -runs @())) 'never been started by hand on the pin' 'no run at all: refused'
Refused (Gate (World -runs @((Run '111' 2 -sha $otherSha)))) 'never been started by hand' 'a run on another commit (GitHub ignored the question): refused'
Refused (Gate (World -runs @((Run '111' 2 -event 'push')))) 'never been started by hand' 'a run started by a push, which scans the libraries only when one changed: refused'
Refused (Gate (World -runs @((Run '111' 2 -event 'schedule')))) 'never been started by hand' 'the nightly run, which never scans the libraries: refused'
Refused (Gate (World -runs @((Run '111' 2 -from 'someone/AvP')))) 'never been started by hand' "a run from another repository: refused"
Refused (Gate (World -runs @((Run '111' 2 -path '.github/workflows/other.yml')))) 'never been started by hand' 'a run of another workflow: refused'

Write-Host 'The newest run is not a pass'
Refused (Gate (World -runs @((Run '111' 5), (Run '222' 1 'failure')) -jobs @{ '111' = $fullJobs; '222' = $fullJobs })) 'did not pass (it is failure)' 'an older run passed but the newest FAILED: refused (a vulnerability found since)'
Refused (Gate (World -runs @((Run '111' 5), (Run '222' 0.1 '' 'in_progress')) -jobs @{ '111' = $fullJobs })) 'still in_progress. Wait' 'the newest is still running: refused, wait for it'
Refused (Gate (World -runs @((Run '111' 2 'cancelled')))) 'did not pass (it is cancelled)' 'a cancelled run: refused'

Write-Host 'Its jobs'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)'), (Job 'Did the libraries change?'), (Job 'Build the vcpkg libraries' 'skipped'), (Job 'Snyk on the vcpkg libraries')) })) "'Build the vcpkg libraries' of run 111 is completed, skipped" 'the library build SKIPPED (no full scan): refused'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)' 'failure'), (Job 'Did the libraries change?'), (Job 'Build the vcpkg libraries'), (Job 'Snyk on the vcpkg libraries')) })) "'Snyk Open Source (SCA)' of run 111 is completed, failure" 'the SCA job failed under a run that says success: refused'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)'), (Job 'Snyk on the vcpkg libraries')) })) "has no job 'Build the vcpkg libraries'" 'no library build job at all (an older snyk.yml): refused'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Build the vcpkg libraries'), (Job 'Snyk on the vcpkg libraries')) })) "has no job 'Snyk Open Source (SCA)'" 'no SCA job: refused'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)'), (Job 'Build the vcpkg libraries')) })) "has no job 'Snyk on the vcpkg libraries'" 'no library scan job: refused'
Refused (Gate (World -jobs @{ '111' = @((Job 'Snyk Code (SAST)'), (Job 'Snyk Open Source (SCA)'), (Job 'Build the vcpkg libraries' '' 'in_progress'), (Job 'Snyk on the vcpkg libraries')) })) 'is in_progress' 'a job still going: refused'
Refused (Gate (World -jobs @{ '111' = @() })) 'has no jobs' 'a run with no jobs: refused'

Write-Host 'Its age'
Refused (Gate (World -runs @((Run '111' 24.5)))) 'started 24.5 hours ago' 'a run 24.5 hours old: refused'
Refused (Gate (World -runs @((Run '111' 200)))) 'needs one under 24 hours old' 'a run 200 hours old: refused'
Refused (Gate (World -runs @((Run '111' -1)))) 'in the future' 'a run that starts an hour in the future: refused'
Check ((Gate (World -runs @((Run '111' -0.05)))).Ok) 'a run three minutes ahead of this clock (the two clocks): let through'
$badTime = (Run '111' 2) -replace '\t[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}Z\t', "${T}22/11/2026 10:00${T}"
Refused (Gate (World -runs @($badTime))) 'is not a time GitHub writes' 'a start time not written as GitHub writes it: refused'

Write-Host 'The pin'
Refused (Gate (World -pinAnswer @("dir${T}${pin}${T}"))) 'is a dir, not a pinned submodule' 'avpx/ a folder, not a pin: refused'
Refused (Gate (World -pinAnswer @("submodule${T}${pin}${T}https://github.com/someone/AvP.git"))) "is not AvPx's own repository" 'avpx/ pointed at another repository: refused'
Refused (Gate (World -pinAnswer @("submodule${T}${pin}${T}https://github.com/melanikus/avp.git"))) "is not AvPx's own repository" "avpx/'s address in other capitals: refused (it's written one way)"
Refused (Gate (World -pinAnswer @("submodule${T}0de2af8${T}https://github.com/Melanikus/AvP.git"))) 'is not a 40-letter commit id' 'a short pin: refused'
Refused (Gate (World -pinAnswer @("submodule${T}${pin}${T}https://github.com/Melanikus/AvP.git", "submodule${T}${pin}${T}https://github.com/Melanikus/AvP.git"))) 'answers for avpx/' 'two answers for avpx/: refused'

Write-Host 'GitHub not answering, or answering wrongly'
Refused (Gate (World -pinAnswer $null)) 'GitHub would not say which AvPx commit' 'no answer about the pin: refused'
Refused (Gate (World -runs $null)) "GitHub would not say which runs" 'no answer about the runs: refused'
Refused (Gate (World -jobs @{})) 'GitHub would not say the jobs of run 111' 'no answer about the jobs: refused'
Refused (Gate (World -runs @("111${T}${pin}"))) 'is not in the form asked for' 'a run answer with too few fields: refused'
Refused (Gate (World -runs @((Run '111' 2 -page 'https://example.com/run/111')))) 'gives a page that is not its own' "a run whose page isn't its own on GitHub: refused"
Refused (Gate (World -runs @((Run '11a' 2)))) 'has an id that is not a number' 'a run id that is not a number: refused'

Write-Host 'What is asked about'
$early = Gate (World) -repo 'Melanikus/AVP_OpenInteng/../x'
Check ((-not $early.Ok) -and $early.Why.Contains('is not owner/name') -and $script:Asked.Count -eq 0) "a repository that isn't owner/name: refused before GitHub is asked anything ($($early.Why))"
$early = Gate (World) -at 'main'
Check ((-not $early.Ok) -and $early.Why.Contains('not a 40-letter commit id') -and $script:Asked.Count -eq 0) "a commit that isn't 40 letters: refused before GitHub is asked anything ($($early.Why))"

Write-Host "This machine's language"
$culture = [System.Threading.Thread]::CurrentThread.CurrentCulture
try
{
    [System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::GetCultureInfo('pt-BR')
    $old = Gate (World -runs @((Run '111' 24.5)))
    Check ((-not $old.Ok) -and $old.Why.Contains('24.5 hours')) "in pt-BR (decimal comma) the hours are still written 24.5 and the run read the same ($($old.Why))"
    Check ((Gate (World)).Ok) 'in pt-BR a good run is still let through'
}
finally
{
    [System.Threading.Thread]::CurrentThread.CurrentCulture = $culture
}

Write-Host 'It only reads'
Check ($script:NotReads.Count -eq 0) "in every case above, each question the gate asked was a plain read: gh api, one path, --paginate, --jq ($($script:NotReads.Count) were not$(if ($script:NotReads.Count) { ': ' + ($script:NotReads -join ' | ') }))"
$writes = @(
    @('api', '-X', 'POST', "repos/$repository/contents/avpx?ref=$commit", '--jq', '.'),
    @('api', '--method', 'PATCH', "repos/$repository/contents/avpx?ref=$commit", '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '-f', 'x=y', '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '-F', 'x=@file', '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '--field', 'x=y', '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '--raw-field', 'x=y', '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '--input', 'body.json', '--jq', '.'),
    @('api', '-H', 'X-HTTP-Method-Override: DELETE', "repos/$repository/contents/avpx?ref=$commit", '--jq', '.'),
    @('api', "repos/$repository/contents/avpx?ref=$commit", '-XPOST', '--jq', '.'),
    @('workflow', 'run', 'snyk.yml', '--repo', 'Melanikus/AvP')
)
$caught = 0
foreach ($write in $writes) { $before = $script:NotReads.Count; $null = gh @write; if ($script:NotReads.Count -eq $before + 1 -and $LASTEXITCODE -ne 0) { $caught++ } }
Check ($caught -eq $writes.Count) "and this test's own guard does catch a write: each of $($writes.Count) (-X, --method, -f, -F, --field, --raw-field, --input, -H, -XPOST, gh workflow run) is refused ($caught were)"

Write-Host 'release.yml asks it twice'
$workflow = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot '../.github/workflows/release.yml'))
$jobStarts = @(for ($i = 0; $i -lt $workflow.Count; $i++) { if ($workflow[$i] -cmatch '^  [a-z][a-z-]*:\s*$') { $i } })
function JobLines([string]$name)
{
    for ($j = 0; $j -lt $jobStarts.Count; $j++)
    {
        if ($workflow[$jobStarts[$j]] -cmatch "^  ${name}:\s*$")
        {
            $last = if ($j + 1 -lt $jobStarts.Count) { $jobStarts[$j + 1] - 1 } else { $workflow.Count - 1 }
            return , @($workflow[$jobStarts[$j]..$last])
        }
    }
    return , @()
}
$calls = @($workflow | Where-Object { $_ -cmatch 'Assert-AvpxLibraryScan -Repository \$env:GITHUB_REPOSITORY -Commit \$env:GITHUB_SHA' })
Check ($calls.Count -eq 2) "release.yml asks the gate exactly twice, each about this repository and this commit ($($calls.Count))"
$sca = JobLines 'sca'
$testAt = [array]::FindIndex([string[]]$sca, [Predicate[string]] { param($l) $l -cmatch '^\s+run: \./tools/sca-gate-test\.ps1\s*$' })
$gateAt = [array]::FindIndex([string[]]$sca, [Predicate[string]] { param($l) $l -cmatch 'Assert-AvpxLibraryScan' })
Check (@($sca | Where-Object { $_ -cmatch 'Assert-AvpxLibraryScan' }).Count -eq 1 -and $testAt -ge 0 -and $gateAt -gt $testAt) "the job sca runs this test, then the gate (lines $testAt, $gateAt of the job)"
$build = JobLines 'build'
Check (@($build | Where-Object { $_ -cmatch '^    needs: \[check, sca\]\s*$' }).Count -eq 1) 'the build needs sca: nothing is built before the gate passes'
$publish = JobLines 'publish'
$askAt = [array]::FindIndex([string[]]$publish, [Predicate[string]] { param($l) $l -cmatch 'Assert-AvpxLibraryScan' })
$attestAt = [array]::FindIndex([string[]]$publish, [Predicate[string]] { param($l) $l -cmatch '- name: Record how the package was built' })
$draftAt = [array]::FindIndex([string[]]$publish, [Predicate[string]] { param($l) $l -cmatch '- name: Make the draft\s*$' })
Check ($askAt -ge 0 -and $attestAt -gt $askAt -and $draftAt -gt $askAt) "the draft's job asks the gate again before the package is attested and the draft made (lines $askAt, $attestAt, $draftAt of the job)"
Check (@($publish | Where-Object { $_ -cmatch '^    needs: \[check, build, scan, sca\]\s*$' }).Count -eq 1) "the draft's job needs sca too"

if ($failed) { Write-Host "SOME CHECKS FAILED ($failed)"; exit 1 }
Write-Host 'All checks passed'
exit 0
