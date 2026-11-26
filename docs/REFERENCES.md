# References

## Decomps (already in the recomps)

`recomp\mp1\lib\marioparty`, `recomp\mp2\lib\marioparty2`, `recomp\mp3\lib\marioparty3`: the decomps' source, as
submodules. Struct, overlay and function names beyond `syms\`. `recomp\mpN\build\decomp\*.elf` is each one's built
ELF. `lib\mario-party-engine` is the recomps' shared engine patches.

## Recomp internals (third_party\src)

Upstream source for reading, not for building: each recomp builds the copy pinned in its own `lib\` submodule
(N64ModernRuntime cdf5abb, RecompFrontend b2c5c18, rt64 5473732 in all three), and these are newer.

| Folder | Commit | For |
|---|---|---|
| `n64recomp-05-04-2025` | 598ce20 (2026-11-03) | hook syntax, `us.toml` options, how natives and patches link |
| `n64modernruntime` | e4342ae (2026-11-03) | ultramodern threads, RDRAM, `quit()`: the single-host design |
| `recompfrontend` | 2ddbfc8 (2026-10-16) | the launcher, ROM storage, `--boot`, config |
| `rt64` | d9817c6 (2026-10-23) | the renderer, shared by the single host |
| `splat` | 7548b97 (2026-10-10) | the decomps' splitter: overlay ROM/VRAM ranges from their yaml |
| `n64loaderwv-12.2` | 44a59c8 | Ghidra N64 loader source |

`third_party\tools\n64recomp-05-04-2025\RecompModTool.exe`: N64Recomp's mod tool (not used by the alpha).

## Tools

- **Ghidra 12.2** (`third_party\tools\ghidra_12.2_PUBLIC`) with **N64LoaderWV 12.2** (`third_party\tools\n64loaderwv-12.2`, install through Ghidra's File > Install Extensions). Ghidra 12 needs a 64-bit JDK 21 or newer; this PC has only Java 8, so install one (e.g. Eclipse Temurin 21) first. Load a decomp's ELF rather than the raw ROM where possible: it has the symbols.
- **splat** runs on Python 3.9+. Windows has only the Store's python stub; use WSL's python3 (the decomp builds are in WSL anyway).
- **ares 148** (`emu\ares-148-release`, source `emu\ares-148-src`): accurate N64 emulation with a GDB server. `tools\ares_gdb.ps1` reads and writes memory through it (`docs\TESTING.md`).
- **Project64** (`emu\project64-*`): the dev build has a debugger with memory search.
- **PartyPlanner64 0.8.2**, **Mario Party Tools 1.1.1** (`third_party\`): boards, MP1 data compression, text.
- `ref\n64-sdk` (libultra manual `man-v5-1`, `ultra` headers), `ref\n64-docs`, `ref\n64-demos`.

ares saves its EEPROMs beside the ROMs (`roms\*.eeprom`): those are ares's, separate from the recomps' saves.

## Local references (docs\references, not committed)

Super Mario Wiki pages saved as PDF: Mario Party 1, 2, 3, the series, boards, and each game's minigame list (what
each minigame does, its kind and coin rules). Also screenshots of Mario Party 2's rules screen, practice armed and not.
Kept out of git (`.gitignore`): they hold the games' pictures.
