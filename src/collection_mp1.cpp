// Mario Party N64 Collection: the handoff between Mario Party 1 and the Mario Party 2 and 3 recomps (the collection's
// docs/handoff-design.md says how the three fit together; recomp/mp1's CMakeLists.txt compiles this file from the
// collection's src/). The patches
// (patches/collection.c) call these natives:
//
//   recomp_collection_launch(game, mode): the Party Mode menu's Mario Party 2 or 3 (mode 0, mode=party in the handoff),
//     or the raft's (mode 1: mode=coaster for Mario Party 2, mode=story for Mario Party 3). Writes the handoff file (state=out), starts
//     that game's recomp with --boot and quits this one. 0 if it couldn't (no path for it, or it didn't start): the
//     game then carries on as Mario Party 1.
//   recomp_collection_take_return(totals): at boot. 1 when the other game has returned at the end of a party
//     (state=returned), with its human players' stars and coins written to totals[0] and totals[1] (two s32s in RDRAM);
//     2 when the player quit it from its pause menu (state=quit: back to the village, nothing added or saved); 3 for its
//     debug menu's Reset (state=reset: Mario Party 1 starts as usual, its logos and title, nothing added); 0 otherwise.
//   recomp_collection_return_applied(): once the totals are in this game's save; deletes the handoff file, so they're
//     added once.
//
// The handoff file is MarioPartyCollection/handoff.txt in the data folder (%LOCALAPPDATA% on Windows, ~/.config on
// Linux: collection_platform.hpp), key=value lines (version, session, target, state, stars, coins, mp1: this program,
// for the other game to start again). Where the other games' programs are comes from collection.ini beside this one
// (mp2=<path>, mp3=<path>), which the collection's packaging writes.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"

#include "collection_platform.hpp"
#include "collection_window.hpp"
#include "collection_cache.hpp"

namespace fs = std::filesystem;

namespace {
    using Fields = std::map<std::string, std::string>;
    using collection_platform::collection_folder;
    using collection_platform::own_exe;
    using collection_platform::start_exe;

    fs::path handoff_path() {
        fs::path folder = collection_folder();
        return folder.empty() ? fs::path() : folder / "handoff.txt";
    }

    // key=value lines; anything else is skipped.
    Fields read_fields(const fs::path& path) {
        Fields fields;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            size_t eq = line.find('=');
            if (eq != std::string::npos && line[0] != '#') {
                fields[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
        return fields;
    }

    // Written whole to a temporary file, then renamed over the old one.
    bool write_fields(const fs::path& path, const Fields& fields) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        fs::path temp = path;
        temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::trunc);
            if (!file) {
                return false;
            }
            for (const auto& [key, value] : fields) {
                file << key << '=' << value << '\n';
            }
        }
        fs::rename(temp, path, ec);
        return !ec;
    }

    int32_t to_count(const Fields& fields, const char* key) {
        auto it = fields.find(key);
        if (it == fields.end()) {
            return 0;
        }
        long value = std::strtol(it->second.c_str(), nullptr, 10);
        return value < 0 ? 0 : (value > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)value);
    }

    std::string new_session() {
        std::random_device device;
        char text[17];
        snprintf(text, sizeof(text), "%08x%08x", device(), device());
        return text;
    }
}

extern "C" void recomp_collection_launch(uint8_t* rdram, recomp_context* ctx) {
    int32_t game = (int32_t)ctx->r4;
    // 0 a party; 1 that game's one-player mode (Mario Party 2's Mini-Game Coaster, Mario Party 3's Story Mode).
    int32_t mode = (int32_t)ctx->r5;
    ctx->r2 = 0;
    fs::path exe_path = own_exe();
    fs::path handoff = handoff_path();
    if (exe_path.empty() || handoff.empty() || (game != 2 && game != 3)) {
        return;
    }
    Fields ini = read_fields(exe_path.parent_path() / "collection.ini");
    std::string key = game == 2 ? "mp2" : "mp3";
    if (ini.count(key) == 0) {
        fprintf(stderr, "collection: no %s= in collection.ini\n", key.c_str());
        return;
    }
    // A relative path is from this program's folder, so a build folder can be moved or renamed.
    fs::path target = collection_platform::find_exe(exe_path.parent_path(), ini[key]);
    std::error_code exists_ec;
    if (!fs::exists(target, exists_ec)) {
        fprintf(stderr, "collection: %s not found\n", ini[key].c_str());
        return;
    }

    Fields fields;
    fields["version"] = "1";
    fields["session"] = new_session();
    fields["target"] = key;
    // What the other game starts in: only Party Mode so far (other modes later, e.g. from the Mini-Game Island raft).
    fields["mode"] = mode == 1 ? (game == 2 ? "coaster" : "story") : "party";
    fields["state"] = "out";
    fields["stars"] = "0";
    fields["coins"] = "0";
    std::u8string exe_utf8 = exe_path.u8string();
    fields["mp1"] = std::string(exe_utf8.begin(), exe_utf8.end());
    if (!write_fields(handoff, fields)) {
        fprintf(stderr, "collection: couldn't write %s\n", handoff.string().c_str());
        return;
    }
    // The window mode kept for the next game, and the launcher's black backdrop up (collection_window.hpp; Windows).
    collection_window::before_handover();
    if (!start_exe(target)) {
        std::error_code ec;
        fs::remove(handoff, ec);
        return;
    }
    ctx->r2 = 1;
    ultramodern::quit();
}

