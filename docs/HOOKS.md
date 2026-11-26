# Hooks and patches

Each recomp's changes live in its `collection` branch: `us.toml` (the section headed "Mario Party N64 Collection"),
`patches/collection.c` (MIPS C), two or three natives at the end of `patches/syms.ld` (dummy addresses 0x8F0000xx),
and `CMakeLists.txt` (compiles the collection's `src\` file). The natives' C++ is in the collection's `src\`:
`collection_mp1.cpp` (MP1) and `collection_game.cpp` (MP2 and MP3 both compile this one copy).

## How a hook is written

```toml
[[patches.hook]]
func = "func_800F731C_GameModeOverworld"
before_vram = 0x800F775C
text = "ctx->r5 = 0;"
```

`text` is C inserted into the generated code. To call a patch function and keep the registers:
`recomp_context saved=*ctx; f(rdram,ctx); gpr r=ctx->r2; *ctx=saved;`. Branch targets exist as labels
(`L_80106BF0`), so hook text can `goto` them. A native is `extern "C" void name(uint8_t* rdram, recomp_context* ctx)`.

## Mario Party 1

| Function | Before | What |
|---|---|---|
| func_800F731C_GameModeOverworld | 0x800F775C | Warp Pipe choice defaults to Depart (a1 = 0) |
| func_800F731C_GameModeOverworld | 0x800F7454 | 0x96/0x97 → empty message, records the window |
| func_800F731C_GameModeOverworld | 0x800F7508 | last-shown line −1, clears the village window |
| func_8006F9B0 | 0x8006FB60 | one scripted A (0x8000) in the text window's update |
| func_800F72E0_AdventureModeSetup | 0x800F77F4 | 0x284 → "Pick a Mario Party." menu text |
| func_800F72E0_AdventureModeSetup | 0x800F781C | runs the Party menu (`collection_run_menu`) |
| func_800F72E0_AdventureModeSetup | 0x800F7928 | B on the number of players → back to the menu |
| func_800F72E0_AdventureModeSetup | 0x800F86A0 | `collection_setup_frame` (fade + launch) |
| func_8001A0F0 | 0x8001A304 | boot: take the return, add totals, save (func_8005B244) |
| func_800F6704_LogosSequence | 0x800F6718 | Start counts as pressed on a return |
| func_800F6774_LogosSequence | 0x800F67B8 | omOvlCallEx(0x61) → 0x69 on a return |
| func_8005835C | 0x800583B4 | debug menu hold check (pause watcher) |
| func_8005835C | 0x8005844C | R suppressed while the menu is held |
| func_800F67B0_MushroomBank | 0x800F6874 | bank stars ";%5d" |
| func_8005A4C0 | 0x8005A73C | Item List stars "%4d" |
| func_8005835C | 0x80058394 | debug menu L+R count, every watcher frame (banner too) |
| func_800F6610_MinigameInstructions | 0x800F6610 | practice: restore players/system after a practice, disarm, allowed? |
| func_800F7C58_MinigameInstructions | 0x800F7FD0 | header process (0x800F84B0) → practice labels |
| func_800F7C58_MinigameInstructions | 0x800F808C | practice: R arms/disarms (C buttons masked while armed) |
| func_800F7C58_MinigameInstructions | 0x800F82FC | practice: page shown taken as -1 → text redrawn |
| func_800F7C58_MinigameInstructions | 0x800F8314 | practice: armed → practice text, goto L_800F8440 (wait) |
| func_800F6990_MinigameInstructions | 0x800F6990 | practice: frees windows, omOvlCallEx(minigame, 0, 0x14) itself, return (every mode) |
| func_800F7C58_MinigameInstructions | 0x800F8044 | practice labels come in as the text window opens |
| omInitObjMan | 0x8005D174 | practice minigame: blinking PRACTICE R process |
| omOvlGotoEx | 0x8005E054 | practice minigame leaving: func_80070ED4 frees its window system; going to results 0x7C/0x7B: minigame losses zeroed |
| func_800FA3A4_ResultsScene / _ResultsEternalStar2 | 0x800FA584 | bank stars kept to 9,999 instead of 100 |
| DataRead | 0x800145B0 | ID with a game in the top byte (2 = MP2) served from the graphics cache |
| omOvlReturnEx | 0x8005DFB8 | skipped once a practice was quit with R (no second return) |

## Mario Party 2

| Function | Before | What |
|---|---|---|
| func_80102C88_37F938_name_5B | 0x80102E08 | Mario Land cursor on Horror Land |
| func_801068C4_383574_name_5B | 0x801069B0 | skip Toad's greeting / Rules Land (`goto L_80106BF0`) |
| func_801076B8_357CC8_name_51 | 0x80107714 | latch the human players' totals |
| func_801076B8_357CC8_name_51 | 0x80107794 | party over → back to MP1 |
| func_80066994_67594 | 0x800669EC | debug menu hold check (turn-start gate, 3 s by osGetCount) |
| func_80066994_67594 | 0x80066ADC | R suppressed |
| func_8005DA20_5E620 | 0x8005DB1C | pause's menu process → `collection_debug_process` |
| func_8005DA20_5E620 | 0x8005DB90 | pause's quit branch: End Party / Reset / Quit Game |

## Mario Party 3

The survey's hooks come first in `us.toml`; the collection's title redirect is chained into the survey's
`omOvlGotoEx` hook text (0x80048228): `collection_title_goto`.

| Function | Before | What |
|---|---|---|
| omOvlGotoEx | 0x80048228 | 0x7A → 0x79 becomes file 1 "Mario" + Star Lift (0x78) |
| func_801096AC_boardresult2 | 0x801096DC | party over → back to MP1 |
| MBPauseCreate | 0x800F20E0 | pause quit branch: `collection_pause_quit` (Quit Game / debug Reset → MP1; End Party queues 0x4F, a0 = 1) |
| MBPauseCreate | 0x800F1FB4 | omAddPrcObj(MBPauseMain) → `collection_debug_process` while the debug menu opens |
| func_800FF158_shared_board_00112D78 | 0x800FF1B0 | debug menu hold check (pause watcher); opened → `goto L_800FF178` |
| func_800FF158_shared_board_00112D78 | 0x800FF2D0 | R (map) suppressed while L is held |
| func_80105DB4_msetup | 0x80105E08 | B on the Star Lift → back to MP1 |
| func_8010B82C_msetup | 0x8010B82C | Tumble's message helper: skip 0x1500 / 0x1502 in collection mode (`collection_lift_message`) |
| func_8010B858_msetup | 0x8010B858 | Tumble's wait helper: returns 0 after a skipped message (`collection_lift_wait`) |

## Natives (end of patches/syms.ld)

| Game | Addresses | Natives |
|---|---|---|
| MP1 | 0x8F00003C–0x8F00004C | recomp_collection_launch, recomp_collection_take_return, recomp_collection_return_applied, recomp_collection_cache_size, recomp_collection_cache_read |
| MP2 | 0x8F000040, 0x8F000044 | recomp_collection_active, recomp_collection_return |
| MP3 | 0x8F00002C, 0x8F000030 | recomp_collection_active, recomp_collection_return |
