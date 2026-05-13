# Editor Startup — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce wall-clock time of the `./CkAuto/UnrealToolbox.exe --build --test` cycle by shipping a measurement layer plus a project-side cleanup bundle and a small AS-side investigation/fix pass.

**Architecture:** Two PowerShell scripts in `CkAuto/` extract timing markers from the existing UE log (no editor instrumentation) and diff two snapshots. Project-side cleanup is `.ini` / `.uproject` edits. AS work is a discovery task that may or may not produce a small engine-fork patch — the plan has explicit gates.

**Tech Stack:** PowerShell 7, Pester (PS testing), UE 5.5, Hazelight AngelScript plugin (`D:\Repos\UnrealEngineAngelscript`).

**Spec:** [docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md](../specs/2026-05-13-editor-startup-phase1-design.md)

**Scope correction from spec:** `Measure-Startup.ps1` is a **standalone** script run against any toolbox-produced log; it does NOT add a `--measure` flag to the toolbox binary (the toolbox is precompiled, no source available). Workflow becomes: run toolbox → run `Measure-Startup.ps1 -LogPath …`. Functionally equivalent to the spec.

---

## File Structure

**Created:**
- `CkAuto/Measure-Startup.ps1` — parses a UE log, writes JSON timing snapshot.
- `CkAuto/Compare-Startup.ps1` — diffs two snapshots, prints delta table.
- `CkAuto/Tests/Measure-Startup.Tests.ps1` — Pester test pinning regex against a known fixture.
- `CkAuto/Tests/Fixtures/Baseline-CkPlugins.log` — frozen excerpt of a known-good log for the test.
- `docs/superpowers/plans/2026-05-13-as-double-reload-findings.md` — written during Task 8; outcome of AS investigation.

