// Mario Party N64 Collection: what a game does with its window as it hands over to another (collection_mp1.cpp's
// launch, collection_game.cpp's return), right before starting the other game's exe. Header-only: both files include
// it.
//
// 1. The window mode the player has now goes into Mario Party 1's settings (%LOCALAPPDATA%\MarioPartyRecompiled\
//    graphics.json, "wm_option": "Fullscreen" or "Windowed"), the collection's one copy (collection_game.cpp says
//    how Mario Party 2 and 3 share it), so the next game opens the same way: RecompFrontend's fullscreen hotkey
//    changes the mode without saving it, so the file alone can be out of date. Fullscreen is the window having no
//    caption (RT64's fullscreen is a borderless window over the monitor).
// 2. The collection's launcher (src/launcher/collection_launcher.cpp: its black backdrop window, class
//    "MarioPartyCollectionBackdrop"), when it is running, is told this process is about to hand over (WM_COPYDATA,
//    dwData 'MPCS', this process's id), and covers the window's place with black before this game's window closes,
//    until the next game's window is up. Without the launcher nothing more happens: the games switch as they always
//    have.
#pragma once

#ifdef _WIN32
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace collection_window {
    constexpr ULONG_PTR handover_message = 0x4D504353; // 'MPCS'

    struct Search {
        DWORD pid;
        HWND found;
    };

    // This process's game window: its largest visible top-level window.
    inline HWND own_window() {
        Search search{ GetCurrentProcessId(), nullptr };
        EnumWindows(
            [](HWND window, LPARAM data) -> BOOL {
                Search* search = reinterpret_cast<Search*>(data);
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                if (pid == search->pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
                    search->found = window;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&search));
        return search.found;
    }

    inline bool is_fullscreen(HWND window) {
        return window != nullptr && (GetWindowLongW(window, GWL_STYLE) & WS_CAPTION) != WS_CAPTION;
    }

    // "wm_option" in Mario Party 1's graphics.json set to the mode the player has now.
    inline void save_window_mode(bool fullscreen) {
        const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
        if (local == nullptr) {
            return;
        }
        std::filesystem::path path = std::filesystem::path(local) / L"MarioPartyRecompiled" / L"graphics.json";
        std::string text;
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return;
            }
            std::stringstream contents;
            contents << file.rdbuf();
            text = contents.str();
        }
        // "wm_option": "<value>" (RecompFrontend writes it as one line, with a space after the colon).
        size_t key = text.find("\"wm_option\"");
        size_t open = key == std::string::npos ? key : text.find('"', text.find(':', key) + 1);
        size_t close = open == std::string::npos ? open : text.find('"', open + 1);
        if (close == std::string::npos) {
            return;
        }
        std::string updated = text.substr(0, open + 1) + (fullscreen ? "Fullscreen" : "Windowed") + text.substr(close);
        if (updated == text) {
            return;
        }
        std::filesystem::path temp = path;
        temp += ".collection.tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file) {
                return;
            }
            file << updated;
        }
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
    }

    // Before starting the other game: the mode saved, the launcher told (it waits up to 1 s for the backdrop).
    inline void before_handover() {
        HWND window = own_window();
        if (window != nullptr) {
            save_window_mode(is_fullscreen(window));
        }
        HWND launcher = FindWindowW(L"MarioPartyCollectionBackdrop", nullptr);
        if (launcher != nullptr) {
            DWORD pid = GetCurrentProcessId();
            COPYDATASTRUCT data{ handover_message, sizeof(pid), &pid };
            DWORD_PTR result = 0;
            SendMessageTimeoutW(launcher, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 1000, &result);
            // The new game may take the foreground from the launcher's process.
            AllowSetForegroundWindow(ASFW_ANY);
        }
    }
}
#endif
