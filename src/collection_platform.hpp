// Mario Party N64 Collection: what differs by system, for the natives the three games compile (collection_mp1.cpp,
// collection_game.cpp, collection_cache.hpp).
//
//   data_root(): the folder each recomp keeps its own under (RecompFrontend's app folder, <data_root>/<program name>):
//     %LOCALAPPDATA% on Windows, ~/.config on Linux, ~/Library/Application Support on macOS. The collection's own files
//     (handoff.txt, the graphics cache) are in <data_root>/MarioPartyCollection.
//   own_exe(): this program's path.
//   start_exe(exe): another of the games started with --boot, from its own folder (where its assets are); false if it
//     couldn't be. On Linux and macOS a child of its own session (it outlives this one, which quits).
//   native_path(text): a path written in collection.ini, with either kind of slash, for this system (a Windows build's
//     "MP2\MarioParty2Recompiled.exe" is "MP2/MarioParty2Recompiled" elsewhere: without the .exe if that isn't there).
//
// A portable install (RecompFrontend's portable.txt beside the program) keeps its data beside the program instead;
// the collection doesn't follow that yet.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace collection_platform {
    namespace fs = std::filesystem;

    inline fs::path data_root() {
#ifdef _WIN32
        const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
        return local != nullptr ? fs::path(local) : fs::path();
#else
        const char* home = getenv("HOME");
        if (home == nullptr) {
            struct passwd* user = getpwuid(getuid());
            home = user != nullptr ? user->pw_dir : nullptr;
        }
        if (home == nullptr) {
            return {};
        }
#if defined(__APPLE__)
        return fs::path(home) / "Library" / "Application Support";
#else
        return fs::path(home) / ".config";
#endif
#endif
    }

    inline fs::path collection_folder() {
        fs::path root = data_root();
        return root.empty() ? fs::path() : root / "MarioPartyCollection";
    }

    inline fs::path own_exe() {
#ifdef _WIN32
        wchar_t buffer[MAX_PATH * 4];
        DWORD length = GetModuleFileNameW(nullptr, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
        return length > 0 ? fs::path(std::wstring(buffer, length)) : fs::path();
#elif defined(__APPLE__)
        char buffer[4096];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) != 0) {
            return {};
        }
        std::error_code ec;
        fs::path path = fs::canonical(buffer, ec);
        return ec ? fs::path(buffer) : path;
#else
        std::error_code ec;
        fs::path path = fs::read_symlink("/proc/self/exe", ec);
        return ec ? fs::path() : path;
#endif
    }

    inline fs::path native_path(std::string text) {
#ifndef _WIN32
        for (char& c : text) {
            if (c == '\\') {
                c = '/';
            }
        }
#endif
        return fs::path(std::u8string(text.begin(), text.end()));
    }

    // A program named in collection.ini, from `folder` when relative; on Linux and macOS without a Windows build's .exe
    // when that is what's there.
    inline fs::path find_exe(const fs::path& folder, const std::string& text) {
        fs::path path = native_path(text);
        if (path.is_relative()) {
            path = folder / path;
        }
#ifndef _WIN32
        std::error_code ec;
        if (!fs::exists(path, ec) && path.extension() == ".exe") {
            path.replace_extension();
        }
#endif
        return path;
    }

    inline bool start_exe(const fs::path& exe) {
#ifdef _WIN32
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
#else
        std::string program = exe.string();
        std::string folder = exe.parent_path().string();
        if (access(program.c_str(), X_OK) != 0) {
            fprintf(stderr, "collection: %s isn't there or can't be run\n", program.c_str());
            return false;
        }
        // Twice forked, so the game is no child of this one (which quits) and nothing waits on it.
        pid_t child = fork();
        if (child < 0) {
            fprintf(stderr, "collection: couldn't start %s\n", program.c_str());
            return false;
        }
        if (child == 0) {
            setsid();
            if (fork() != 0) {
                _exit(0);
            }
            if (chdir(folder.c_str()) != 0) {
                _exit(127);
            }
            execl(program.c_str(), program.c_str(), "--boot", (char*)nullptr);
            _exit(127);
        }
        int status = 0;
        waitpid(child, &status, 0);
        return true;
#endif
    }
}
