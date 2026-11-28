# Mario Party N64 Collection

Mario Party 1, 2 and 3 in one compilation for Windows (first) and Linux, built from three N64Recomp-based recompilations. Players supply their own ROMs; nothing ROM-derived is ever committed or shipped. The full plan is in the claude.ai Project "Mario Party Compilation Game", doc `collection-plan.md`.

## Layout

- `src/` the collection's natives: `collection_platform.hpp` (what differs by system: data folder, own program, starting a game; Windows, Linux, macOS), `collection_mp1.cpp` (MP1), `collection_game.cpp` (MP2 and MP3 compile this one copy; never copy it into a recomp), `collection_window.hpp` (both: window mode kept, launcher told at a handover). `src/launcher/` the launcher `MarioPartyCollection.exe` (`tools\build_launcher.cmd`; the way to start the collection: black backdrop between games).
- `tools/` `build_recomp.cmd`, `package_build.ps1`, `drive_game.ps1`, `ares_gdb.ps1` (memory through ares's GDB server, port 9123), `dump_mp1_messages.ps1`, `dump_mp2_messages.ps1`.
- `docs/` `BUILDS.md` (history, what's tested, next), `ADDRESSES.md`, `HOOKS.md`, `TESTING.md`, `REFERENCES.md`, `handoff-design.md`, `milestones.txt`.
- `recomp/mp1|mp2|mp3` the recomp repos (submodules initialised; the collection's changes on branch `collection`): `us.toml` (section "Mario Party N64 Collection"), `patches/collection.c`, natives at the end of `patches/syms.ld`, `CMakeLists.txt`. `build/decomp/*.elf` is the decomp's ELF (reference only).
- `bin/build-NNN` packaged test builds (`Mario Party Collection.exe`, `MP1`, `MP2`, `MP3`) (last 10 + `docs/milestones.txt`). `prebuilt/` upstream release exes. `roms/` the user's dumps. `ref/` N64 SDK and docs (reference only). `emu/` ares 148 (GDB server) and Project64. `third_party/` upstream N64Recomp, N64ModernRuntime, RecompFrontend, rt64, splat source (reading only: builds use each recomp's pinned `lib/` submodules), Ghidra 12.2 + N64LoaderWV, Mario Party Tools, PartyPlanner64. The decomps' source is in each recomp's `lib/marioparty*`. See `docs/REFERENCES.md`.
- Git: the root repo tracks `CLAUDE.md`, `src/`, `tools/`, `docs/` only (`.gitignore` excludes the rest). Each recomp commits only the collection's files on `collection`.

## Building

- `tools\build_recomp.cmd <1|2|3> [full|configure]`: `full` after a `us.toml` change, nothing for `patches/` or `src/` changes. `NINJA_JOBS` defaults to 12 (more ran this PC out of memory).
- Toolchain: VS 2026 Community (`vcvars64`), LLVM 23 clang-cl and `ld.lld` (`C:\Program Files\LLVM\bin`), MSYS2 `clang64\bin\clang.exe` for `patches/` (official LLVM has no MIPS backend; MSYS2's `ld.lld` can't link them), CMake 3.31, Ninja, Git, MSYS2 `usr\bin\make`.
- Submodules use `core.autocrlf=false` (rt64's patches fail on CRLF) and `core.longpaths=true`. For WSL scripts: `sed -i 's/\r$//' tools/*.sh tools/docker/Dockerfile.decomp`.
- Every build that changes code: `tools\package_build.ps1 -Notes "..."` → next `bin\build-NNN`, and a row in `docs\BUILDS.md`. Builds are local testing only, never shared or committed.
- `build\*.exe` runs only from its repo root (needs `assets\`).

## Testing

Back up saves (`%LOCALAPPDATA%\MarioPartyRecompiled\`) before anything that writes MP1's save. Incremental builds: the survey. Milestones: full scripted loop with `drive_game.ps1`. See `docs\TESTING.md`.

## Decisions

- Linux: the collection code is portable (collection_platform.hpp); Linux packages are built only on request (about every other milestone), x86-64 through each recomp's tools/release.sh linux (Docker); ARM later.
- Final design: one host program (window, RT64, audio, input) with MP1/MP2/MP3 as separate modules, each its own runtime and RDRAM. **Alpha:** switch programs (MP1 launches MP2/MP3 with `--boot` and exits; they relaunch MP1) through `%LOCALAPPDATA%\MarioPartyCollection\handoff.txt` (`docs\handoff-design.md`).
- Totals flow one way: at Start on the last results page, MP2/MP3 send their human (non-CPU) players' star and coin sums; MP1 adds them at boot and saves only through its own save routine (EEPROM checksum). Limits: stars 0–9,999, coins 0–99,999, excess dropped. MP2/MP3 save normally. Quit Game → village, nothing added; debug Reset → MP1 title.
- Settings: MP1's are the master copy (`%LOCALAPPDATA%\MarioPartyRecompiled\` general/graphics/sound/controls.json). MP2/MP3 copy them in at start and copy their own changes back (`src/collection_game.cpp`); saves, stored ROMs and mods stay per game.
- MP2 in collection mode skips Toad's greeting and the Rules Land tutorial. MP3 in collection mode opens the Star Lift on "How many will play this time?". Backing out goes to MP1's village: MP2's warp pipe Yes, B on its board select (Toad asks), MP3's Star Lift B (Tumble asks "Go back to the Mushroom Village?"). From the raft: MP2's Coaster (Leave asks "Go back to the Mushroom Village?", Yes default; every way out of the Coaster, its end included) and MP3's Story Mode map (B asks the same, Yes default, no goodbye) go back to the village. MP1 → MP1 adds no transition; MP2/MP3 picks fade out with MP1's wipe and music fade first.
- Graphics cache: `src/collection_cache.hpp` pulls pictures one game reuses from another's stored ROM into `%LOCALAPPDATA%\MarioPartyCollection\cache\` at MP1's start (never committed or shipped); MP1's DataRead serves IDs 0x02xxxxxx (MP2) from it. MP1's practice uses MP2's pills and banner, falling back to its own text windows.
- Minigames never take coins in MP1 (results get no losses; winners keep their own prizes, Crane Game included). Coin battles (Crane Game, Bash 'n' Cash, Grab Bag) give at most 30 a player. A computer's 1-player minigame (board or Mini-Game Stadium) is skipped to the results (10 coins at 50/75/90% by skill). Message speed starts Fast in each new party (all three games). Every Mushroom Shop item is for sale from the start. The village Start screens have a third page, Mario Party 1 Records (the saved minigame times), and a fourth, Mario Party 2 Records (read from MP2's save each time the pages open, never copied into MP1's). A real minigame starts only once every human has pressed Start on its rules screen (MP2's START/PRESS button above each player; computers follow); practice still starts on one press. Bowser, Chance Time and Boo still take coins.
- Debug menu per game: L+R ≥ 3 s on the board map only (never minigames, their instructions or board events), the game's own window and font: End Party Mode Immediately, Add 20 Coins to P1, Max Coins P1, Max Stars P1, Reset last. Opened through the game's own pause so the turn freezes; a hold on the player's start banner arms it for the dice block. MP1, MP2, MP3 done.

## Status

See `docs\BUILDS.md`.