**Modified:**
- `Config/DefaultEngine.ini` — platform `.ini` cull + misc settings sweep.
- `CkPlugins.uproject` — engine plugin disables (audit-driven, list confirmed before flipping).
- *(conditional)* `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\<file>` — small AS reload-flow fix and/or debug-DB deferral. Only if budget gates pass (≤50 lines for #9, ≤30 lines for #10).

---

## Task 1: Skeleton `Measure-Startup.ps1` with config detection

**Files:**
- Create: `CkAuto/Measure-Startup.ps1`

- [ ] **Step 1: Create the skeleton script**

Create `CkAuto/Measure-Startup.ps1`:

```powershell
#requires -Version 7.0
<#
.SYNOPSIS
Parse a UnrealToolbox-produced UE log and emit a JSON timing snapshot.

.DESCRIPTION
Extracts startup phase durations from a CkPlugins editor log (e.g.
Saved/Logs/CkPlugins.log or Saved/Logs/BuildTest.log) and writes a
JSON snapshot suitable for diffing across runs.

Pure log-parsing — no editor instrumentation. If a future UE version
changes log line wording, the corresponding regex stops matching and
that phase reports null in the output (rather than silently producing
wrong numbers). Pester test catches this on the next CI run.

.PARAMETER LogPath
Path to the UE log file to parse. Required.

.PARAMETER Out
Path to write the JSON snapshot. If omitted, prints JSON to stdout.

.PARAMETER Config
The build configuration the editor was launched in (DebugGame /
Development / etc.). Embedded in the snapshot so Compare-Startup.ps1
can refuse to diff runs configured differently.

.EXAMPLE
./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/CkPlugins.log -Out Saved/Logs/Startup-baseline.json -Config DebugGame
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [Parameter(Mandatory = $false)]
    [string]$Out,

    [Parameter(Mandatory = $false)]
    [ValidateSet('DebugGame', 'Development', 'Test', 'Shipping', 'Debug')]
    [string]$Config = 'DebugGame'
)

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Error "Log file not found: $LogPath"
    exit 1
}

$snapshot = [ordered]@{
    run_id = [System.IO.Path]::GetFileNameWithoutExtension($LogPath) + '_' + (Get-Date -Format 'yyyyMMdd-HHmmss')
    config = $Config
    log_path = (Resolve-Path -LiteralPath $LogPath).Path
    phases = [ordered]@{}
}

$json = $snapshot | ConvertTo-Json -Depth 5
if ($Out) {
    $outDir = Split-Path -Parent -Path $Out
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    }
    Set-Content -LiteralPath $Out -Value $json -Encoding utf8
    Write-Host "Snapshot written: $Out"
} else {
    Write-Output $json
}
```

- [ ] **Step 2: Run it against the existing log to confirm the skeleton works**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/CkPlugins.log
```

Expected: JSON output with empty `phases: {}`, populated `run_id`, `config: "DebugGame"`, `log_path` resolved to an absolute path. No errors.

- [ ] **Step 3: Commit**

```bash
git add CkAuto/Measure-Startup.ps1
git commit -m "feat(CkAuto): Measure-Startup.ps1 skeleton — JSON snapshot framing only"
```

---

## Task 2: Extract `engine_init_seconds` and `total_to_pie_ready_seconds`

**Files:**
- Modify: `CkAuto/Measure-Startup.ps1`

- [ ] **Step 1: Add a helper to parse the log's leading timestamp**

In `Measure-Startup.ps1`, after the `$snapshot = …` block and before the `$json = …` block, insert:

```powershell
# UE log lines look like: "[2026.05.13-17.25.58:097][  0]LogInit: ..."
# Format: YYYY.MM.DD-HH.MM.SS:fff   — millisecond precision.
$timestampRx = '^\[(?<ts>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]'

function Convert-UeTimestampToDateTime {
    param([string]$Ts)
    # "2026.05.13-17.25.58:097" -> DateTime
    if ($Ts -match '^(?<y>\d{4})\.(?<mo>\d{2})\.(?<d>\d{2})-(?<h>\d{2})\.(?<mi>\d{2})\.(?<s>\d{2}):(?<ms>\d{3})$') {
        return [datetime]::new(
            [int]$Matches['y'], [int]$Matches['mo'], [int]$Matches['d'],
            [int]$Matches['h'], [int]$Matches['mi'], [int]$Matches['s'],
            [int]$Matches['ms'], [System.DateTimeKind]::Local)
    }
    return $null
}

# Read once; we'll re-use $lines for all marker extraction.
$lines = Get-Content -LiteralPath $LogPath -Encoding utf8

# Find first timestamped line (start of the run).
$startTs = $null
foreach ($line in $lines) {
    if ($line -match $timestampRx) {
        $startTs = Convert-UeTimestampToDateTime -Ts $Matches['ts']
        break
    }
}

# Find "Engine is initialized. Leaving FEngineLoop::Init()".
$engineInitTs = $null
foreach ($line in $lines) {
    if ($line -match 'LogInit:\s*Display:\s*Engine is initialized\.\s*Leaving FEngineLoop::Init') {
        if ($line -match $timestampRx) {
            $engineInitTs = Convert-UeTimestampToDateTime -Ts $Matches['ts']
        }
        break
    }
}

# Find first "LogAutomationCommandLine: Display: Test Started" (or last AS reload marker as fallback).
$testStartTs = $null
foreach ($line in $lines) {
    if ($line -match 'LogAutomationController:\s*Display:\s*Test Started') {
        if ($line -match $timestampRx) {
            $testStartTs = Convert-UeTimestampToDateTime -Ts $Matches['ts']
        }
        break
    }
}

if ($startTs -and $engineInitTs) {
    $snapshot.phases['engine_init_seconds'] = [math]::Round(($engineInitTs - $startTs).TotalSeconds, 3)
} else {
    $snapshot.phases['engine_init_seconds'] = $null
}

if ($startTs -and $testStartTs) {
    $snapshot.phases['total_to_pie_ready_seconds'] = [math]::Round(($testStartTs - $startTs).TotalSeconds, 3)
} else {
    $snapshot.phases['total_to_pie_ready_seconds'] = $null
}
```

- [ ] **Step 2: Run it against the existing log and verify numbers**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest.log
```

Expected: JSON output with `engine_init_seconds` around 18, `total_to_pie_ready_seconds` around 26. Exact values vary per run.

Sanity check by hand against the log:
```bash
grep -nE "Engine is initialized|LogAutomationController.*Test Started" Saved/Logs/BuildTest.log | head -3
```
The reported numbers must match the deltas between those timestamps.

- [ ] **Step 3: Commit**

```bash
git add CkAuto/Measure-Startup.ps1
git commit -m "feat(CkAuto): Measure-Startup extracts engine_init + total_to_pie_ready"
```

---

## Task 3: Extract the AS-specific phase markers

**Files:**
- Modify: `CkAuto/Measure-Startup.ps1`

- [ ] **Step 1: Add the per-AS-marker extractor**

Before the final `$json = …` block in `Measure-Startup.ps1`, insert:

```powershell
# AS markers are repeat-able lines like:
#   "Angelscript: == bindings total == took 1569.312 ms"
#   "Angelscript: class generator reload took 1512.389 ms"      (occurs twice)
#   "Angelscript: ==script reload total == took 2700.654 ms"    (occurs once)
#   "Angelscript: Sending debug database took 0.739 seconds"
#
# We capture all matches per pattern and assign by ordinal (first / second occurrence)
# where the AS init flow runs the same step twice.

function Get-AsMarkerMs {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [int]$Ordinal = 0
    )
    $hits = @()
    foreach ($line in $Lines) {
        if ($line -match $Pattern) {
            if ($Matches['ms']) {
                $hits += [double]$Matches['ms']
            } elseif ($Matches['sec']) {
                $hits += [double]$Matches['sec'] * 1000.0
            }
        }
    }
    if ($Ordinal -lt $hits.Count) {
        return $hits[$Ordinal]
    }
    return $null
}

function ConvertTo-Seconds {
    param([System.Nullable[double]]$Ms)
    if ($null -eq $Ms) { return $null }
    return [math]::Round($Ms / 1000.0, 3)
}

# Pattern fragments — `ms` and `sec` named groups feed Get-AsMarkerMs.
# Note: AS sometimes uses "ms" and sometimes "seconds" suffixes.
$rxBindingsTotal   = 'Angelscript:\s*==\s*bindings total\s*==\s*took\s+(?<ms>[\d.]+)\s*ms'
$rxClassGenReload  = 'Angelscript:\s*class generator reload took\s+(?<ms>[\d.]+)\s*ms'
$rxScriptReload    = 'Angelscript:\s*==script reload total\s*==\s*took\s+(?<ms>[\d.]+)\s*ms'
$rxScriptCompile   = 'Angelscript:\s*script compilation total took\s+(?<ms>[\d.]+)\s*ms'
$rxPostFullReload  = 'Angelscript:\s*post full reload took\s+(?<ms>[\d.]+)\s*ms'
$rxDebugDb         = 'Angelscript:\s*Sending debug database took\s+(?<sec>[\d.]+)\s+seconds'

$snapshot.phases['as_bindings_seconds']        = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxBindingsTotal 0)
$snapshot.phases['as_class_gen_reload_1_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxClassGenReload 0)
$snapshot.phases['as_class_gen_reload_2_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxClassGenReload 1)
$snapshot.phases['as_script_compile_1_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxScriptCompile 0)
$snapshot.phases['as_script_compile_2_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxScriptCompile 1)
$snapshot.phases['as_post_full_reload_1_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxPostFullReload 0)
$snapshot.phases['as_post_full_reload_2_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxPostFullReload 1)
$snapshot.phases['as_script_reload_total_seconds'] = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxScriptReload 0)
$snapshot.phases['as_debug_db_seconds']        = ConvertTo-Seconds (Get-AsMarkerMs $lines $rxDebugDb 0)
```

- [ ] **Step 2: Run and verify against expected log values**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest.log
```

Expected (from the 2026-05-13 snapshot — your numbers will differ slightly run-to-run):
- `as_bindings_seconds`: ~1.57
- `as_class_gen_reload_1_seconds`: ~1.51
- `as_class_gen_reload_2_seconds`: ~1.0
- `as_script_reload_total_seconds`: ~2.70
- `as_debug_db_seconds`: ~0.74

Cross-check by hand:
```bash
grep -E "bindings total|class generator reload|script reload total|post full reload|Sending debug database" Saved/Logs/BuildTest.log
```

- [ ] **Step 3: Commit**

```bash
git add CkAuto/Measure-Startup.ps1
git commit -m "feat(CkAuto): Measure-Startup extracts all AS-side timing markers"
```

---

## Task 4: Pin parser behavior with a Pester test

**Files:**
- Create: `CkAuto/Tests/Measure-Startup.Tests.ps1`
- Create: `CkAuto/Tests/Fixtures/Baseline-CkPlugins.log`

- [ ] **Step 1: Capture a fixture log**

Extract a minimal log fragment containing every marker line plus enough context that the timestamp-delta math works. From the bottom of `Saved/Logs/BuildTest.log`, copy the section that runs from the first `LogInit: Initializing FReadOnlyCVARCache` through the first `LogAutomationController: Display: Test Started` line and the `Angelscript: Sending debug database` line.

```bash
mkdir -p CkAuto/Tests/Fixtures
# Manually copy the relevant chunk. Aim for ~100-200 lines covering:
#  - first timestamped line
#  - Engine is initialized line
#  - all 6 AS marker lines (bindings, class gen reload x2, script reload total, post full reload x2, debug db, script compile total x2)
#  - first Test Started line
# Write the chunk to CkAuto/Tests/Fixtures/Baseline-CkPlugins.log.
```

If easier, write a one-off extraction:
```bash
grep -nE "LogInit: Initializing FReadOnlyCVARCache|Engine is initialized|Angelscript:|LogAutomationController: Display: Test Started" Saved/Logs/BuildTest.log | head -50
# Pick the surrounding ranges and use sed -n 'A,Bp' Saved/Logs/BuildTest.log >> CkAuto/Tests/Fixtures/Baseline-CkPlugins.log
```

The fixture must be a real log excerpt, not synthetic. Synthetic lines drift from real log wording and defeat the test's purpose.

- [ ] **Step 2: Write the Pester test**

Create `CkAuto/Tests/Measure-Startup.Tests.ps1`:

```powershell
#requires -Modules @{ ModuleName = 'Pester'; ModuleVersion = '5.0.0' }

BeforeAll {
    $script:ScriptUnderTest = Join-Path $PSScriptRoot '..' 'Measure-Startup.ps1'
    $script:Fixture         = Join-Path $PSScriptRoot 'Fixtures' 'Baseline-CkPlugins.log'
}

Describe 'Measure-Startup.ps1' {

    It 'emits valid JSON for a known-good fixture' {
        $json = & $script:ScriptUnderTest -LogPath $script:Fixture -Config DebugGame
        $obj  = $json | ConvertFrom-Json
        $obj.config       | Should -Be 'DebugGame'
        $obj.phases       | Should -Not -BeNullOrEmpty
    }

    It 'parses engine_init_seconds' {
        $json = & $script:ScriptUnderTest -LogPath $script:Fixture -Config DebugGame
        $obj  = $json | ConvertFrom-Json
        $obj.phases.engine_init_seconds | Should -BeOfType [double]
        $obj.phases.engine_init_seconds | Should -BeGreaterThan 0
    }

    It 'parses both AS class-gen-reload occurrences distinctly' {
        $json = & $script:ScriptUnderTest -LogPath $script:Fixture -Config DebugGame
        $obj  = $json | ConvertFrom-Json
        $obj.phases.as_class_gen_reload_1_seconds | Should -Not -BeNullOrEmpty
        $obj.phases.as_class_gen_reload_2_seconds | Should -Not -BeNullOrEmpty
        # The two passes must be different durations (if they're identical the fixture is wrong).
        $obj.phases.as_class_gen_reload_1_seconds | Should -Not -Be $obj.phases.as_class_gen_reload_2_seconds
    }

    It 'parses as_script_reload_total_seconds' {
        $json = & $script:ScriptUnderTest -LogPath $script:Fixture -Config DebugGame
        $obj  = $json | ConvertFrom-Json
        $obj.phases.as_script_reload_total_seconds | Should -BeGreaterThan 0
    }

    It 'parses as_debug_db_seconds (which is in seconds-suffix format)' {
        $json = & $script:ScriptUnderTest -LogPath $script:Fixture -Config DebugGame
        $obj  = $json | ConvertFrom-Json
        $obj.phases.as_debug_db_seconds | Should -BeGreaterThan 0
    }

    It 'reports null for a missing marker rather than crashing' {
        $empty = New-TemporaryFile
        try {
            Set-Content -LiteralPath $empty.FullName -Value '[2026.05.13-17.25.58:097][  0]LogInit: nothing useful'
            $json = & $script:ScriptUnderTest -LogPath $empty.FullName -Config DebugGame
            $obj  = $json | ConvertFrom-Json
            $obj.phases.engine_init_seconds           | Should -BeNullOrEmpty
            $obj.phases.as_bindings_seconds           | Should -BeNullOrEmpty
        } finally {
            Remove-Item -LiteralPath $empty.FullName -Force -ErrorAction SilentlyContinue
        }
    }
}
```

- [ ] **Step 3: Run the test, confirm green**

```bash
pwsh -NoProfile -Command "Invoke-Pester -Path CkAuto/Tests/Measure-Startup.Tests.ps1 -Output Detailed"
```

Expected: all 6 tests pass. If Pester isn't installed, run `pwsh -Command "Install-Module Pester -Scope CurrentUser -Force"` first.

- [ ] **Step 4: Commit**

```bash
git add CkAuto/Tests/Measure-Startup.Tests.ps1 CkAuto/Tests/Fixtures/Baseline-CkPlugins.log
git commit -m "test(CkAuto): pin Measure-Startup regex against fixture log"
```

---

## Task 5: `Compare-Startup.ps1` — diff two snapshots

**Files:**
- Create: `CkAuto/Compare-Startup.ps1`

- [ ] **Step 1: Write Compare-Startup.ps1**

Create `CkAuto/Compare-Startup.ps1`:

```powershell
#requires -Version 7.0
<#
.SYNOPSIS
Diff two startup snapshot JSONs (produced by Measure-Startup.ps1).

.DESCRIPTION
Prints a per-phase delta table comparing Before vs After. Refuses to diff
runs configured differently (e.g. DebugGame vs Development) — these aren't
comparable because optimisation levels differ dramatically.

Missing phases on one side are flagged so a "regex drifted" outcome from
the parser side isn't accidentally interpreted as an improvement.

.PARAMETER Before
Path to the baseline snapshot JSON.

.PARAMETER After
Path to the post-change snapshot JSON.

.EXAMPLE
./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-baseline.json Saved/Logs/Startup-after.json
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Before,

    [Parameter(Mandatory = $true, Position = 1)]
    [string]$After
)