extern "C" void recomp_collection_take_return(uint8_t* rdram, recomp_context* ctx) {
    gpr totals = ctx->r4;
    ctx->r2 = 0;
    fs::path handoff = handoff_path();
    std::error_code ec;
    if (handoff.empty() || !fs::exists(handoff, ec)) {
        return;
    }
    Fields fields = read_fields(handoff);
    if (fields["version"] != "1") {
        return;
    }
    if (fields["state"] == "reset") {
        MEM_W(0, totals) = 0;
        MEM_W(4, totals) = 0;
        ctx->r2 = 3;
        return;
    }
    if (fields["state"] == "quit") {
        MEM_W(0, totals) = 0;
        MEM_W(4, totals) = 0;
        ctx->r2 = 2;
        return;
    }
    if (fields["state"] != "returned") {
        return;
    }
    MEM_W(0, totals) = to_count(fields, "stars");
    MEM_W(4, totals) = to_count(fields, "coins");
    ctx->r2 = 1;
}

extern "C" void recomp_collection_return_applied(uint8_t* rdram, recomp_context* ctx) {
    fs::path handoff = handoff_path();
    std::error_code ec;
    if (!handoff.empty()) {
        fs::remove(handoff, ec);
    }
}

// ---- The graphics cache (collection_cache.hpp) ----------------------------------------------------------------------
// Made or refreshed as Mario Party 1 starts, before its game does.
namespace {
    struct CacheAtStart {
        CacheAtStart() { collection_cache::build(); }
    } cache_at_start;

    // A cached file asked for by the patches as a file ID of their own: the game's number in the top byte (2 Mario Party
    // 2), the file's own ID (directory << 16 | file) below it. patches/collection.c's hook at DataRead serves these.
    std::vector<uint8_t> cached_file(uint32_t id) {
        uint32_t game = id >> 24;
        if (game == 2) {
            return collection_cache::load("mp2", id & 0xFFFFFF);
        }
        return {};
    }
}

// recomp_collection_cache_size(id): the cached file's size, 0 if it isn't cached.
extern "C" void recomp_collection_cache_size(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(int32_t)cached_file((uint32_t)ctx->r4).size();
}

// recomp_collection_cache_read(id, dest): the cached file's bytes written to dest in RDRAM; 1 when it was there.
extern "C" void recomp_collection_cache_read(uint8_t* rdram, recomp_context* ctx) {
    std::vector<uint8_t> data = cached_file((uint32_t)ctx->r4);
    gpr dest = ctx->r5;
    for (size_t i = 0; i < data.size(); i++) {
        MEM_B((int32_t)i, dest) = data[i];
    }
    ctx->r2 = data.empty() ? 0 : 1;
}

// ---- Mario Party 2's records, for Mario Party 1's Records page --------------------------------------------------------
// recomp_collection_mp2_records(out): read from Mario Party 2's own save each time (<data folder>/MarioParty2Recompiled/
// saves/marioparty2.n64.us.1.0.bin, its 4 Kbit EEPROM, big-endian: "HUDSON" at 0, its save block from 8 with version 5 at
// 8, the records as u16s from 0x0A (record i at 0x0A + 2i), and at 0x1F8 the low 16 bits of the sum of bytes 8-0x1F7).
// Writes Bobsled Run (1), Handcar Havoc (2), Sky Pilots (3) (frames at 60 a second), Mecha-Marathon (4: whole metres in
// the high byte, hundredths in the low; 0 never played, which Mario Party 2 shows as 20.00) and Slot Car Derby 1-3
// (5-7, frames at 30 a second) to out as (value, ticks a second) pairs, -1 ticks for a distance. Returns the row count
// (7), or 0 when there is no valid save.
extern "C" void recomp_collection_mp2_records(uint8_t* rdram, recomp_context* ctx) {
    gpr out = ctx->r4;
    ctx->r2 = 0;
    fs::path path = collection_platform::data_root() / "MarioParty2Recompiled" / "saves" / "marioparty2.n64.us.1.0.bin";
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> save((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (save.size() < 0x200 || memcmp(save.data(), "HUDSON", 6) != 0 || save[8] != 5) {
        return;
    }
    uint32_t sum = 0;
    for (size_t i = 8; i < 0x1F8; i++) {
        sum += save[i];
    }
    if ((sum & 0xFFFF) != (uint32_t)((save[0x1F8] << 8) | save[0x1F9])) {
        return;
    }
    static const int indices[7] = { 1, 2, 3, 4, 5, 6, 7 };
    static const int rates[7] = { 60, 60, 60, -1, 30, 30, 30 };
    for (int row = 0; row < 7; row++) {
        size_t at = 0x0A + 2 * indices[row];
        int32_t value = (save[at] << 8) | save[at + 1];
        if (rates[row] == -1 && value == 0) {
            value = 20 << 8;
        }
        MEM_W(row * 8, out) = value;
        MEM_W(row * 8 + 4, out) = rates[row];
    }
    ctx->r2 = 7;
}
