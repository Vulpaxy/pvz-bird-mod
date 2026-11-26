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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int) {
    std::wstring name = (cmd && *cmd) ? cmd : L"birdmod.dll";   // optional: another DLL name, for testing
    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wcscpy(wcsrchr(dll, L'\\') + 1, name.c_str());
    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) return Say((name + L" must be next to this launcher.").c_str(), MB_ICONERROR);

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
    return 0;
}