if (-not (Test-Path -LiteralPath $Before)) { Write-Error "Before snapshot not found: $Before"; exit 1 }
if (-not (Test-Path -LiteralPath $After))  { Write-Error "After snapshot not found: $After"; exit 1 }

$b = Get-Content -LiteralPath $Before -Raw | ConvertFrom-Json
$a = Get-Content -LiteralPath $After  -Raw | ConvertFrom-Json

if ($b.config -ne $a.config) {
    Write-Error "Configs differ: before=$($b.config) vs after=$($a.config). Re-run both with the same -Config."
    exit 2
}

$phaseNames = ($b.phases.PSObject.Properties.Name + $a.phases.PSObject.Properties.Name) | Sort-Object -Unique

$rows = foreach ($phase in $phaseNames) {
    $beforeVal = $b.phases.$phase
    $afterVal  = $a.phases.$phase

    $missing = ''
    if ($null -eq $beforeVal -and $null -ne $afterVal) { $missing = 'BEFORE-MISSING' }
    elseif ($null -ne $beforeVal -and $null -eq $afterVal) { $missing = 'AFTER-MISSING' }
    elseif ($null -eq $beforeVal -and $null -eq $afterVal) { $missing = 'BOTH-MISSING' }

    $delta = $null
    $pct   = $null
    if ($null -ne $beforeVal -and $null -ne $afterVal) {
        $delta = [math]::Round($afterVal - $beforeVal, 3)
        if ($beforeVal -ne 0) {
            $pct = [math]::Round((($afterVal - $beforeVal) / $beforeVal) * 100.0, 1)
        }
    }

    [pscustomobject]@{
        Phase  = $phase
        Before = $beforeVal
        After  = $afterVal
        Delta  = $delta
        'Pct%' = $pct
        Note   = $missing
    }
}

