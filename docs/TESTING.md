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

## Checking addresses on the real game: ares

ares (`emu\ares-148-release`) runs the original ROM with a GDB server (Settings > Debug; port 9123). Use it to confirm
an address before hooking it, or to compare the recomp against the original:

    powershell -File tools\ares_gdb.ps1 -Read 0x800ED140 -Length 8
    powershell -File tools\ares_gdb.ps1 -Write 0x800ED140 -Hex 000003E7
    powershell -File tools\ares_gdb.ps1 -Packet "Z2,800ed140,4"     # write watchpoint (a stop shows the writer's PC)

ares halts the game while a client is asking; the script detaches (`D`) at the end, which resumes it. Any GDB client
works too (e.g. `gdb-multiarch` in WSL with `target remote <Windows IP>:9123`), but ares drops a client whose first
byte isn't `+`. Leave "Use IPv4" off unless WSL needs it: with it on, ares listens on every network interface.

## Crashes

Windows event log, event ID 1000, gives the fault offset. Symbolize with the RelWithDebInfo `.pdb`:
`llvm-symbolizer --obj=build\MarioPartyRecompiled.exe 0x<140000000 + offset>`.
