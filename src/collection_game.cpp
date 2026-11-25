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
// The handoff file: %LOCALAPPDATA%\MarioPartyCollection\handoff.txt, key=value lines (version, session, target, mode,
// state, stars, coins, mp1). Windows only for now.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
    using Fields = std::map<std::string, std::string>;

    fs::path handoff_path() {
#ifdef _WIN32
        const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
        if (local != nullptr) {
            return fs::path(local) / L"MarioPartyCollection" / L"handoff.txt";
        }
#endif
        return {};
    }

    // "mp2" or "mp3", from this exe's name.
    std::string this_game() {
#ifdef _WIN32
        wchar_t buffer[MAX_PATH * 4];
        DWORD length = GetModuleFileNameW(nullptr, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
        std::wstring name = fs::path(std::wstring(buffer, length)).filename().wstring();
        if (name.find(L"MarioParty2") == 0) {
            return "mp2";
        }
        if (name.find(L"MarioParty3") == 0) {
            return "mp3";
        }
#endif
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

#ifdef _WIN32
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

extern "C" void recomp_collection_active(uint8_t* rdram, recomp_context* ctx) {
    check();
    ctx->r2 = active ? 1 : 0;
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
#ifdef _WIN32
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
    if (!start_exe(mp1)) {
        fields["state"] = "out";
        write_fields(handoff_path(), fields);
        return;
    }
    active = false;
    ctx->r2 = 1;
    ultramodern::quit();
#endif
}