$rows | Format-Table -AutoSize

# Summary line — total_to_pie_ready_seconds delta if both present.
$total_b = $b.phases.total_to_pie_ready_seconds
$total_a = $a.phases.total_to_pie_ready_seconds
if ($null -ne $total_b -and $null -ne $total_a) {
    $totalDelta = [math]::Round($total_a - $total_b, 3)
    $sign = if ($totalDelta -lt 0) { 'FASTER' } elseif ($totalDelta -gt 0) { 'SLOWER' } else { 'SAME' }
    Write-Host ""
    Write-Host "Total wall-clock: $($total_b)s -> $($total_a)s  ($totalDelta s, $sign)"
}
```

- [ ] **Step 2: Smoke-test by comparing a snapshot to itself**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest.log -Out /tmp/snap.json -Config DebugGame
pwsh ./CkAuto/Compare-Startup.ps1 /tmp/snap.json /tmp/snap.json
```

Expected: every Delta is 0, every Pct% is 0, total wall-clock prints `SAME`.

- [ ] **Step 3: Commit**

```bash
git add CkAuto/Compare-Startup.ps1
git commit -m "feat(CkAuto): Compare-Startup.ps1 diffs two snapshots, refuses mismatched configs"
```

---

## Task 6: Capture baseline snapshot (post-measurement, pre-changes)

