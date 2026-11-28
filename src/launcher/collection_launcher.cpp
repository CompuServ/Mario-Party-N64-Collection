// Mario Party N64 Collection: the launcher ("Mario Party Collection.exe", the one program at the top of a build folder;
// Mario Party 1 is in its MP1\ folder, Mario Party 2 and 3 in MP2\ and MP3\).
// Start the collection here: it starts Mario Party 1 and stays in the background, so that when one game hands over to
// another (Mario Party 1's Party Mode menu to Mario Party 2 or 3, and back) the player sees black, not the desktop,
// between the two, windowed or fullscreen.
//
// - Its window (class "MarioPartyCollectionBackdrop") is a black, borderless, topmost popup that never takes the focus
//   or shows in the taskbar, hidden until a handover.
// - A game about to hand over (src/collection_window.hpp, before it starts the other exe) sends it WM_COPYDATA
//   ('MPCS', its process id). The launcher finds that process's window and shows the backdrop over its place (the
//   whole monitor when the game is fullscreen), before the game's window closes: the game has already faded to black.
// - It then waits for a window of another Mario Party recomp (MarioParty*Recompiled.exe) to show. A windowed game's
//   window is moved and sized to where the last one was (the games always open centred at their default size);
//   a fullscreen one goes fullscreen by itself (the outgoing game saved the mode in Mario Party 1's graphics.json,
//   which the next one reads). The backdrop stays over the new window while it starts drawing (1.5 s), then hides,
//   and the new window is brought to the front with the focus. 30 s without a new window: the backdrop hides.
// - Mario Party 1 starts with --boot (straight into the game) when RecompFrontend has stored its ROM
//   (%LOCALAPPDATA%\MarioPartyRecompiled\marioparty.n64.us.1.0.z64), else without, for its launcher screen.
// - It quits once no Mario Party recomp has been running for 5 s (the player closed the game), and a second copy just
//   quits. Windows only.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cwchar>
#include <filesystem>
#include <set>
#include <string>

namespace {
    constexpr ULONG_PTR handover_message = 0x4D504353; // 'MPCS'
    constexpr UINT_PTR tick_timer = 1;
    constexpr UINT tick_ms = 15;
    constexpr DWORD new_window_timeout_ms = 30000;
    constexpr DWORD cover_ms = 1500;
    constexpr DWORD place_ms = 400;
    constexpr DWORD no_game_quit_ms = 5000;
    const wchar_t* backdrop_class = L"MarioPartyCollectionBackdrop";

    enum class State { Idle, WaitingForNewWindow, Covering };

    HWND backdrop = nullptr;
    State state = State::Idle;
    DWORD state_since = 0;
    DWORD old_pid = 0;
    RECT old_rect{};
    bool old_fullscreen = false;
    HWND new_window = nullptr;
    DWORD last_game_seen = 0;
    DWORD last_game_check = 0;

    bool is_game_exe(const wchar_t* name) {
        std::wstring exe(name);
        for (auto& c : exe) {
            c = (wchar_t)towlower(c);
        }
        return exe.rfind(L"marioparty", 0) == 0 && exe.size() > 18 && exe.compare(exe.size() - 14, 14, L"recompiled.exe") == 0;
    }

