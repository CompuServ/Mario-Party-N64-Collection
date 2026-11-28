// Mario Party N64 Collection: the Mario Party 2 / Mario Party 3 side of the handoff with Mario Party 1. Both recomps'
// builds compile this one copy (their CMakeLists.txt name ../../src/collection_game.cpp; docs/handoff-design.md says how
// the games fit together).
// Which game this is comes from the exe's name (MarioParty2Recompiled / MarioParty3Recompiled).
//
// The patches (patches/collection.c) call these natives:
//
//   recomp_collection_active(): 1 when Mario Party 1 started this game for a party (the handoff file says state=out,
//     target=this game), else 0, and the game plays as usual. Read once, at the first call.
//   recomp_collection_return(kind, stars, coins): back to Mario Party 1. kind 1: the party ended, its human players'
//     stars and coins go into Mario Party 1's save (state=returned); kind 0: the player quit, nothing is added, to the
//     village (state=quit); kind 2: the debug menu's Reset, nothing added, to Mario Party 1's title (state=reset). Writes the handoff file, starts Mario Party 1 (its exe from the file's mp1=) with --boot and quits
//     this game. 0 if it couldn't (not started by the collection, or Mario Party 1 didn't start): the game carries on.
//
// The handoff file: MarioPartyCollection/handoff.txt in the data folder (%LOCALAPPDATA% on Windows, ~/.config on Linux,
// collection_platform.hpp), key=value lines (version, session, target, mode, state, stars, coins, mp1).
//
// Settings: Mario Party 1's are the collection's one copy. The settings menu's files (general.json, graphics.json,
// sound.json, controls.json: the same options in all three recomps) live in each game's own folder
// (<data folder>/<program name>, RecompFrontend's app folder), read once at start and written when changed. Before this
// game's main runs (a static object's constructor), Mario Party 1's (<data folder>/MarioPartyRecompiled) are copied
// over this game's, so it starts with them; then a thread copies any of this game's that changes back over Mario
// Party 1's within a second, and the return to Mario Party 1 copies them once more before starting it. Saves, the
// stored ROM and mods stay each game's own.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"

#include "collection_platform.hpp"
#include "collection_window.hpp"

namespace fs = std::filesystem;

namespace {
    using Fields = std::map<std::string, std::string>;

    fs::path handoff_path() {
        fs::path folder = collection_platform::collection_folder();
        return folder.empty() ? fs::path() : folder / "handoff.txt";
    }

    // "mp2" or "mp3", from this exe's name.
    std::string this_game() {
        std::string name = collection_platform::own_exe().filename().string();
        if (name.find("MarioParty2") == 0) {
            return "mp2";
        }
        if (name.find("MarioParty3") == 0) {
            return "mp3";
        }
        return {};
    }

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

    // The handoff as read at the first call: active when it's addressed to this game and still out.
    bool checked = false;
    bool active = false;
    Fields handoff;

    void check() {
        if (checked) {
            return;
        }
        checked = true;
        fs::path path = handoff_path();
        std::error_code ec;
        if (path.empty() || !fs::exists(path, ec)) {
            return;
        }
        handoff = read_fields(path);
        active = handoff["version"] == "1" && handoff["state"] == "out" && handoff["target"] == this_game();
        if (active) {
            fprintf(stderr, "collection: started by Mario Party 1 (session %s)\n", handoff["session"].c_str());
        }
    }

    // ---- Shared settings ----
    const char* const shared_settings[] = { "general.json", "graphics.json", "sound.json", "controls.json" };

    fs::path local_app_data() {
        return collection_platform::data_root();
    }

    // This game's settings folder (RecompFrontend's app folder: the exe's name) and Mario Party 1's.
    fs::path own_settings_folder() {
        fs::path exe = collection_platform::own_exe();
        fs::path local = local_app_data();
        if (!exe.empty() && !local.empty()) {
            return local / exe.stem();
        }
        return {};
    }

    fs::path master_settings_folder() {
        fs::path local = local_app_data();
        return local.empty() ? fs::path() : local / "MarioPartyRecompiled";
    }

    std::mutex settings_mutex;
    std::map<std::string, fs::file_time_type> settings_synced;