**Files:**
- Create: `Saved/Logs/Startup-baseline.json` (NOT committed — gitignored)
- Modify: `.gitignore` if needed (already covers `Saved/`)

- [ ] **Step 1: Run a fresh build+test cycle**

```bash
# Editor lock check
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
# If 'locked', wait it out.
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --output=Saved/Logs/BuildTest-baseline.log --project='D:\Repos\CkPlugins'"
```

Expected: build+test runs to completion with exit code 0. 19/19 IskmRenderer tests pass.

- [ ] **Step 2: Snapshot it**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest-baseline.log -Out Saved/Logs/Startup-baseline.json -Config DebugGame
cat Saved/Logs/Startup-baseline.json
```

Expected: JSON with all phases populated. Record the numbers — these are the values to beat.

- [ ] **Step 3: No commit needed — `Saved/` is gitignored**

Skip. The baseline lives on your machine for comparison; it doesn't go to the repo.

---

## Task 7: Audit currently-loaded engine plugins

**Files:**
- Read-only audit, no writes yet. Output: a list to confirm with the user.

- [ ] **Step 1: List which plugins UE loaded during the baseline run**

```bash
grep -oE "Mounting plugin [A-Za-z0-9]+|LogPluginManager: Mounting plugin [A-Za-z0-9]+" Saved/Logs/BuildTest-baseline.log | sort -u
```

Expected: a sorted list of plugin names that loaded during startup. Save to a scratch file:

```bash
grep -oE "Mounting plugin [A-Za-z0-9]+" Saved/Logs/BuildTest-baseline.log | sort -u > /tmp/loaded-plugins.txt
wc -l /tmp/loaded-plugins.txt
```

- [ ] **Step 2: Cross-reference against the project's allowlist**

`CkPlugins.uproject` already disables ~15 plugins explicitly (Paper2D, SubversionSourceControl, etc.). Identify plugins that are loading but the project doesn't actively use. Candidates to consider:

- `XRBase`, `OpenXR`, `OpenXREyeTracker`, `OpenXRHandTracking`, `OpenXRMsftHandInteraction` — VR/AR, not used by CkPlugins or any downstream Ck game.
- `OnlineFramework`, `OnlineSubsystem*` (except whatever EOS/SteamGameServers we actually need) — host project doesn't ship multiplayer.
- `MediaCompositing`, `MediaFrameworkUtilities`, `MovieRenderPipeline*` — host project doesn't render movies or use Sequencer-as-render-pipeline.
- `LegacyEditorWidgets` — UE 5.x deprecation tier; nothing in CkPlugins uses it.
- `MobileLauncherProfileWizard` — already disabled, but verify it's still loading via a transitive dep.
- `OodleNetwork`, `OodleData` (if not used at runtime) — runtime cost is non-zero.
- `ChaosNiagara`, `GeometryCollection*` if CkChaos doesn't actually use Chaos GeometryCollection.
- `LiveCoding` — if not used during toolbox runs, can be off. (Confirm with user — they may rely on it for non-toolbox iteration.)

**Output a candidate list** as a markdown table in a scratch file or directly in the conversation, with: plugin name, observed in baseline, reason candidate, risk-of-disabling. **Do not flip any toggles yet.**

- [ ] **Step 3: Present the candidate list to the user, get the confirmed-disable set**

Stop and ask. The audit produces a hypothesis; the user approves which entries actually move into Task 8's bundle.

---

## Task 8: Project-side cleanup bundle PR

**Files:**
- Modify: `Config/DefaultEngine.ini`
- Modify: `CkPlugins.uproject`

- [ ] **Step 1: Platform `.ini` cull — confirm the right key for UE 5.5**

The log lines we want to silence:
```
LogConfig: Display: Loading VulkanPC ini files took 0.10 seconds
LogConfig: Display: Loading Mac ini files took 0.10 seconds
... (8 more platforms)
```

Search the engine source for the actual config key that drives this:

```bash
grep -rn "PlatformsToLoad\|LoadAllPlatformConfigs\|ConfigSystemPlatforms" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Core/ 2>/dev/null | head -10
```

This is a discovery step — the key name may be `[/Script/UnrealEd.EditorPlatformSettings] +PlatformsToLoad=Windows` or `[Core.System] +PlatformsToLoad=Windows` or something else in 5.5. Record the actual key.

- [ ] **Step 2: Add the platform cull setting and verify**

In `Config/DefaultEngine.ini`, after the existing `[SystemSettings]` section (around line 75), add a section with the discovered key. Example shape (replace key/section with what Step 1 found):

```ini
[Core.System]
+PlatformsToLoad=Windows
```

Then run a single editor cycle (no test):

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --output=Saved/Logs/Build-platform-cull.log --project='D:\Repos\CkPlugins'"
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --output=Saved/Logs/Test-platform-cull.log --project='D:\Repos\CkPlugins'"
```

