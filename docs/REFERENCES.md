# References

## In the project

- `ref\n64-sdk` libultra manual (`man-v5-1`) and `ultra` headers; `ref\n64-docs`, `ref\n64-demos`.
- `recomp\mpN\build\decomp\*.elf` the decomps' ELFs; `recomp\mpN\syms\` symbol names.
- `third_party\tools\marioparty-tools-1.1.1` (MP1 data compression, text), `partyplanner64-0.8.2` (board editor/viewer).
- `emu\project64-*` Project64 (dev build has a debugger).

## Worth adding

- N64Recomp, N64ModernRuntime and RecompFrontend docs (hook syntax, natives, the runtime's threading): the single-host design will need the runtime's internals.
- The Mario Party 1/2/3 decomp repos and wikis (struct and overlay names beyond `syms/`).
- Ghidra with an N64 loader, for functions the decomps haven't named.
- splat (the decomps' splitter) configs, for overlay ROM/VRAM ranges.
- Ares, as a debugging emulator with an accurate RDP, to compare against.
