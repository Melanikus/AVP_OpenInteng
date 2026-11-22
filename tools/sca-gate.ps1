<#
sca-gate.ps1: THE RELEASE'S LIBRARY SCAN GATE. An editor release ships the libraries AvPx builds at the commit avpx/ is pinned to (OpenAL Soft,
fmt, the two Smacker libraries and the rest of AvPx's package), so a release is made only when AvPx's OWN Snyk workflow has scanned that very
commit in full, passed, and started less than 24 hours ago. Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng, on Bishop 2's order
(the owner's question "We don't run snyk SCA on OpenInteng?"; Tao 1: no severity threshold, and a scan that can't run is a failed scan).
AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

WHAT IT PROVES. Nothing of AvPx's scan is copied here: this only READS GitHub's record of a run of AvPx's own .github/workflows/snyk.yml, through
GitHub's API, and refuses the release unless all of these hold:
- the run is on the pin: its head_sha is the commit avpx/ points at in the commit being released (read from GitHub, so the draft job, which has
  no copy of avpx/, asks the same question the same way);
- it was started by hand (event workflow_dispatch): only then does AvPx's workflow scan the libraries in full (its job "Did the libraries
  change?" answers yes "it was run by hand"). A push or a pull request scans them only when a library file changed, and the nightly run never;
- it is the NEWEST such run, and it finished and passed. A newer run that failed (a vulnerability found since) refuses the release even when an
  older one passed; a newer run still going says to wait for it;
- every job of it passed, none skipped, and among them "Snyk Open Source (SCA)", "Build the vcpkg libraries" and "Snyk on the vcpkg libraries".
  "Snyk on the vcpkg libraries" passes at once, saying why, when it has nothing to scan, so it is "Build the vcpkg libraries" (which runs only
  for a full scan) having passed that proves the libraries were built and scanned;
- it started less than 24 hours before this check (and not in the future), so a vulnerability published since an older scan is not let
  through. release.yml asks twice: before anything is built, and again in the job that makes the draft, after the owner's approvals, which can
  take hours (Bishop 2, 2026-11-22).
There is no severity threshold here because there is none in AvPx's run: ANY finding fails its job. Anything GitHub won't answer is a refusal.

HOW A RUN GETS ONTO THE PIN (docs/TAKING-OVER.md, "Before a release"): a hand-started workflow runs on a branch's or a tag's newest commit, and
the pin is often older than avpx_main, so the one making the release puts a tag editor-pin/<the pin> on the pin in Melanikus/AvP and starts
AvPx's Snyk workflow on that tag (gh workflow run snyk.yml --repo Melanikus/AvP --ref editor-pin/<the pin>). The tag's name isn't checked:
head_sha is the proof, and a run started on avpx_main when its newest commit is the pin proves the same. The run uses the pin's own snyk.yml.

Use: . ./tools/sca-gate.ps1; Assert-AvpxLibraryScan -Repository <owner/name of this repository> -Commit <the 40-letter commit being released>
It writes what it found and gives back the run it took; any refusal is thrown, as "REFUSED: <why>". tools/sca-gate-test.ps1 is its test.
Works in Windows PowerShell 5.1 and PowerShell 7.
#>

# AvPx: where its Snyk workflow runs, the workflow, and the jobs a full scan has
$script:AvpxRepository = 'Melanikus/AvP'
$script:AvpxSnykWorkflow = '.github/workflows/snyk.yml'
$script:AvpxFullScanJobs = @('Snyk Open Source (SCA)', 'Build the vcpkg libraries', 'Snyk on the vcpkg libraries')
$script:AvpxSubmoduleUrl = '^https://github\.com/Melanikus/AvP(\.git)?\z'
$script:ScanMostHours = 24
$script:ScanFutureMostMinutes = 10   # a run's start a little ahead of this machine's clock is the two clocks; more than this is refused

function Stop-ScaGate([string]$why)
{
    throw "REFUSED: $why"
}

# GitHub's answer to one question, as lines of tab-separated fields (gh's own jq writes them, escaping any tab inside a field, so a field never
# splits). A question GitHub won't answer is a refusal, named. Each line must have exactly the fields asked for.
function Get-ScaGateAnswer([string]$what, [string]$path, [string]$jq, [int]$fields, [switch]$Paginate)
{
    $lines = if ($Paginate) { @(gh api --paginate $path --jq $jq) } else { @(gh api $path --jq $jq) }

    if ($LASTEXITCODE -ne 0)
    {
        Stop-ScaGate "GitHub would not say $what"
    }

    $rows = @()

    foreach ($line in @($lines | Where-Object { $_ -ne $null -and "$_" -ne '' }))
    {
        $parts = "$line".Split("`t")

        if ($parts.Count -ne $fields)
        {
            Stop-ScaGate "GitHub's answer about $what is not in the form asked for: $line"
        }

        $rows += , $parts
    }

    return , $rows
}

# A time as GitHub writes it (2026-11-22T10:15:30Z), read the same way whatever this machine's own language is
function ConvertFrom-ScaGateTime([string]$text, [string]$what)
{
    $styles = [System.Globalization.DateTimeStyles]::AssumeUniversal -bor [System.Globalization.DateTimeStyles]::AdjustToUniversal
    $time = [DateTimeOffset]::MinValue

    if ($text -cnotmatch '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\z' -or
        -not [DateTimeOffset]::TryParseExact($text, "yyyy-MM-dd'T'HH:mm:ss'Z'", [System.Globalization.CultureInfo]::InvariantCulture, $styles, [ref]$time))
    {
        Stop-ScaGate "$what is not a time GitHub writes: $text"
    }

    return $time
}

function Assert-AvpxLibraryScan([string]$Repository, [string]$Commit)
{
    # What's being released. Both come from GitHub's own settings for the run, but they go into the questions below, so they're checked first
    if ($Repository -cnotmatch '^[A-Za-z0-9-]{1,39}/[A-Za-z0-9._-]{1,100}\z')
    {
        Stop-ScaGate "the repository is not owner/name: $Repository"
    }

    if ($Commit -cnotmatch '^[0-9a-f]{40}\z')
    {
        Stop-ScaGate "the commit being released is not a 40-letter commit id: $Commit"
    }

    # 1. THE PIN: what avpx/ points at in the commit being released, and that it is AvPx's own repository
    $pinRows = Get-ScaGateAnswer 'which AvPx commit avpx/ is pinned to' "repos/$Repository/contents/avpx?ref=$Commit" '[.type, .sha, (.submodule_git_url // "")] | @tsv' 3

    if ($pinRows.Count -ne 1)
    {
        Stop-ScaGate "GitHub gave $($pinRows.Count) answers for avpx/ in $Commit, not one"
    }

    $kind, $pin, $url = $pinRows[0]

    if ($kind -cne 'submodule')
    {
        Stop-ScaGate "avpx/ in $Commit is a $kind, not a pinned submodule"
    }

    if ($pin -cnotmatch '^[0-9a-f]{40}\z')
    {
        Stop-ScaGate "avpx/'s pin in $Commit is not a 40-letter commit id: $pin"
    }

    if ($url -cnotmatch $script:AvpxSubmoduleUrl)
    {
        Stop-ScaGate "avpx/ in $Commit is not AvPx's own repository (https://github.com/$($script:AvpxRepository)): $url"
    }

    # 2. THE RUNS of AvPx's Snyk workflow on the pin that were started by hand. GitHub is asked for only those, and each one is checked again
    # here all the same: an answer is believed only as far as it is checked
    $runFields = '[(.id|tostring), .head_sha, .event, .status, (.conclusion // ""), (.run_started_at // ""), .path, (.head_repository.full_name // ""), .html_url] | @tsv'
    $runRows = Get-ScaGateAnswer "which runs of AvPx's Snyk workflow there are on $pin" "repos/$($script:AvpxRepository)/actions/workflows/snyk.yml/runs?head_sha=$pin&event=workflow_dispatch&per_page=100" ".workflow_runs[] | $runFields" 9 -Paginate
    $runs = @($runRows | Where-Object { $_[1] -ceq $pin -and $_[2] -ceq 'workflow_dispatch' -and $_[6] -ceq $script:AvpxSnykWorkflow -and $_[7] -ceq $script:AvpxRepository })

    if ($runs.Count -eq 0)
    {
        Stop-ScaGate "AvPx's Snyk workflow has never been started by hand on the pin $pin, so its libraries have no full scan. Start it there (docs/TAKING-OVER.md, Before a release), wait for it to pass, then release"
    }

    foreach ($run in $runs)
    {
        if ($run[0] -cnotmatch '^[1-9][0-9]{0,19}\z')
        {
            Stop-ScaGate "a run of AvPx's Snyk workflow has an id that is not a number: $($run[0])"
        }

        # (its page, only ever printed: it has to be that run's own page on GitHub)
        if ($run[8] -cne "https://github.com/$($script:AvpxRepository)/actions/runs/$($run[0])")
        {
            Stop-ScaGate "run $($run[0]) of AvPx's Snyk workflow gives a page that is not its own on GitHub: $($run[8])"
        }

        # (a run not started yet has no start time: it counts as the newest, and is waited for below)
        if ($run[5] -ne '')
        {
            $null = ConvertFrom-ScaGateTime $run[5] "The start of run $($run[0])"
        }
    }

    # The newest one counts. GitHub's times are all written the same way, so the text sorts as the time does; a run with no start yet sorts last
    $newest = @($runs | Sort-Object -Property @{ Expression = { if ($_[5] -eq '') { '9999' } else { $_[5] } } }, @{ Expression = { [long]$_[0] } })[-1]
    $id, $null, $null, $status, $conclusion, $started, $null, $null, $link = $newest

    if ($status -cne 'completed')
    {
        Stop-ScaGate "the newest run of AvPx's Snyk workflow on the pin is still $status. Wait for it to finish: $link"
    }

    if ($conclusion -cne 'success')
    {
        Stop-ScaGate "the newest run of AvPx's Snyk workflow on the pin did not pass (it is $conclusion), so a vulnerability or an error stands in its libraries: $link"
    }

    # 3. ITS JOBS: every one passed, none skipped, and the full scan's three among them
    $jobs = Get-ScaGateAnswer "the jobs of run $id" "repos/$($script:AvpxRepository)/actions/runs/$id/jobs?filter=latest&per_page=100" '.jobs[] | [.name, .status, (.conclusion // "")] | @tsv' 3 -Paginate

    if ($jobs.Count -eq 0)
    {
        Stop-ScaGate "run $id of AvPx's Snyk workflow has no jobs: $link"
    }

    foreach ($job in $jobs)
    {
        if ($job[1] -cne 'completed' -or $job[2] -cne 'success')
        {
            Stop-ScaGate "the job '$($job[0])' of run $id is $($job[1]), $($job[2]): every job of a full scan has to pass, and a skipped one scanned nothing: $link"
        }
    }

    foreach ($needed in $script:AvpxFullScanJobs)
    {
        if (@($jobs | Where-Object { $_[0] -ceq $needed }).Count -eq 0)
        {
            Stop-ScaGate "run $id of AvPx's Snyk workflow has no job '$needed', so it isn't a full scan of the libraries (the pin's snyk.yml may be older than that job: docs/TAKING-OVER.md says what to do): $link"
        }
    }

    # 4. ITS AGE: started less than 24 hours ago, and not in the future
    $startedAt = ConvertFrom-ScaGateTime $started "The start of run $id"
    $age = [DateTimeOffset]::UtcNow - $startedAt
    # (the hours with one decimal, written with a point whatever this machine's own language is. 0.0, not 0: with a whole 0 PowerShell picks
    # Math.Max's whole-number form and rounds the hours to a whole number first, which the test caught as 24.5 written 25.0)
    $hours = [Math]::Round([Math]::Max(0.0, $age.TotalHours), 1).ToString('0.0', [System.Globalization.CultureInfo]::InvariantCulture)

    if ($age.TotalMinutes -lt -$script:ScanFutureMostMinutes)
    {
        Stop-ScaGate "run $id says it started at $started, which is in the future by this machine's clock: $link"
    }

    if ($age.TotalHours -ge $script:ScanMostHours)
    {
        Stop-ScaGate "the newest full scan of the pin's libraries (run $id) started $hours hours ago, at $started; a release needs one under $($script:ScanMostHours) hours old. Start AvPx's Snyk workflow on the pin again (docs/TAKING-OVER.md, Before a release): $link"
    }

    Write-Host "AvPx's libraries at the pin $pin were scanned in full by run $id, started at $started ($hours hours ago), and every one of its $($jobs.Count) jobs passed: $link"

    return [pscustomobject]@{ Pin = $pin; Run = $id; Started = $started; Link = $link }
}