Then check:
```bash
grep -c "Loading .* ini files took" Saved/Logs/Test-platform-cull.log
```

Expected: dramatically fewer matches than baseline (ideally 1-2 for Windows only, vs ~10 for all platforms). If the count is unchanged, the key didn't work — try a different key from Step 1's candidates. If the build broke or tests fail, revert and document why in the commit message of the next step.

- [ ] **Step 3: Disable confirmed engine plugins**

For each plugin in the user-confirmed list from Task 7 Step 3, add an entry to the `Plugins` array of `CkPlugins.uproject`:

```json
{
    "Name": "<PluginName>",
    "Enabled": false
}
```

**Do one plugin at a time.** After each addition:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --output=Saved/Logs/BuildTest-plugin-<name>.log --project='D:\Repos\CkPlugins'"
```

Expected: build+test passes with 19/19 tests green. If the toolbox fails with `Plugin X failed to load` or similar, **revert that one entry** and move to the next candidate. Record both successes and reverts in a scratch list.

- [ ] **Step 4: Misc DefaultEngine.ini sweep**

Concrete candidates (only land the ones you confirm work):

```ini
[/Script/LiveCoding.LiveCodingSettings]
bEnabled=False
```

(Disable Live Coding auto-load — saves a small amount of module-init time. Only land if user said they don't use Live Coding during the toolbox loop.)

Verify after each addition with another build+test cycle.

- [ ] **Step 5: Snapshot and compare**

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest-plugin-<last>.log -Out Saved/Logs/Startup-after-bundle.json -Config DebugGame
pwsh ./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-baseline.json Saved/Logs/Startup-after-bundle.json
```

Expected: `total_to_pie_ready_seconds` is lower than baseline. Record by how much.

- [ ] **Step 6: Commit**

```bash
git add Config/DefaultEngine.ini CkPlugins.uproject
git commit -m "perf(startup): cull platform .ini loads + disable unused engine plugins

Baseline ./CkAuto/UnrealToolbox.exe --build --test --test-pattern IskmRenderer
showed ~X seconds spent in:
  - 10x platform .ini loads (~1.2s)
  - module init for <N> plugins the host project never uses

Restrict platform .ini loading to Windows only; disable <list>. Tests
remain green (19/19 IskmRenderer). Total wall-clock saving: <Ys>."
```

(Replace `<X>`, `<N>`, `<list>`, `<Ys>` with the actual values.)

---

## Task 9: AS double-reload investigation

**Files:**
- Create: `docs/superpowers/plans/2026-05-13-as-double-reload-findings.md`
- (Possibly) Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\<file>` — only if fix is ≤50 lines.

- [ ] **Step 1: Identify the two reload sites in the engine source**

The log shows two AS reload passes — call them "Pass A" (around log time +6s, ends with `post full reload took 178 ms`) and "Pass B" (around log time +24s, ends with `==script reload total == took 2700 ms`).

Pass B is preceded by `==script reload total ==` which is the marker for `FAngelscriptCodeModule::ReloadScripts` (or equivalent — confirm in source).

Search for the trigger sites:

```bash
grep -rnE "class generator reload|script reload total|post full reload" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/ 2>/dev/null
```

Then find what calls each path. Likely entry points to look at:
- `FAngelscriptCodeModule::StartupModule`
- `FAngelscriptCodeModule::ReloadScripts` / `ReloadAll`
- `FAngelscriptManager::Initialize` / `BindRegistered`
- Editor delegates: `FEditorDelegates::OnEditorBoot`, `OnPostEngineInit`, `OnAllSourceCodeMapsLoaded`

- [ ] **Step 2: Add log breadcrumbs to confirm the call chain**

Temporarily, in the Angelscript engine fork, add `UE_LOG(LogAngelscript, Display, TEXT("AS-RELOAD-TRIGGER: %s called from %s"), TEXT(<site>), TEXT(<context>));` at each candidate trigger site identified in Step 1. Rebuild the engine plugin only (not the whole editor target — should be a quick incremental build). Run:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --output=Saved/Logs/Test-as-trace.log --project='D:\Repos\CkPlugins'"
grep "AS-RELOAD-TRIGGER" Saved/Logs/Test-as-trace.log
```

You should see two lines, one for Pass A and one for Pass B. Note the trigger context.

- [ ] **Step 3: Determine the root cause and write findings**

Possible outcomes (record the actual one in the findings doc):

- **(a) Editor fires the same delegate twice during init.** E.g. `OnPostEngineInit` and `OnPostInitProperties` both end up causing a reload. Fix: guard the second reload with a `bReloadAlreadyRan` flag. Likely <20 lines.
- **(b) The two reloads do different work intentionally.** Pass A does "bind native types and register UFUNCTION wrappers"; Pass B does "post-content-load script recompile after assets are loaded". Both legitimate. Fix is structural — defer to Phase 2.
- **(c) Hot reload (live coding) integration fires a redundant reload.** Fix: skip the reload if hot reload isn't actually loaded.
- **(d) Something else.** Investigate.