    std::set<DWORD> game_pids() {
        std::set<DWORD> pids;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return pids;
        }
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
            if (is_game_exe(entry.szExeFile)) {
                pids.insert(entry.th32ProcessID);
            }
        }
        CloseHandle(snapshot);
        return pids;
    }

    struct WindowSearch {
        std::set<DWORD> pids;
        DWORD exclude;
        HWND found;
        DWORD found_pid;
    };

    // A visible, unowned top-level window of one of the given processes (not `exclude`).
    HWND find_game_window(const std::set<DWORD>& pids, DWORD exclude, DWORD* pid_out = nullptr) {
        WindowSearch search{ pids, exclude, nullptr, 0 };
        EnumWindows(
            [](HWND window, LPARAM data) -> BOOL {
                WindowSearch* search = reinterpret_cast<WindowSearch*>(data);
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                if (pid != search->exclude && search->pids.count(pid) && IsWindowVisible(window) &&
                    GetWindow(window, GW_OWNER) == nullptr) {
                    search->found = window;
                    search->found_pid = pid;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&search));
        if (pid_out != nullptr) {
            *pid_out = search.found_pid;
        }
        return search.found;
    }

    bool window_is_fullscreen(HWND window) {
        return (GetWindowLongW(window, GWL_STYLE) & WS_CAPTION) != WS_CAPTION;
    }

    RECT monitor_rect(const RECT& rect) {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfoW(MonitorFromRect(&rect, MONITOR_DEFAULTTOPRIMARY), &info);
        return info.rcMonitor;
    }

    void show_backdrop(const RECT& rect) {
        SetWindowPos(backdrop, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        RedrawWindow(backdrop, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }

    void hide_backdrop() {
        SetWindowPos(backdrop, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
    }

    // A game is about to hand over: black over its window's place.
    void begin_handover(DWORD pid) {
        std::set<DWORD> pids{ pid };
        HWND window = find_game_window(pids, 0);
        if (window != nullptr) {
            GetWindowRect(window, &old_rect);
            old_fullscreen = window_is_fullscreen(window);
        } else if (old_rect.right == old_rect.left) {
            // No window found and none seen before: the primary monitor.
            old_rect = monitor_rect(RECT{ 0, 0, 1, 1 });
            old_fullscreen = true;
        }
        old_pid = pid;
        new_window = nullptr;
        show_backdrop(old_fullscreen ? monitor_rect(old_rect) : old_rect);
        state = State::WaitingForNewWindow;
        state_since = GetTickCount();
    }

    void place_new_window() {
        if (!old_fullscreen && !window_is_fullscreen(new_window)) {
            SetWindowPos(new_window, nullptr, old_rect.left, old_rect.top, old_rect.right - old_rect.left,
                         old_rect.bottom - old_rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void tick() {
        DWORD now = GetTickCount();

        if (state == State::WaitingForNewWindow) {
            DWORD pid = 0;
            HWND window = find_game_window(game_pids(), old_pid, &pid);
            if (window != nullptr) {
                new_window = window;
                place_new_window();
                state = State::Covering;
                state_since = now;
            } else if (now - state_since > new_window_timeout_ms) {
                hide_backdrop();
                state = State::Idle;
            }
        } else if (state == State::Covering) {
            if (!IsWindow(new_window)) {
                state = State::WaitingForNewWindow;
                state_since = now;
            } else if (now - state_since < place_ms) {
                place_new_window();
            } else if (now - state_since >= cover_ms) {
                hide_backdrop();
                SetForegroundWindow(new_window);
                BringWindowToTop(new_window);
                state = State::Idle;
            }
        }

        // Quit once no game has been running for a while (checked twice a second).
        if (now - last_game_check >= 500) {
            last_game_check = now;
            if (!game_pids().empty() || state != State::Idle) {
                last_game_seen = now;
            } else if (now - last_game_seen > no_game_quit_ms) {
                PostQuitMessage(0);
            }
        }
    }

    LRESULT CALLBACK backdrop_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_COPYDATA: {
                const COPYDATASTRUCT* data = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
                if (data != nullptr && data->dwData == handover_message && data->cbData >= sizeof(DWORD)) {
                    begin_handover(*reinterpret_cast<const DWORD*>(data->lpData));
                    return TRUE;
                }
                return FALSE;
            }
            case WM_TIMER:
                tick();
                return 0;
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
            case WM_ERASEBKGND: {
                RECT rect;
                GetClientRect(window, &rect);
                FillRect(reinterpret_cast<HDC>(wparam), &rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                return 1;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    std::filesystem::path own_folder() {
        wchar_t buffer[MAX_PATH * 4];
        DWORD length = GetModuleFileNameW(nullptr, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
        return std::filesystem::path(std::wstring(buffer, length)).parent_path();
    }

    bool start_mario_party_1() {
        // MP1\ beside the launcher (from build-027), or beside it (older builds).
        std::filesystem::path folder = own_folder() / L"MP1";
        std::error_code ec;
        if (!std::filesystem::exists(folder / L"MarioPartyRecompiled.exe", ec)) {
            folder = own_folder();
        }
        std::filesystem::path exe = folder / L"MarioPartyRecompiled.exe";
        if (!std::filesystem::exists(exe, ec)) {
            MessageBoxW(nullptr, L"Mario Party 1 (MP1\\MarioPartyRecompiled.exe) isn't beside the collection's launcher.",
                        L"Mario Party N64 Collection", MB_ICONERROR);
            return false;
        }
        bool boot = false;
        if (const wchar_t* local = _wgetenv(L"LOCALAPPDATA")) {
            boot = std::filesystem::exists(std::filesystem::path(local) / L"MarioPartyRecompiled" /
                                               L"marioparty.n64.us.1.0.z64", ec);
        }
        std::wstring command = L"\"" + exe.wstring() + L"\"" + (boot ? L" --boot" : L"");
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup,
                            &process)) {
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Physical pixels everywhere, so the rectangles taken from one game's window fit the next one's.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE single = CreateMutexW(nullptr, TRUE, L"MarioPartyCollectionLauncher");
    if (single == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = backdrop_proc;
    window_class.hInstance = instance;
    window_class.hCursor = nullptr;
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    window_class.lpszClassName = backdrop_class;
    RegisterClassW(&window_class);
    backdrop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, backdrop_class,
                               L"Mario Party N64 Collection", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (backdrop == nullptr) {
        return 1;
    }

    if (!start_mario_party_1()) {
        return 1;
    }
    last_game_seen = GetTickCount();
    SetTimer(backdrop, tick_timer, tick_ms, nullptr);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
