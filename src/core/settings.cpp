#include "settings.h"

#include <windows.h>

#include <shlobj.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace zk {

std::wstring DataDir() {
    PWSTR p = nullptr;
    std::wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p)) && p) {
        d = std::wstring(p) + L"\\ZetmarKolor";
        CreateDirectoryW(d.c_str(), nullptr);
    }
    if (p) CoTaskMemFree(p);
    return d;
}

namespace {

std::wstring SettingsPath() {
    std::wstring d = DataDir();
    return d.empty() ? d : d + L"\\settings.json";
}

bool ReadSmallFile(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (1 << 20);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr) && got == out.size();
    }
    CloseHandle(h);
    return ok;
}

std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Pozycja wartosci klucza (po dwukropku) albo npos.
size_t FindValue(const std::string& json, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return p;
    p = json.find(':', p + k.size());
    return p == std::string::npos ? p : p + 1;
}

int GetInt(const std::string& json, const char* key, int def) {
    size_t p = FindValue(json, key);
    return p == std::string::npos ? def : std::atoi(json.c_str() + p);
}

// Napis JSON; ucieczki obslugiwane tylko dla \\ i \" (wystarcza dla sciezek).
std::string GetString(const std::string& json, const char* key) {
    size_t p = FindValue(json, key);
    if (p == std::string::npos) return "";
    p = json.find('"', p);
    if (p == std::string::npos) return "";
    std::string out;
    for (++p; p < json.size() && json[p] != '"'; ++p) {
        if (json[p] == '\\' && p + 1 < json.size()) ++p;
        out += json[p];
    }
    return out;
}

std::string Escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if ((unsigned char)c < 32) continue;
        if (c == '\\' || c == '"') o += '\\';
        o += c;
    }
    return o;
}

}  // namespace

// Zapis przez plik tymczasowy o unikalnej nazwie (kilka instancji Photoshopa nie koliduje) z wymuszeniem zapisu
// na dysk, zeby przerwany zapis nie zostawil uszkodzonego pliku.
bool WriteFileAtomic(const std::wstring& path, const std::string& data) {
    std::wstring tmp = path + L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount()) + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    bool ok = (data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr)) && wrote == data.size();
    if (ok) ok = FlushFileBuffers(h) != FALSE;
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}

bool DebugEnabled() {
    std::wstring d = DataDir();
    return !d.empty() && GetFileAttributesW((d + L"\\debug.flag").c_str()) != INVALID_FILE_ATTRIBUTES;
}

AppSettings LoadSettings() {
    AppSettings s;
    std::string json;
    std::wstring path = SettingsPath();
    if (path.empty() || !ReadSmallFile(path, json)) return s;
    s.language = GetInt(json, "language", 0);
    if (s.language < 0 || s.language > 2) s.language = 0;
    s.lastLutDir = FromUtf8(GetString(json, "lastLutDir"));
    s.lutLibraryDir = FromUtf8(GetString(json, "lutLibraryDir"));
    s.lastLookDir = FromUtf8(GetString(json, "lastLookDir"));
    return s;
}

bool SaveSettings(const AppSettings& s) {
    std::wstring path = SettingsPath();
    if (path.empty()) return false;
    char buf[64];
    std::snprintf(buf, sizeof buf, "{\n  \"version\": 1,\n  \"language\": %d,\n", s.language);
    std::string json = buf;
    json += "  \"lastLutDir\": \"" + Escape(ToUtf8(s.lastLutDir)) + "\",\n";
    json += "  \"lutLibraryDir\": \"" + Escape(ToUtf8(s.lutLibraryDir)) + "\",\n";
    json += "  \"lastLookDir\": \"" + Escape(ToUtf8(s.lastLookDir)) + "\"\n}\n";
    return WriteFileAtomic(path, json);
}

}  // namespace zk