Create `docs/superpowers/plans/2026-05-13-as-double-reload-findings.md`:

```markdown
# AS Double-Reload Investigation Findings

**Date:** 2026-05-13
**Author:** <implementer>
**Outcome:** [(a) one-line fix landed / (b) structural, deferred to Phase 2 / (c) hot-reload guarded / (d) other]

## Pass A (~log timestamp +6s)
- Triggered by: <delegate / call site>
- Work performed: <summary>

## Pass B (~log timestamp +24s)
- Triggered by: <delegate / call site>
- Work performed: <summary>

## Are the two passes doing the same work?
[Yes / No / Partially — what overlaps]

## Decision
[Fix here in ≤50 lines / Defer to Phase 2]

## If fixed: what changed?
[engine file / lines / commit hash]

## If deferred: what's the Phase 2 shape?
[Brief — Phase 2 spec will expand]
```

- [ ] **Step 4: Remove the temporary log breadcrumbs from Step 2**

If breadcrumbs from Step 2 are still in the engine fork, remove them. They were diagnostic only. Verify with another grep that they're gone.

- [ ] **Step 5: Decide and execute**

- If outcome is (b) — structural — commit ONLY the findings doc. STOP this task. Skip Task 10 too (no AS fix lands in Phase 1).
- If outcome is (a)/(c)/(d) AND the fix is ≤50 lines — proceed to Step 6.

- [ ] **Step 6: Land the fix (only if Step 5 decided yes)**

Edit the appropriate file in `D:\Repos\UnrealEngineAngelscript\…`. The fix shape depends on the outcome but here's an example for outcome (a):

```cpp
// In FAngelscriptCodeModule::PostEngineInit or wherever Pass B is triggered:
static bool bReloadAlreadyRanThisSession = false;
if (bReloadAlreadyRanThisSession) {
    return;
}
bReloadAlreadyRanThisSession = true;
// ... existing reload logic ...
```

Rebuild, re-test:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --output=Saved/Logs/BuildTest-as-fix.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 tests pass. Snapshot and compare:

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest-as-fix.log -Out Saved/Logs/Startup-after-as-fix.json -Config DebugGame
pwsh ./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-after-bundle.json Saved/Logs/Startup-after-as-fix.json
```

Expected: `as_class_gen_reload_2_seconds`, `as_post_full_reload_2_seconds`, `as_script_reload_total_seconds` drop close to zero (or to `null`-with-explanation, depending on what the fix does). Total wall-clock drops by ~5 sec.

- [ ] **Step 7: Commit the fix in the engine fork**

```bash
cd D:/Repos/UnrealEngineAngelscript
git add Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/<file>
git commit -m "fix(angelscript): elide redundant <pass-name> reload during editor init

Editor init was firing the same reload path twice: <root-cause summary>.
Pass A did <work>, Pass B repeated <same work>, costing ~5 sec on a
typical DebugGame launch. Guard the second invocation so it only runs
on actual hot-reload / live-coding triggers, not on initial boot."
```

Push it. (User runs the push if non-interactive auth blocks the agent.)

- [ ] **Step 8: Commit the findings doc in CkPlugins**

```bash
cd D:/Repos/CkPlugins
git add docs/superpowers/plans/2026-05-13-as-double-reload-findings.md
git commit -m "docs(plans): AS double-reload investigation findings"
```

---

## Task 10: AS debug-database deferral (conditional)

**Skip this task entirely if Task 9 produced a regression in `as_script_reload_total_seconds` or `as_class_gen_reload_*_seconds`.** The conditional is: only land if Task 9 didn't make things worse. Re-check the comparison output from Task 9 Step 6 before starting.

**Files:**
- Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\<debugger-server-file>` — exact file TBD via Step 1.

- [ ] **Step 1: Find where `Sending debug database` is logged**

```bash
grep -rn "Sending debug database" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/ 2>/dev/null
```

Expected: one or two hits. Read the surrounding code — it's the AS-debugger server pushing the type/symbol database. Identify the function and what gates it (probably nothing — it sends unconditionally on startup).

- [ ] **Step 2: Check if Hazelight already has a "debugger attached" or "lazy-send" flag**

```bash
grep -rnE "IsDebuggerAttached|bDebuggerConnected|HasClient|GetNumClients" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/Debug 2>/dev/null
```

If a flag like `bDebuggerConnected` already exists, gate the send on it: send only when a client connects. If no such flag exists, add one (small — a bool plus the existing OnClientConnected callback).

- [ ] **Step 3: Modify the send call site to defer**

Example (replace with what Step 1/2 actually find):

```cpp
// Before:
void FAngelscriptDebuggerServer::OnEngineInitComplete() {
    SendDebugDatabase();  // ~740ms blocking
}

// After:
void FAngelscriptDebuggerServer::OnEngineInitComplete() {
    // Debug database is sent lazily on first client connect.
    // See FAngelscriptDebuggerServer::OnClientConnected.
}

void FAngelscriptDebuggerServer::OnClientConnected(/* …client info… */) {
    if (!bDebugDatabaseSent) {
        SendDebugDatabase();
        bDebugDatabaseSent = true;
    }
    // …existing client-handshake code…
}
```

