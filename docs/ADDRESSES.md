# Addresses

RDRAM addresses (N64 virtual) found so far, per game. Function names are the ones in each recomp's `syms/` (check
there before using a name in `us.toml`). Hooks are listed in `docs\HOOKS.md`.

## Mario Party 1 (recomp\mp1)

| Address | Type | What |
|---|---|---|
| 0x800ED100 | struct | `GwCommon` |
| 0x800ED140 | u32 | `coinNum` (bank coins) |
| 0x800ED144 | s16 | `starNum` (bank stars) |
| 0x800ED148 | | `GwCommon` flags (+0x48) |
| 0x800EE320 | s16 | TURN_CHOICE: turn-start flag (debug menu gate) |
| 0x800F3848 | | GAME_FRAME counter |
| 0x800F544C | | held buttons |
| 0x800F32BC | | P1 stars (board) |
| 0x800D86EC | s16 | board exit mask (0x20 = game over) |
| 0x800D86EE | | BOARD_EXIT_WIPE |
| 0x800D86F0 | | BOARD_EXIT_FADE_MUSIC |
| 0x801025B4 | s16 | Adventure setup's window handle |
| 0x8006D7D8 | func | LoadStringIntoWindow(window, msg, -1, -1): msg > 0x80000000 is a pointer to an encoded string |
| 0x8006FCF0 | func | choice: func_8006FCF0(window, default, 0) |
| 0x8006FCC0 | func | wait on a window |
| 0x8005B244 | func | save (EEPROM, with checksum): the only way MP1's save is written |
| 0x800F6884 | | 100-star event (one-time; open question whether to keep it) |

Overlays: 0x61 opening, 0x69 Mushroom Village (`GameModeOverworld`), 0x6A Adventure setup, 0x81 title. Setup state
0xA = number of players, 0x26 = back to the village. Messages: 0x96 Toad's greeting, 0x97 "Choose the place…",
0x99 Warp Pipe two-option choice, 0x284 setup's first message, 0x26C Mini-Game Stadium's.

Text codes (MP1): 0x0B start, 0x0C…0x0D a choice, 0x0A newline, 0x10 space, 0xFF wait for A, 0x85 period,
0xC3 question mark. `tools\dump_mp1_messages.ps1 -Ids 0x99,0x284` prints messages from the ROM.

## Mario Party 2 (recomp\mp2)

| Address | Type | What |
|---|---|---|
| 0x800FD2C0 | struct[4] | players, stride 0x34: flags s16 +0x06 (CPU bit to confirm), coins s16 +0x08, stars s16 +0x0E |
| 0x800FD2CE | s16 | P1 stars |
| 0x800FA198 | s16 | TURN_CHOICE: turn-start flag (debug menu gate) |
| 0x80101504 | | held buttons |
| 0x800E1CF6 | s16 | PAUSE_QUIT |
| 0x800E1F94 | s16 | board exit mask (0x20) |
| 0x800E1F82 | | set to 1 on End Party |
| 0x800F64B0 | | second sound (stopped on End Party) |
| func_8005DA20_5E620 | func | pause opener (port) |
| func_8005DCC8_5E8C8 | func | add coins |
| func_8008BB48 | func | choice (window, default, 0) |

Overlays: 0x5B Mario Land, 0x52 → 0x4F/0x50 → 0x51 end of game, 0x70 coin results after each minigame, 0x61 queued
after Bowser Land. Text space is 0x20. `tools\dump_mp2_messages.ps1` reads messages.

## Mario Party 3 (recomp\mp3)

| Address | Type | What |
|---|---|---|
| 0x800D1108 | struct[4] | `GwPlayer`, stride 0x38: stat u8 +0x04 (bit 0 = CPU), coin s16 +0x0A, star s8 +0x0E |
| 0x800CE200 | s32 | current overlay (omovl) |
| 0x800CD058 | | playMode (1 = Battle Royal) |
| 0x800CC15C | | file 1 name bytes |
| 0x800D09A9 | u8 | last file |
| 0x80105664 | s16 | board exit mask (0x20) |
| MBPauseCreate | func | pause |
| MBPlayerCoinAdd | func | (player, n) |

Overlays: 0x7A → 0x79 title, 0x77/0x78 Star Lift (`msetup`), `boardresult2` results.