    fs::file_time_type write_time(const fs::path& path) {
        std::error_code ec;
        fs::file_time_type time = fs::last_write_time(path, ec);
        return ec ? fs::file_time_type::min() : time;
    }

    // This game's changed settings files over Mario Party 1's.
    void settings_to_master() {
        std::lock_guard<std::mutex> lock(settings_mutex);
        fs::path own = own_settings_folder();
        fs::path master = master_settings_folder();
        if (own.empty() || master.empty() || own == master) {
            return;
        }
        for (const char* name : shared_settings) {
            fs::path from = own / name;
            fs::file_time_type time = write_time(from);
            if (time == fs::file_time_type::min() || settings_synced[name] == time) {
                continue;
            }
            std::error_code ec;
            fs::create_directories(master, ec);
            fs::copy_file(from, master / name, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                settings_synced[name] = time;
            }
        }
    }

    // At start, before main: Mario Party 1's settings over this game's (this game's seed Mario Party 1's where it has
    // none yet), then the thread that sends this game's changes back.
    struct SharedSettings {
        SharedSettings() {
            fs::path own = own_settings_folder();
            fs::path master = master_settings_folder();
            if (own.empty() || master.empty() || own == master) {
                return;
            }
            std::error_code ec;
            fs::create_directories(own, ec);
            for (const char* name : shared_settings) {
                fs::path from = master / name;
                fs::path to = own / name;
                if (fs::exists(from, ec)) {
                    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
                } else if (fs::exists(to, ec)) {
                    fs::create_directories(master, ec);
                    fs::copy_file(to, from, fs::copy_options::overwrite_existing, ec);
                }
                settings_synced[name] = write_time(to);
            }
            std::thread([] {
                while (true) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    settings_to_master();
                }
            }).detach();
        }
    };
    SharedSettings shared_settings_at_start;

    using collection_platform::start_exe;
}

extern "C" void recomp_collection_active(uint8_t* rdram, recomp_context* ctx) {
    check();
    ctx->r2 = active ? 1 : 0;
}

// recomp_collection_mode(): what Mario Party 1 started this game for: 0 a party (mode=party), 1 its one-player mode
// (mode=coaster: Mario Party 2's Mini-Game Coaster; mode=story: Mario Party 3's Story Mode); -1 when not started by the
// collection.
extern "C" void recomp_collection_mode(uint8_t* rdram, recomp_context* ctx) {
    check();
    if (!active) {
        ctx->r2 = (gpr)(int32_t)-1;
        return;
    }
    const std::string& mode = handoff["mode"];
    ctx->r2 = (mode == "coaster" || mode == "story") ? 1 : 0;
}

extern "C" void recomp_collection_return(uint8_t* rdram, recomp_context* ctx) {
    int32_t kind = (int32_t)ctx->r4;
    int32_t stars = (int32_t)ctx->r5;
    int32_t coins = (int32_t)ctx->r6;
    ctx->r2 = 0;
    check();
    if (!active) {
        return;
    }
    std::string mp1_text = handoff["mp1"];
    fs::path mp1 = fs::path(std::u8string(mp1_text.begin(), mp1_text.end()));
    std::error_code ec;
    if (mp1.empty() || !fs::exists(mp1, ec)) {
        fprintf(stderr, "collection: Mario Party 1 not found at %s\n", mp1_text.c_str());
        return;
    }
    Fields fields = handoff;
    fields["state"] = kind == 1 ? "returned" : (kind == 2 ? "reset" : "quit");
    fields["stars"] = std::to_string(kind == 1 && stars > 0 ? stars : 0);
    fields["coins"] = std::to_string(kind == 1 && coins > 0 ? coins : 0);
    if (!write_fields(handoff_path(), fields)) {
        fprintf(stderr, "collection: couldn't write the handoff file\n");
        return;
    }
    // Mario Party 1 reads the settings as it starts: any change made here goes back first.
    settings_to_master();
    // The window mode kept for Mario Party 1, and the launcher's black backdrop up (collection_window.hpp).
    collection_window::before_handover();
    if (!start_exe(mp1)) {
        fields["state"] = "out";
        write_fields(handoff_path(), fields);
        return;
    }
    active = false;
    ctx->r2 = 1;
    ultramodern::quit();
}