**Budget reminder:** total change ≤30 lines. If it grows past that, revert and document as a Phase 2 candidate in the findings doc from Task 9.

- [ ] **Step 4: Verify**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --output=Saved/Logs/BuildTest-as-debugdb.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 tests pass. The `Sending debug database` log line should NOT appear during the no-debugger-attached run.

```bash
grep -c "Sending debug database" Saved/Logs/BuildTest-as-debugdb.log
```

Expected: 0.

Snapshot and compare:

```bash
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest-as-debugdb.log -Out Saved/Logs/Startup-after-debugdb.json -Config DebugGame
pwsh ./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-after-as-fix.json Saved/Logs/Startup-after-debugdb.json
```

Expected: `as_debug_db_seconds` reports `null` (or `AFTER-MISSING` note) since the marker no longer fires. Total wall-clock drops by ~0.7s.

- [ ] **Step 5: Confirm debugger connection still works**

Manual check: launch the AS debugger client (VSCode extension or whatever you use), connect to a running editor. The debug DB should send on connect — verify by looking for the same log line *after* the connection event.

If the debugger can't get types after this change, the deferral broke type resolution. Revert and document as Phase 2 in the findings doc.

- [ ] **Step 6: Commit the engine fork change**

```bash
cd D:/Repos/UnrealEngineAngelscript
git add Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/<debugger-file>
git commit -m "perf(angelscript-debugger): defer debug database send until first client connect

SendDebugDatabase was running unconditionally during editor init,
costing ~740ms even when no debugger client was ever going to connect.
Move the call into the OnClientConnected path, guarded by a one-shot
flag. Same payload reaches the client on first connect; idle editors
save the cost entirely."
```

Push it (user runs the push if non-interactive auth blocks the agent).

---

## Task 11: Final snapshot + success-criteria sign-off

**Files:**
- (Possibly) modify: `docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md` — append final results.

- [ ] **Step 1: Bump submodule SHAs in CkPlugins parent (if engine fork moved)**

If Tasks 9 or 10 landed engine-side fixes, the GitLink / Foundation / etc. submodules didn't move — but the engine itself did. Engine changes don't show in CkPlugins's git tree (engine is external to the project). No submodule bump needed for engine changes. Skip this step.

If you also bumped one of the Ck* submodules during the bundle/audit, commit that pointer bump now in CkPlugins.

- [ ] **Step 2: One final clean baseline cycle**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --output=Saved/Logs/BuildTest-phase1-final.log --project='D:\Repos\CkPlugins'"
pwsh ./CkAuto/Measure-Startup.ps1 -LogPath Saved/Logs/BuildTest-phase1-final.log -Out Saved/Logs/Startup-phase1-final.json -Config DebugGame
pwsh ./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-baseline.json Saved/Logs/Startup-phase1-final.json
```

Expected: total wall-clock savings reflect the union of Tasks 7-10. Print the table.

- [ ] **Step 3: Decide on Phase 2**

If remaining startup time is acceptable → Phase 1 ends. Done.
If meaningful headroom remains (e.g. you still see 18s in engine init, or asset-registry scans look chunky) → write a Phase 2 spec. Likely Phase 2 candidates from the original design:

- Tooltip backport (`SDeferredToolTip` from UE 5.8)
- Async / background AssetRegistry
- Plugin culling of our own Ck* set

- [ ] **Step 4: Update the spec with final numbers**

Append a "Phase 1 Results" section to `docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md`:

```markdown
## Phase 1 Results (filled in on completion)

**Baseline (2026-05-13):** total_to_pie_ready_seconds = X.Xs
**After Phase 1:**         total_to_pie_ready_seconds = Y.Ys
**Saving:** Z.Zs (P%)

Per-phase breakdown:
| Phase | Before | After | Delta |
|---|---|---|---|
| engine_init_seconds        | … | … | … |
| as_bindings_seconds        | … | … | … |
| as_class_gen_reload_1      | … | … | … |
| as_class_gen_reload_2      | … | … | … |
| as_script_reload_total     | … | … | … |
| as_debug_db_seconds        | … | … | … |
| **total_to_pie_ready**     | … | … | … |

Phase 2 decision: [pursue / not pursue]. Reasoning: <…>.
```

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md
git commit -m "docs(spec): record Phase 1 startup-optimisation results"
```

Push.

---

## Self-Review Notes

**Spec coverage:** Deliverable 1 (measurement) → Tasks 1-5; Deliverable 2 (project bundle) → Tasks 6-8; Deliverable 3 (AS investigation) → Task 9; Deliverable 4 (AS debug-DB) → Task 10; final sign-off → Task 11. All four spec deliverables have at least one task.

**Conditional sequencing matches spec:** Task 10 explicitly skips if Task 9 produced a regression; Task 11 Step 1 handles the case where engine vs. submodule changes happened. Findings doc is required output regardless of fix-or-defer.

**Budget gates from spec are preserved:** ≤50 lines for Task 9 fix, ≤30 lines for Task 10 fix. Both are explicit "revert and document" branches.

**No placeholders in test code or scripts.** The `<file>` placeholders in Tasks 9-10 are inside a code block introduced as "Example (replace with what Step 1/2 actually find)" — they're explicitly framed as discovery-output substitutions, not unfilled blanks the engineer should ignore.
