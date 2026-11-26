// Mario Party N64 Collection: the graphics cache. Pictures one Mario Party shows that another reuses (Mario Party 1's
// minigame rules screen wears Mario Party 2's GAME RULES / NEXT / PRACTICE pills), pulled out of the player's own
// stored ROMs and kept, unpacked, in %LOCALAPPDATA%\MarioPartyCollection\cache\. Nothing from it is ever committed or
// shipped: it is made on the player's machine from the dumps they gave each recomp.
//
// build(): at start (Mario Party 1's static initializer). For each game whose files are listed, if its stored ROM is
// there and is the USA release, and the cache doesn't already hold every listed file from that same ROM (manifest.txt:
// its size and the two CRCs of its header), each file is read from the ROM's main file system and unpacked to
// <game>_<file id>.bin. A game whose ROM isn't stored yet is skipped; its files are simply missing, and the game that
// wants them falls back to its own look.
//
// The main file system (PartyPlanner64's "mainfs"): u32 directory count then offsets, each directory a u32 file count
// then offsets; a file is u32 unpacked size, u32 packing, then its data (12 bytes in for packings 2-4, else 8). Packing
// 0 stored, 1 LZSS (1 KB window from 0x3BE, flag bits low first), 2-4 Hudson's LZ (32-bit flag words), 5 run-length.
// ROMs are big-endian (.z64), as the recomps store them.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace collection_cache {
    namespace fs = std::filesystem;

    struct Game {
        const char* key;          // file prefix and manifest key
        const wchar_t* rom;       // the stored ROM, under %LOCALAPPDATA%
        const char* name;         // the ROM header's name (0x20), as a check
        uint32_t mainfs;          // the main file system's offset in the USA ROM
        std::vector<uint32_t> files;
    };

    // The files reused, by game. Mario Party 2's minigame rules screen (overlay 0x5F): 0xB0013 the empty pill, 0xB0014
    // GAME RULES, 0xB0017 ADVICE, 0xB0018 CONTROLS, 0xB001A PRACTICE (text laid on the pill), 0xB0021 NEXT, 0xB0022
    // PRACTICE R, 0xB0023 QUIT R (whole pills); and 0x900FB, the QUIT PRACTICE R banner its minigames show while practicing.
    inline std::vector<Game> games() {
        return {
            { "mp2", L"MarioParty2Recompiled\\marioparty2.n64.us.1.0.z64", "MarioParty2", 0x41DD30,
              { 0xB0013, 0xB0014, 0xB0017, 0xB0018, 0xB001A, 0xB0021, 0xB0022, 0xB0023, 0x900FB } },
        };
    }

    inline fs::path local_app_data() {
#ifdef _WIN32
        const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
        if (local != nullptr) {
            return fs::path(local);
        }
#endif
        return {};
    }

    inline fs::path folder() {
        fs::path local = local_app_data();
        return local.empty() ? fs::path() : local / L"MarioPartyCollection" / L"cache";
    }

    inline fs::path file_path(const char* game, uint32_t id) {
        char name[32];
        snprintf(name, sizeof(name), "%s_%06X.bin", game, id);
        return folder() / name;
    }

    inline uint32_t be32(const std::vector<uint8_t>& b, size_t at) {
        if (at + 4 > b.size()) {
            return 0;
        }
        return ((uint32_t)b[at] << 24) | ((uint32_t)b[at + 1] << 16) | ((uint32_t)b[at + 2] << 8) | b[at + 3];
    }

    // One file of the main file system, unpacked; empty if it can't be read.
    inline std::vector<uint8_t> extract(const std::vector<uint8_t>& rom, uint32_t mainfs, uint32_t id) {
        std::vector<uint8_t> out;
        uint32_t dir = id >> 16;
        uint32_t file = id & 0xFFFF;
        if (dir >= be32(rom, mainfs)) {
            return out;
        }
        size_t dir_at = mainfs + be32(rom, mainfs + 4 * (1 + dir));
        if (file >= be32(rom, dir_at)) {
            return out;
        }
        size_t at = dir_at + be32(rom, dir_at + 4 * (1 + file));
        uint32_t size = be32(rom, at);
        uint32_t packing = be32(rom, at + 4);
        if (size == 0 || size > 0x100000) {
            return out;
        }
        size_t src = at + ((packing >= 2 && packing <= 4) ? 12 : 8);
        auto byte = [&](size_t i) -> uint8_t { return i < rom.size() ? rom[i] : 0; };
        out.reserve(size);
        if (packing == 0) {
            for (uint32_t i = 0; i < size; i++) {
                out.push_back(byte(src + i));
            }
        } else if (packing == 1) {
            uint8_t window[1024] = {};
            uint32_t pos = 0x3BE;
            uint32_t flags = 0;
            while (out.size() < size && src < rom.size()) {
                if (!(flags & 0x100)) {
                    flags = byte(src++) | 0xFF00;
                }
                if (flags & 1) {
                    uint8_t c = byte(src++);
                    out.push_back(c);
                    window[pos] = c;
                    pos = (pos + 1) & 0x3FF;
                } else {
                    uint8_t b1 = byte(src);
                    uint8_t b2 = byte(src + 1);
                    src += 2;
                    uint32_t from = ((b2 & 0xC0) << 2) | b1;
                    uint32_t count = (b2 & 0x3F) + 3;
                    for (uint32_t i = 0; i < count && out.size() < size; i++) {
                        uint8_t c = window[(from + i) & 0x3FF];
                        window[pos] = c;
                        pos = (pos + 1) & 0x3FF;
                        out.push_back(c);
                    }
                }
                flags >>= 1;
            }
        } else if (packing >= 2 && packing <= 4) {
            uint32_t word = 0;
            int bits = 0;
            while (out.size() < size && src < rom.size()) {
                if (bits == 0) {
                    word = be32(rom, src);
                    src += 4;
                    bits = 32;
                }
                if (word & 0x80000000u) {
                    out.push_back(byte(src++));
                } else {
                    uint8_t b1 = byte(src);
                    uint8_t b2 = byte(src + 1);
                    src += 2;
                    size_t back = (((b1 & 0x0F) << 8) | b2) + 1;
                    size_t count = ((b1 & 0xF0) >> 4) + 2;
                    if (count == 2) {
                        count = byte(src++) + 0x12;
                    }
                    for (size_t i = 0; i < count && out.size() < size; i++) {
                        out.push_back(back > out.size() ? 0 : out[out.size() - back]);
                    }
                }
                word <<= 1;
                bits--;
            }
        } else if (packing == 5) {
            while (out.size() < size && src < rom.size()) {
                uint8_t c = byte(src++);
                uint32_t count = c & 0x7F;
                if (c & 0x80) {
                    for (uint32_t i = 0; i < count && out.size() < size; i++) {
                        out.push_back(byte(src++));
                    }
                } else {
                    uint8_t v = byte(src++);
                    for (uint32_t i = 0; i < count && out.size() < size; i++) {
                        out.push_back(v);
                    }
                }
            }
        }
        if (out.size() != size) {
            out.clear();
        }
        return out;
    }

    inline std::map<std::string, std::string> read_manifest() {
        std::map<std::string, std::string> fields;
        std::ifstream file(folder() / "manifest.txt");
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                fields[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
        return fields;
    }

    inline void write_manifest(const std::map<std::string, std::string>& fields) {
        std::ofstream file(folder() / "manifest.txt", std::ios::trunc);
        file << "# Mario Party N64 Collection graphics cache: made from the stored ROMs; delete the folder to remake it.\n";
        for (const auto& [key, value] : fields) {
            file << key << '=' << value << '\n';
        }
    }

    // Makes or refreshes the cache; quick when it is already whole (a size check and the ROM header's CRCs).
    inline void build() {
        fs::path dir = folder();
        fs::path local = local_app_data();
        if (dir.empty() || local.empty()) {
            return;
        }
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::map<std::string, std::string> manifest = read_manifest();
        bool changed = false;
        for (const Game& game : games()) {
            fs::path rom_path = local / game.rom;
            if (!fs::exists(rom_path, ec)) {
                continue;
            }
            std::vector<uint8_t> header(0x40);
            {
                std::ifstream rom_file(rom_path, std::ios::binary);
                rom_file.read((char*)header.data(), header.size());
                if (!rom_file) {
                    continue;
                }
            }
            // The USA release (region 'E' at 0x3E) of that game, in big-endian order (0x80 first).
            if (header[0] != 0x80 || header[0x3E] != 'E' ||
                memcmp(header.data() + 0x20, game.name, strlen(game.name)) != 0) {
                continue;
            }
            char stamp[64];
            snprintf(stamp, sizeof(stamp), "%llu,%08X,%08X", (unsigned long long)fs::file_size(rom_path, ec),
                     be32(header, 0x10), be32(header, 0x14));
            bool whole = manifest[game.key] == stamp;
            for (uint32_t id : game.files) {
                if (!fs::exists(file_path(game.key, id), ec)) {
                    whole = false;
                }
            }
            if (whole) {
                continue;
            }
            std::ifstream rom_file(rom_path, std::ios::binary);
            std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rom_file)), std::istreambuf_iterator<char>());
            for (uint32_t id : game.files) {
                std::vector<uint8_t> data = extract(rom, game.mainfs, id);
                if (data.empty()) {
                    fprintf(stderr, "collection cache: %s file %06X not read\n", game.key, id);
                    continue;
                }
                std::ofstream out(file_path(game.key, id), std::ios::binary | std::ios::trunc);
                out.write((const char*)data.data(), data.size());
            }
            manifest[game.key] = stamp;
            changed = true;
        }
        if (changed) {
            manifest["version"] = "1";
            write_manifest(manifest);
        }
    }

    // A cached file's bytes; empty if it isn't there.
    inline std::vector<uint8_t> load(const char* game, uint32_t id) {
        std::ifstream file(file_path(game, id), std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
}
