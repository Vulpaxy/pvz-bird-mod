// Starts Plants vs. Zombies through Steam (unless it is already running),
// waits for the unpacked game process popcapgame1.exe, and loads birdmod.dll
// into it. Must be built as 32-bit to match the game.
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>

static DWORD FindProcess(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{sizeof pe};
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !pid; ok = Process32NextW(snap, &pe))
        if (!_wcsicmp(pe.szExeFile, exe)) pid = pe.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

static bool AlreadyLoaded(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{sizeof me};
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &me); ok && !found; ok = Module32NextW(snap, &me))
        found = !_wcsicmp(me.szModule, name);
    CloseHandle(snap);
    return found;
}

static int Say(const wchar_t* msg, UINT icon = MB_ICONINFORMATION) {
    MessageBoxW(nullptr, msg, L"PvZ bird mod", icon);
    return 1;
}

// Arguments: [--angry-birds <folder>] [--pvz <folder>] [dll name]
// The folders (passed by Melty or a shortcut) are saved to birdmod.ini next to
// the DLL, which reads them, so the mod never has to ask where the games are.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::wstring name = L"birdmod.dll", abDir, pvzDir;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        if (!_wcsicmp(argv[i], L"--angry-birds") && i + 1 < argc) abDir = argv[++i];
        else if (!_wcsicmp(argv[i], L"--pvz") && i + 1 < argc) pvzDir = argv[++i];
        else if (argv[i][0] && argv[i][0] != L'-') name = argv[i];   // optional: another DLL name, for testing
    }
    if (argv) LocalFree(argv);
    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wcscpy(wcsrchr(dll, L'\\') + 1, name.c_str());
    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) return Say((name + L" must be next to this launcher.").c_str(), MB_ICONERROR);
    if (!abDir.empty() || !pvzDir.empty()) {
        std::wstring ini = dll;
        ini = ini.substr(0, ini.rfind(L'\\') + 1) + L"birdmod.ini";
        if (!abDir.empty())  WritePrivateProfileStringW(L"birdmod", L"AngryBirdsFolder", abDir.c_str(), ini.c_str());
        if (!pvzDir.empty()) WritePrivateProfileStringW(L"birdmod", L"PvzFolder", pvzDir.c_str(), ini.c_str());
    }

    DWORD pid = FindProcess(L"popcapgame1.exe");
    if (!pid) {
        ShellExecuteW(nullptr, L"open", L"steam://rungameid/3590", nullptr, nullptr, SW_SHOWNORMAL);
        for (int i = 0; i < 1200 && !(pid = FindProcess(L"popcapgame1.exe")); ++i) Sleep(100);
        if (!pid) return Say(L"Plants vs. Zombies did not start within 2 minutes.", MB_ICONERROR);
        Sleep(1500);   // let the game finish loading its own DLLs
    }
    if (AlreadyLoaded(pid, name.c_str())) return Say(L"The bird mod is already running in this game.");

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                              PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) return Say(L"Could not open the game process.", MB_ICONERROR);
    SIZE_T bytes = (wcslen(dll) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(proc, remote, dll, bytes, nullptr);
    auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE th = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!th) return Say(L"Could not load the mod into the game.", MB_ICONERROR);
    WaitForSingleObject(th, 10000);
    DWORD module = 0; GetExitCodeThread(th, &module);
    CloseHandle(th);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    if (!module) return Say(L"The game refused to load birdmod.dll.", MB_ICONERROR);
    // Stay open while the game runs, so whatever started the launcher (Melty, a
    // shortcut) sees the game as running until the player closes it.
    if (HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
        WaitForSingleObject(game, INFINITE);
        CloseHandle(game);
    }
    return 0;
}
