# MP1 ⇄ MP2/MP3 handoff (alpha) — design

Status: proposal, from research of the decomps and recomp sources (2026-11-23). Nothing here is built yet. Addresses
marked *(check)* were read from code but not seen working in game.

## The loop

1. MP1, Party Mode menu → Mario Party 2 or 3: MP1 fades out (its own wipe and music fade), writes the handoff file,
   starts the other recomp with `--boot`, and quits.
2. MP2/MP3 sees the handoff file at boot and goes straight to its target screen.
3. The player plays. MP2/MP3 run as they normally do, their own save included: the collection doesn't stop or change
   their saves. Start on the last results page (the one with the players' final stars and coins) → the hook sums the
   human (non-CPU) players' stars and coins, writes them into the handoff file, starts MP1 with `--boot`, and quits.
   B on the target screen returns with 0.
4. MP1 sees a returned handoff at boot and goes to Mushroom Village. As the village loads, it adds the totals to its
   save as a plain sum: stars = min(stars + returned, 9,999), coins = min(coins + returned, 99,999), anything past the
   limit dropped. It saves through its own routine (`func_8005B244`, checksum included) and deletes the handoff file.

Without a handoff file every recomp behaves exactly as it does now.

Decided 2026-11-23: MP2/MP3 saves are written normally by their own games (an earlier draft stubbed them out); the
collection only reads their totals and writes MP1's save.

## Handoff file

`%LOCALAPPDATA%\MarioPartyCollection\handoff.txt`, plain `key=value` lines, written whole to a temp file then renamed:

```
version=1
session=<random id>          # same id out and back; a stale or repeated return is ignored
target=mp2 | mp3             # set by MP1
state=out | returned         # out: MP1 → game; returned: game → MP1
stars=<n>                    # human players' total, set by MP2/MP3
coins=<n>
```

Paths of the three exes: the collection build folder holds all three (`Build NNN\MP1\`, `\MP2\`, `\MP3\`), each found
relative to the running exe.

## Shared pieces (each recomp)

- `src/collection_mp1.cpp` (MP1) / `src/collection_game.cpp` (MP2, MP3) (native): reads/writes the handoff file, starts the next exe (`CreateProcess`, Windows
  first), and quits (`ultramodern::quit()`). Exposed to patches as natives (as `recomp_survey_register` is).
- `us.toml` section "Mario Party N64 Collection" + `patches/collection.c`, as MP1 has now.
- `--boot` already exists in all three `main.cpp`s (starts the game on the launcher's first frame, ROM must already be
  stored in the recomp's `%LOCALAPPDATA%` folder — it is, for all three, on this PC).

## MP3 → Battle Royal Star Lift (researched; simplest)

- Boot: `func_8000E3C0_main` → `ovl_boot` (0x58) → `omOvlCallEx(0x7A opening, 2, 0x92)` + history → title →
  `filesel` (0x79) → `modesel` (0x77) → case 7 sets `GwSystem.playMode` (0x800CD058) = 1 and
  `omOvlCallEx(0x78 msetup, 0, 0x91)`. msetup event 0 = Battle Royal (the Star Lift).
- Redirect: when 0x58 goes to 0x7A and a handoff is out, go to **(0x78, 0, 0x91)** instead, after setting
  playMode = 1. Hook on `omOvlCallEx`/`omOvlGotoEx` entry (chain into the survey's existing `omOvlGotoEx` hook text,
  us.toml:38, not a second hook at the same address).
- Save file "Mario": file 1 is loaded at boot already (`SLCurBoxNoSet(0)` 0x8000E62C, `func_80050800`). Set the
  current box 0x800A1E81 = 0, the name at GwCommon+0x94 (0x800CC15C) = "Ｍａｒｉｏ" (Shift-JIS
  `82 6C 82 81 82 92 82 89 82 8F`), last file 0x800D09A9 = 0. No EEPROM write needed. (The user's file 1 is already
  named Mario.)
- Saves: MP3 saves as normal (its writes all go through `WriteEeprom` 0x8000CB30, `_DestroyEeprom` 0x8000CA8C, for
  reference). Back up `%LOCALAPPDATA%\MarioParty3Recompiled\saves\` before testing.
- Return: `func_801096AC_boardresult2` waits for the last page's Start and fade, then `omOvlReturnEx(1)` at
  **0x801096DC** — hook there: players 0–3 at 0x800D1108 + p·0x38, skip `stat` (+4) bit 0 (CPU), add star s8 +0x0E,
  coin s16 +0x0A. B on the lift: msetup's `omOvlReturnEx(1)` (0x80105E08) → return with 0.
- CPU flag is **bit 0** (`PLAYER_IS_CPU = 1`), not bit 1.

## MP2 → Party Mode board select, cursor on Horror Land (researched; two checks in game)

- The screen is **overlay 0x5B "Mario Land"** (board icons + Toad). Normal path: BootLogos 0x57 → Title 0x62 →
  `omOvlCallEx(0x5B, 0, 0x1081)` + `omOvlHisChg(1, 0x62, 0, 0x192)`. BootLogos goes to the Title with
  `omOvlCallEx(0x62, 1, 0x193)` + the same history entry, so replacing that call's arguments with **(0x5B, 0, 0x1081)**
  leaves exactly the history the Title would have — B and later returns work as usual. Hook `omOvlCallEx` entry
  (0x800770EC), one shot, when a0 = 0x62 and a handoff is out.
- Players: Mario Land runs the game's own player/character/CPU setup after a board is picked; nothing to pre-fill.
- Cursor: Mario Land event 0's process `func_80102C88_37F938_name_5B` sets its default slot (0x80114872) and live
  cursor (0x801148A4); Horror Land is slot **2**. Set both, and `GwSystem.current_board_index` (0x800F93AA), after
  0x80102DFC. *(check: the highlight/camera may need refreshing too.)*
- Rules Land tutorial: on a new MP2 save Mario Land plays Toad's greeting and the forced Rules Land tutorial before the
  board select. Decided: the collection skips it. In collection mode, before Mario Land event 0 tests its flags (flag
  bitfield 0x800F8CD8, `_CheckFlag` 0x80068328; event 0 tests flags 1, 2, 5–9, 0xA, 0xB, 0x14), set the flags the game
  sets when the greeting and tutorial are done, so Mario Land opens on the board select; MP2 then saves them itself.
  *(research: which flags — test with a blank save, likely 1 and/or 2.)* Fallback: a hook that takes Mario Land past
  that check to the board-select state.
- Saves: MP2 saves as normal (writes go through `func_8001AFD8_1BBD8`, `func_8001B0E8_1BCE8`, for reference). Back up
  `%LOCALAPPDATA%\MarioParty2Recompiled\saves\` before testing.
- Return: the end of a game is 0x52 (ceremony) → 0x4F/0x50 → **0x51**; overlay 0x70 is the coin results after each
  minigame, not the end (corrects CLAUDE.md). 0x51's exit `func_801076B8_357CC8_name_51` restores each player's stars,
  saves (0x80107754, left as is), and returns: hook at **0x80107714** (after the restore) to read the totals, and leave
  for MP1 after the save. Players at 0x800FD2C0
  + p·0x34, CPU = flags (s16 +6) bit 0, coins s16 +8, stars s16 +0xE. *(check: that these are the totals the player
  saw.)* B on Mario Land leaves to 0x73 (0x80102B9C) → return with 0.

## MP1 side (built in Build 003)

Done: `src/collection_mp1.cpp` (MP1) / `src/collection_game.cpp` (MP2, MP3) (natives `recomp_collection_launch`, `_take_return`, `_return_applied`, patches/syms.ld
0x8F00003C–44), `patches/collection.c` (`collection_leave_for`, `collection_boot`, `collection_boot_after`), hooks at
0x8001A304/0x8001A30C in boot (`func_8001A0F0`: save loaded by `func_8005B0C4`, then `omOvlCallEx(0x67 logos, 0, 0x81)`
→ redirected to (0x69, 0, 0x91); the history is then set by hand to title under village, as omOvlHisChg can't reach
below entry 0). Tested with a hand-made returned handoff (+5 stars, +123 coins): the save went 300 → 423 coins, 0 → 5
stars, checksum rewritten, handoff deleted. The save file has an 8-byte "HUDSON" header: GwCommon at file offset 8
(coins 0x48, stars 0x4C). Not yet seen: the village on screen after such a boot; launching MP2/MP3 for real.

Findings: MP1's board results already add coins to the bank capped at 99,999 (ResultsScene 0x800FA684, `0x1869F`).
The village HUD prints coins with `%5d` and stars with `%3d` (main 0x8005A63C / 0x8005A734, format strings at
0x800CB3CC / 0x800CB448): stars past 999 need a wider field there. Where MP1 adds a board's stars to `starNum` is not
found yet (no direct stores besides the reset).

Original notes:

- Out: on Mario Party 2/3 in the menu, run MP1's wipe + music fade (as setup state 0x26 does), then the native writes
  the handoff, launches and quits.
- Back: boot straight to Mushroom Village (overlay 0x69) with the save loaded; as the village loads, add the totals
  to `GwCommon` (`coinNum` 0x800ED140 u32, `starNum` 0x800ED144 s16), clamped, and save with `func_8005B244` (writes
  GwCommon to EEPROM). Needs its own research: MP1's boot path to the village and the overlay history it expects.
- MP1's own boards: MP1 already adds a board's results to the bank by itself. *(research: how it adds them and whether
  it caps them lower than 9,999 / 99,999 — if so, raise its cap to the collection's limits.)*

## Build and order

- Build MP2 and MP3 from source once, unmodified, as baselines (only prebuilt exes exist now).
- Order: (1) MP1 out + back with a test file (no other game), (2) MP3, (3) MP2. Each step is a numbered build.
- Back up all three `%LOCALAPPDATA%\MarioParty*Recompiled\saves\` before the first test (MP2/MP3 saves are written
  by their games in collection mode).

## Debug menu (requested 2026-11-23, not built)

Hold L+R for 3 seconds on a board → a menu: "End Party Mode Immediately", "Add 20 coins to P1", "Reset" (always last:
MP1's title; from MP2/MP3, start MP1 normally with no totals). For testing the handoff without playing 20 turns.

- UI: RecompFrontend's built-in prompt (`recompui::open_choice_prompt`) has two buttons only; a small menu context of
  our own built on recompui (modelled on `recompui/src/composites/ui_prompt.cpp`), shared by the three recomps. Input:
  `recompinput::profiles::get_n64_input(player, &buttons, ...)` polled natively for L+R. The menu's choice is a flag the
  game thread acts on at a safe point once a frame.
- Add 20 coins to P1: MP1 `GwPlayer` 0x800F32B0 (0x30 a player) coins s16 +8; MP2 0x800FD2C0 +8; MP3 0x800D1108 +0x0A
  (the board's own coin cap per game to respect).
- End Party Mode Immediately: per game. MP1: `GwSystem` 0x800ED5C0 maxTurns +6, currentTurn +8; `func_8004D008` =
  turns left (max − current + 1); `func_80056F40` advances the player and, after the 4th, the turn (cap 99); the board
  queues overlays through `func_80056730(overlay, event, stat)` (e.g. 0x6F minigame instructions from 0x8005719C).
  Still to find: where the end of game is decided, to jump there directly (skipping the last minigame). MP2: main's
  `func_80063950_64550` tests 0x800E1F94 & 0x20 and queues 0x52 (end ceremony). MP3: to research.
