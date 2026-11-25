# Testing

Back up the saves first: `%LOCALAPPDATA%\MarioPartyRecompiled\` (MP1; the other recomps keep theirs beside it).
A backup from before any change is in `saves-backup-2026-11-23`.

## Incremental builds: the survey

Each recomp ships a survey (`patches/survey.c`, `src/game/survey.cpp`, hook at `omOvlGotoEx`'s start): a script
writes an overlay index into a struct found in RDRAM by its two magic words, and the next time the game changes
overlay it goes there instead. Use it to jump straight to the screen a change touches (a board, a menu, a result
screen) instead of playing up to it.

## Milestone builds: full scripted tests

`tools\drive_game.ps1` starts a recomp (or `-Attach`es), presses keys and saves screenshots (PrintWindow,
DPI-aware). Steps: `wait:<s>`, `key:<k>`, `keys:A+B:<s>` (held together), `shot:<name>`. Keys: Enter = Start,
Space = A, LShift = B, E = L, R = R, WASD = stick. A `build\` exe needs `-WorkDir` = its repo root (it needs
`assets\`); a `bin\build-NNN` exe runs from its own folder. `-Keep` leaves the game running.

A milestone pass runs the whole loop: MP1 village → Warp Pipe → Pick a Mario Party → MP2 (and MP3) → debug menu
End Party → results → back in MP1 with the totals added; Quit Game and Reset from each game; B paths.

## Crashes

Windows event log, event ID 1000, gives the fault offset. Symbolize with the RelWithDebInfo `.pdb`:
`llvm-symbolizer --obj=build\MarioPartyRecompiled.exe 0x<140000000 + offset>`.
