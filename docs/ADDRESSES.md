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
| 0x800F5460 | u16[4] | buttons pressed this frame |
| 0x800ED5C0 | struct | `GwSystem` (0x26; +0x1E the minigame on the rules screen) |
| 0x800F37F8 / 0x800C597E | | overlay history (8 bytes an entry) / its top |
| 0x800F09F4 | s32 | overlay omOvlGotoEx last went to |
| 0x800C5DF1 | u8 | window system set up (func_8006CEA0 sets, func_80070ED4 frees) |
| 0x8010F4EA | s16 | rules screen (0x6F): page (0 hints, 1 rules, 2-4 controls) |
| 0x8010F760 / 0x8010F772 | s16 | rules screen: minigame kind (0 one-player) / its player |
| 0x8010F764 | s16 | rules screen: 0 once Start is pressed |
| func_8006D010 | func | plain window (x, y, w, h, 0, 0): remove with func_80070D90, never func_80071E80/C8C (tail sprites) |

Overlays: 0x6F minigame rules (board path: omOvlCallEx(minigame) then HisChg(1, 0x7C) = coin results), 0x7B/0x7C/0x7D results (Stadium/board/Island). Minigames don't set up the window system.
Overlays: 0x61 opening, 0x69 Mushroom Village (`GameModeOverworld`), 0x6A Adventure setup, 0x81 title. Setup state
0xA = number of players, 0x26 = back to the village. Messages: 0x96 Toad's greeting, 0x97 "Choose the place…",
0x99 Warp Pipe two-option choice, 0x284 setup's first message, 0x26C Mini-Game Stadium's.

Text codes (MP1): 0x0B start, 0x0C…0x0D a choice, 0x0A newline, 0x10 space, 0xFF wait for A, 0x85 period,
0xC3 question mark. Buttons (PartyPlanner64's MP1 map): 0x21 A, 0x22 B, 0x23 C-up, 0x24 C-right, 0x25 C-left, 0x26 C-down, 0x27 Z, 0x28 stick, 0x2B Start, 0x2C R; colours 0x02 default, 0x03 red, 0x05 green, 0x06 blue; 0x3D hyphen. `tools\dump_mp1_messages.ps1 -Ids 0x99,0x284` prints messages from the ROM.

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
| 0x800D1108 | struct[4] | `GwPlayer`, stride 0x38: pad u8 +0x02 (controller), stat u8 +0x04 (bit 0 = CPU), coin s16 +0x0A, star s8 +0x0E. In turn order once the board has drawn it |
| 0x800C9520 | u16[4] | buttons pressed this frame |
| 0x800D5546 | u16[4] | buttons held (L 0x20, R 0x10) |
| 0x800CDD58 | s16 | `mbCameraBtnF`: 1 only while the dice block waits (with 0x800CDD64) |
| 0x800CDD64 | s16 | items allowed (B) at the dice block |
| 0x800D037C | s16 | `mbItemBtnF` |
| 0x80105556 | s16 | pause quit flag (`MBPauseCreate` leaves the board when set) |
| 0x800D6A48 | s16 | game over noted (set with exit mask 0x20) |
| 0x80105666 | s16 | exit watcher: fade the music |
| func_800FF158_shared_board_00112D78 | func | board pause watcher (Start/B/R/Z) |
| func_800F93A4_shared_board | func | board exit watcher (mask 0x20 → MBOvlCall(0x4F, 0, 0x4190)) |
| MBPauseQuitMesCreate_00103A78 | func | (message or string pointer, y): window sized to the text |
| func_8005E1D8_main | func | choice (window, 0, 0); func_8005FE54_main(window, 1 << port) limits it to one controller |
| 0x800CE200 | s32 | current overlay (omovl) |
| 0x800CD058 | | playMode (1 = Battle Royal) |
| 0x800CC15C | | file 1 name bytes |
| 0x800D09A9 | u8 | last file |
| 0x80105664 | s16 | board exit mask (0x20) |
| MBPauseCreate | func | pause |
| MBPlayerCoinAdd | func | (player, n) |

Overlays: 0x7A → 0x79 title, 0x77/0x78 Star Lift (`msetup`), 0x4F end of game, `boardresult2` results.

Star Lift (msetup) messages, through `func_8010B82C_msetup` (sets Tumble's window 0x8011A890's message) and
`func_8010B858_msetup` (waits while `func_8005E0C0` says the window is busy): 0x1500 "Welcome! This Star Lift goes to
the Battle Royal Maps!" (first visit only, game flag 0x1A, `func_80107FB4_msetup` 0x8010810C), 0x1502 "I'm Tumble…" +
"OK! It's time to set up the game." (two pages), 0x1503 "How many will play this time?", 0x1504 the player count
picker's caption, 0x1508 "1 Player and 3 Computer Characters", 0x1509 "Choose which character(s)…", 0x150D "Choose
which 3 characters the computer will control.", 0x150E/0x1512 the characters' lines. A `collection_trace` native
logging a hook's argument to stderr found these (removed again; easy to add back for other screens).

Text (as MP2): 0x0B start, ASCII with 0x20 space, 0x0A newline, 0x0C…0x0D a choice, null end. Messages are compressed
in the ROM (English table 0x121CAA0); a string pointer above 0x80000000 works wherever a message ID does.
