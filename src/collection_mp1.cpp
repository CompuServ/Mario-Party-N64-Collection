// Mario Party N64 Collection: the handoff between Mario Party 1 and the Mario Party 2 and 3 recomps (the collection's
// docs/handoff-design.md says how the three fit together; recomp/mp1's CMakeLists.txt compiles this file from the
// collection's src/). The patches
// (patches/collection.c) call these natives:
//
//   recomp_collection_launch(game): the Party Mode menu's Mario Party 2 or 3. Writes the handoff file (state=out), starts
//     that game's recomp with --boot and quits this one. 0 if it couldn't (no path for it, or it didn't start): the
//     game then carries on as Mario Party 1.
//   recomp_collection_take_return(totals): at boot. 1 when the other game has returned at the end of a party
//     (state=returned), with its human players' stars and coins written to totals[0] and totals[1] (two s32s in RDRAM);
//     2 when the player quit it from its pause menu (state=quit: back to the village, nothing added or saved); 3 for its
//     debug menu's Reset (state=reset: Mario Party 1 starts as usual, its logos and title, nothing added); 0 otherwise.
//   recomp_collection_return_applied(): once the totals are in this game's save; deletes the handoff file, so they're
//     added once.
//
// The handoff file is %LOCALAPPDATA%\MarioPartyCollection\handoff.txt, key=value lines (version, session, target,
// state, stars, coins, mp1: this exe, for the other game to start again). Where the other games' exes are comes from
// collection.ini beside this exe (mp2=<path>, mp3=<path>), which the collection's packaging writes. Windows only for
// now; elsewhere launch returns 0 and no return is ever found.

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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "collection_window.hpp"
#endif

namespace fs = std::filesystem;

namespace {
    using Fields = std::map<std::string, std::string>;

    fs::path collection_folder() {
#ifdef _WIN32
        const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
        if (local != nullptr) {
            return fs::path(local) / L"MarioPartyCollection";
        }
#endif
        return {};
    }

    fs::path handoff_path() {
        fs::path folder = collection_folder();
        return folder.empty() ? fs::path() : folder / "handoff.txt";
    }

    fs::path own_exe() {
#ifdef _WIN32
        wchar_t buffer[MAX_PATH * 4];
        DWORD length = GetModuleFileNameW(nullptr, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
        if (length > 0) {
            return fs::path(std::wstring(buffer, length));
        }
#endif
        return {};
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

#ifdef _WIN32
    // The exe with --boot, from its own folder (where its assets are).
    bool start_exe(const fs::path& exe) {
        std::wstring command = L"\"" + exe.wstring() + L"\" --boot";
        std::wstring folder = exe.parent_path().wstring();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup,
                            &process)) {
            fprintf(stderr, "collection: couldn't start %ls (error %lu)\n", exe.c_str(), GetLastError());
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }
#endif
}

extern "C" void recomp_collection_launch(uint8_t* rdram, recomp_context* ctx) {
    int32_t game = (int32_t)ctx->r4;
    ctx->r2 = 0;
#ifdef _WIN32
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
    fs::path target = fs::path(std::u8string(ini[key].begin(), ini[key].end()));
    // A relative path is from this exe's folder, so a build folder can be moved or renamed.
    if (target.is_relative()) {
        target = exe_path.parent_path() / target;
    }
    if (!fs::exists(target)) {
        fprintf(stderr, "collection: %s not found\n", ini[key].c_str());
        return;
    }

    Fields fields;
    fields["version"] = "1";
    fields["session"] = new_session();
    fields["target"] = key;
    // What the other game starts in: only Party Mode so far (other modes later, e.g. from the Mini-Game Island raft).
    fields["mode"] = "party";
    fields["state"] = "out";
    fields["stars"] = "0";
    fields["coins"] = "0";
    std::u8string exe_utf8 = exe_path.u8string();
    fields["mp1"] = std::string(exe_utf8.begin(), exe_utf8.end());
    if (!write_fields(handoff, fields)) {
        fprintf(stderr, "collection: couldn't write %ls\n", handoff.c_str());
        return;
    }
    // The window mode kept for the next game, and the launcher's black backdrop up (collection_window.hpp).
    collection_window::before_handover();
    if (!start_exe(target)) {
        std::error_code ec;
        fs::remove(handoff, ec);
        return;
    }
    ctx->r2 = 1;
    ultramodern::quit();
#endif
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
