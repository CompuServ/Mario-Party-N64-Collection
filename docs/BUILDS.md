# Build history

Packaged by `tools\package_build.ps1 -Notes "..."` into `bin\build-NNN`. `bin\` keeps the last 10 builds plus the
milestones in `docs\milestones.txt`. Builds hold ROM-derived code: local testing only, never shared or committed.

Builds 001–010 were made before the reorganization (folders without spaces, `bin\build-NNN`); their `collection.ini`
was rewritten to absolute paths into their own `MP2\` / `MP3\` (001–002 have none; 003–004 use `prebuilt\`). From
build-011 on, `collection.ini` holds paths relative to MP1's folder.

| Build | What changed | Tested |
|---|---|---|
| 001 ★ | Toad's Warp Pipe menu defaults to "Depart for Adventure" (MP1 hook 0x800F775C, `ctx->r5 = 0`). | by hand |
| 002 | "Pick a Mario Party." menu after setup's first message (Mario Party 1/2/3, B → village via state 0x26). B on the number of players (setup state 0xA, 0x800F7928) returns to the menu. | by hand |
| 003 | MP1's side of the handoff (`collection_mp1.cpp`, natives in `patches/syms.ld`, `collection.ini`, `handoff.txt`). Return tested with a hand-made file (totals saved, direct boot to the village); launch out starts the unmodified MP2. | by hand |
| 004 | Village opens on "Choose the place…" (0x97) instead of Toad's greeting (0x96), hook 0x800F7454. | by hand |
| 005 ★ | All three games: MP2 to Mario Land's board select on Horror Land without Toad's greeting; MP3 to the Battle Royal Star Lift with file 1 "Mario". | by script |
| 006 | MP1's return goes through boot (adds totals, 0x8001A304) → logos take Start as pressed (0x800F6718) → their omOvlCallEx(0x61) becomes the village (0x800F67B8). Fixed the wrong music/SFX from booting straight into the village. | by hand (audio right) |
| 007 | Village opens on its cursor with the Warp Pipe's line: 0x96/0x97 → empty message (0x800F7454), one scripted A in the text window's update (0x8006FB60), last-shown line −1 (0x800F7508). | by script |
| 008 ★ | MP1 board debug menu (`patches/collection.c` "Debug menu"): gated by the pause watcher func_8005835C, opened like the pause menu; End Party = exit mask 0x800D86EC = 0x20. | by script (End Party) |
| 009 | Setup opens on "Pick a Mario Party" (0x284 replaced at 0x800F77F4; Mini-Game Stadium's 0x26C untouched). Village's scripted A cleared once the village is up (it had leaked into setup's window). Bank stars ";%5d" (0x800F6874), Item List stars "%4d" (0x8005A73C). | by hand |
| 010 ★ | MP2 board debug menu: gate on turn-start flag 0x800FA198; opened through MP2's own pause with its menu process swapped at 0x8005DB1C; End Party/Reset via the pause's quit branch 0x8005DB90 with the board still paused (after the pause it crashed in SprKill). MP2's end-of-party totals latched at 0x80107714 (read at 0x80107794 they were 0). MP1's menu gated on 0x800EE320, holding the turn like its pause. Handoff `state=reset`. Full MP2→MP1 round trip ran by script (totals 0 before the latch fix). | by hand ("It worked") |

| 011 | Reorganization only: folders without spaces, MP2/MP3 compile the one `src\collection_game.cpp`, `collection.ini` relative to MP1's folder (new in `collection_mp1.cpp`), recomps' changes on branch `collection`. Full rebuild of all three. | MP1 → MP3 launch (by hand) |
| 012 ★ | MP3 board debug menu: L+R 3 s at a turn's start (gate `mbCameraBtnF` 0x800CDD58 + items flag 0x800CDD64; hook 0x800FF1B0 in the pause watcher), opened through `MBPauseCreate` with its menu process swapped (0x800F1FB4); End Party / Reset through the pause's quit branch (0x800F20E0): End Party queues 0x4F as the exit watcher does for mask 0x20. P1 = controller 1's player (MP3 sorts `GwPlayer` into turn order). | by script: blocked in Koopa Bank, CPU turns, minigame instructions and play; Add 20 / Max Coins / Max Stars; End Party → results → MP1 +99 stars +30 coins saved; Reset → MP1 boot |

| 013 | Settings shared: MP2/MP3 copy MP1's `general/graphics/sound/controls.json` over their own before `main` (static initializer in `src/collection_game.cpp`), a thread copies their changes back to MP1's within 1 s, and again before returning to MP1. MP3 Star Lift opens on "How many will play this time?" (0x1503): msetup's message helper (`func_8010B82C_msetup`) skips 0x1500 Welcome and 0x1502 Tumble's two pages, its wait helper (`func_8010B858_msetup`) returns at once after them. | by script: Star Lift opening; settings pull at start, push while running (file level) |

| 014 ★ | Collection launcher `MarioPartyCollection.exe` (`src/launcher/`, `tools/build_launcher.cmd`): starts MP1 (`--boot` when its ROM is stored), black topmost backdrop over the outgoing window's place (monitor when fullscreen) on each handover (`src/collection_window.hpp`: WM_COPYDATA 'MPCS'), next window moved to the same place, backdrop hidden 1.5 s after it shows; quits 5 s after the last game closes. Handover saves the actual window mode into MP1's `graphics.json` (F11 doesn't save it). MP1/MP2 debug menus: blue window (background 0xC8). MP1/MP2 P1 = controller 1's player (port u8 +3). | by script (`capture_screen.ps1`): MP1↔MP3 windowed (same place and size, black between) and fullscreen (black, no desktop or taskbar); MP1→MP2 party, MP2 menu blue, Add 20 / Max Stars on Mario (3rd in turn order), End Party → MP1; MP1 menu blue, Add 20 |

★ milestone.

## Not yet verified

- Settings changed through the in-game menu in MP2/MP3 reaching MP1 (tested by editing the files, not through the menu).

- MP2 returning real totals after the latch fix (0x80107714).
- MP3's Quit Game return since its hook became `collection_pause_quit` (build-012).
- Whether MP1's and MP2's "P1" debug items hit controller 1's player: MP3 sorts its players into turn order, so slot 0 was whoever went first; MP1 and MP2 use slot 0 too.
