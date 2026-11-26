// Melty's test mode (C++ port of the snippet Melty gives). While Melty tests a release it writes
// %LOCALAPPDATA%\Melty\test\pvz-bird-mod.json; its "checks" names the file to append one line per
// check to, verdict first: "pass <name>" or "fail <name>: why". Only the file name is kept, so it
// never writes outside Melty\test. Without the file (a player's game), nothing happens.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace melty_test {

inline std::wstring& ChecksFile() { static std::wstring f; return f; }

inline std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

inline void Report(const char* name, bool passed, const std::string& why = {}) {
    if (ChecksFile().empty()) return;
    std::string line = std::string(passed ? "pass " : "fail ") + name;
    if (!passed && !why.empty()) {
        std::string w = why;
        for (auto& c : w) if (c == '\r' || c == '\n') c = ' ';
        line += ": " + w;
    }
    line += "\r\n";
    HANDLE h = CreateFileW(ChecksFile().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
    CloseHandle(h);
}

inline void Start() {
    wchar_t local[MAX_PATH] = L"";
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH)) return;
    std::wstring dir = std::wstring(local) + L"\\Melty\\test\\";
    HANDLE h = CreateFileW((dir + L"pvz-bird-mod.json").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;   // not a test run
    std::string json;
    char buf[4096]; DWORD got = 0;
    while (ReadFile(h, buf, sizeof buf, &got, nullptr) && got) json.append(buf, got);
    CloseHandle(h);
    size_t k = json.find("\"checks\"");
    if (k == std::string::npos) return;
    size_t q = json.find('"', json.find(':', k) + 1);
    if (q == std::string::npos) return;
    std::string value;
    for (size_t i = q + 1; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) ++i;   // \\ -> \, \/ -> /
        value += json[i];
    }
    size_t slash = value.find_last_of("\\/");
    std::string name = slash == std::string::npos ? value : value.substr(slash + 1);   // only ever inside Melty\test
    if (name.empty()) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.size(), nullptr, 0);
    std::wstring wname(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.size(), &wname[0], n);
    ChecksFile() = dir + wname;
    Report("mod-loads", true);
}

} // namespace melty_test
