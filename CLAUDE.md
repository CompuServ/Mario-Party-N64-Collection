# Mario Party N64 Collection

Mario Party 1, 2 and 3 in one compilation for Windows (first) and Linux, built from three N64Recomp-based recompilations. Players supply their own ROMs; nothing ROM-derived is ever committed or shipped. The full plan is in the claude.ai Project "Mario Party Compilation Game", doc `collection-plan.md`.

## Layout

- `src/` the collection's natives: `collection_mp1.cpp` (MP1), `collection_game.cpp` (MP2 and MP3 compile this one copy; never copy it into a recomp).
- `tools/` `build_recomp.cmd`, `package_build.ps1`, `drive_game.ps1`, `ares_gdb.ps1` (memory through ares's GDB server, port 9123), `dump_mp1_messages.ps1`, `dump_mp2_messages.ps1`.
- `docs/` `BUILDS.md` (history, what's tested, next), `ADDRESSES.md`, `HOOKS.md`, `TESTING.md`, `REFERENCES.md`, `handoff-design.md`, `milestones.txt`.
- `recomp/mp1|mp2|mp3` the recomp repos (submodules initialised; the collection's changes on branch `collection`): `us.toml` (section "Mario Party N64 Collection"), `patches/collection.c`, natives at the end of `patches/syms.ld`, `CMakeLists.txt`. `build/decomp/*.elf` is the decomp's ELF (reference only).
- `bin/build-NNN` packaged test builds (last 10 + `docs/milestones.txt`). `prebuilt/` upstream release exes. `roms/` the user's dumps. `ref/` N64 SDK and docs (reference only). `emu/` ares 148 (GDB server) and Project64. `third_party/` upstream N64Recomp, N64ModernRuntime, RecompFrontend, rt64, splat source (reading only: builds use each recomp's pinned `lib/` submodules), Ghidra 12.2 + N64LoaderWV, Mario Party Tools, PartyPlanner64. The decomps' source is in each recomp's `lib/marioparty*`. See `docs/REFERENCES.md`.
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

- Final design: one host program (window, RT64, audio, input) with MP1/MP2/MP3 as separate modules, each its own runtime and RDRAM. **Alpha:** switch programs (MP1 launches MP2/MP3 with `--boot` and exits; they relaunch MP1) through `%LOCALAPPDATA%\MarioPartyCollection\handoff.txt` (`docs\handoff-design.md`).
- Totals flow one way: at Start on the last results page, MP2/MP3 send their human (non-CPU) players' star and coin sums; MP1 adds them at boot and saves only through its own save routine (EEPROM checksum). Limits: stars 0–9,999, coins 0–99,999, excess dropped. MP2/MP3 save normally. Quit Game → village, nothing added; debug Reset → MP1 title.
- MP2 in collection mode skips Toad's greeting and the Rules Land tutorial. MP1 → MP1 adds no transition; MP2/MP3 picks fade out with MP1's wipe and music fade first.
- Debug menu per game: L+R ≥ 3 s on the board map only (never minigames, their instructions or board events), the game's own window and font: End Party Mode Immediately, Add 20 Coins to P1, Max Coins P1, Max Stars P1, Reset last. Opened through the game's own pause so the turn freezes. MP1, MP2 done; MP3 next.

## Status

See `docs\BUILDS.md`.
