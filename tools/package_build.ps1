<#
Copies the three recomps' current builds (recomp\mp1, mp2, mp3) into the next numbered folder of bin\ (build-001,
build-002, ...), so every build stays runnable on its own and can be compared with the others, then keeps bin\ to the
last 10 builds plus the milestones listed in docs\milestones.txt (one build number a line).

Each bin\build-NNN folder holds:
  MarioPartyRecompiled.exe + SDL2.dll, dxil.dll, dxcompiler.dll, recompcontrollerdb.txt, LICENSE, assets\
      Mario Party 1, what tools/release.sh puts in a release; it runs from its own folder (double-click the exe).
      Start the collection here.
  MP2\, MP3\              Mario Party 2 and 3 the same way (MarioParty2Recompiled.exe, MarioParty3Recompiled.exe).
  collection.ini          where MP1 finds the MP2 and MP3 exes, relative to its own folder (MP2\..., MP3\...).
  changes\mp1|mp2|mp3\changes.patch   git diff of each recomp against its upstream commit (us.toml, patches\, src\,
                          CMakeLists.txt), new files included; changes\mpN\files\ copies of every changed or new file.
  BUILD.txt               build number, date, each recomp's commit and the notes given with -Notes.

Usage (after the three builds succeeded):
  powershell -ExecutionPolicy Bypass -File tools\package_build.ps1 -Notes "what changed"

The builds hold code recompiled from the user's ROMs: they are for testing on this computer, never to be shared or
committed (CLAUDE.md).
#>
param(
    [string]$Notes = "",
    [string]$Sources = "$PSScriptRoot\..\recomp",
    [string]$Out = "$PSScriptRoot\..\bin",
    [int]$Keep = 10
)
# Continue, not Stop: git writes line-ending warnings to stderr, and diff --no-index exits 1 whenever files differ.
$ErrorActionPreference = "Continue"
$git = "C:\Program Files\Git\cmd\git.exe"
$Sources = (Resolve-Path $Sources).Path
$Out = (Resolve-Path $Out).Path

$games = @(
    @{ Key = "mp1"; Exe = "MarioPartyRecompiled.exe"; Sub = "" },
    @{ Key = "mp2"; Exe = "MarioParty2Recompiled.exe"; Sub = "MP2" },
    @{ Key = "mp3"; Exe = "MarioParty3Recompiled.exe"; Sub = "MP3" }
)
foreach ($g in $games) {
    $exe = Join-Path (Join-Path $Sources $g.Key) "build\$($g.Exe)"
    if (-not (Test-Path $exe)) { throw "No build at ${exe}: build that recomp first." }
}

# The next number: one past the highest build-NNN already there.
$last = Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^build-(\d{3})$' } |
    ForEach-Object { [int]$Matches[1] } | Sort-Object | Select-Object -Last 1
$number = if ($null -eq $last) { 1 } else { $last + 1 }
$name = "build-{0:D3}" -f $number
$dest = Join-Path $Out $name
New-Item -ItemType Directory $dest | Out-Null

$info = @()
foreach ($g in $games) {
    $repo = Join-Path $Sources $g.Key
    $folder = if ($g.Sub -eq "") { $dest } else { Join-Path $dest $g.Sub }
    New-Item -ItemType Directory $folder -Force | Out-Null

    # The program, as a release has it.
    foreach ($f in "build\$($g.Exe)", "build\SDL2.dll", "build\dxil.dll", "build\dxcompiler.dll",
                   "recompcontrollerdb.txt", "LICENSE") {
        Copy-Item (Join-Path $repo $f) $folder
    }
    Copy-Item (Join-Path $repo "assets") $folder -Recurse

    # What the collection changed in the recomp: a patch of the tracked files, plus the new ones.
    $changes = Join-Path (Join-Path $dest "changes") $g.Key
    New-Item -ItemType Directory (Join-Path $changes "files") -Force | Out-Null
    $paths = @("us.toml", "patches", "src", "CMakeLists.txt")
    # Against main (the upstream commit): the collection branch's commits and anything not committed yet.
    $diff = & $git -C $repo diff --no-color main -- $paths
    $new = @(& $git -C $repo ls-files --others --exclude-standard -- $paths | Where-Object { $_ -match '\.(c|h|cpp|hpp|toml|ld|txt)$' })
    foreach ($f in $new) {
        $diff += & $git -C $repo diff --no-color --no-index -- /dev/null $f
    }
    [IO.File]::WriteAllText((Join-Path $changes "changes.patch"), (($diff -join "`n") + "`n"))
    $changed = @(& $git -C $repo diff --name-only main -- $paths) + $new
    $uncommitted = @(& $git -C $repo diff --name-only HEAD -- $paths) + $new
    foreach ($f in $changed) {
        $target = Join-Path (Join-Path $changes "files") $f
        New-Item -ItemType Directory (Split-Path $target) -Force | Out-Null
        Copy-Item (Join-Path $repo $f) $target
    }
    $commit = & $git -C $repo log -1 --format="%h %s"
    $branch = & $git -C $repo rev-parse --abbrev-ref HEAD
    $info += "recomp\$($g.Key): $branch @ $commit"
    $info += "    uncommitted: $(if ($uncommitted.Count) { $uncommitted -join ', ' } else { 'none' })"
}
$collectionCommit = & $git -C (Split-Path $Out) log -1 --format="%h %s" 2>$null
if ($collectionCommit) { $info += "collection: $collectionCommit" }

# Where MP1's handoff (src/collection_mp1.cpp) finds the other games: this build's own, relative to MP1's folder.
@"
# Mario Party N64 Collection: the other games' exes, for MP1's Party Mode menu (relative to this folder).
mp2=MP2\MarioParty2Recompiled.exe
mp3=MP3\MarioParty3Recompiled.exe
"@ | Set-Content (Join-Path $dest "collection.ini") -Encoding ascii

@"
$name
Date: $(Get-Date -Format "yyyy-MM-dd HH:mm")
$($info -join "`n")

$Notes
"@ | Set-Content (Join-Path $dest "BUILD.txt") -Encoding utf8

"$name -> $dest"

# Keep the last $Keep builds and the milestones (docs\milestones.txt: build numbers, one a line, # for comments).
$milestoneFile = "$PSScriptRoot\..\docs\milestones.txt"
$milestones = @()
if (Test-Path $milestoneFile) {
    $milestones = Get-Content $milestoneFile | ForEach-Object { ($_ -replace '#.*$', '').Trim() } |
        Where-Object { $_ -match '^\d+$' } | ForEach-Object { [int]$_ }
}
$builds = Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^build-(\d{3})$' } |
    Sort-Object { [int]($_.Name.Substring(6)) } -Descending
$builds | Select-Object -Skip $Keep | Where-Object { $milestones -notcontains [int]($_.Name.Substring(6)) } |
    ForEach-Object { Remove-Item $_.FullName -Recurse -Force; "removed old build $($_.Name)" }
