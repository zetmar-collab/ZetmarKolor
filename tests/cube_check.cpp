// Sprawdza wszystkie pliki .cube w folderze (rekurencyjnie) parserem wtyczki. Kod wyjscia 1 = jest uszkodzony plik.
// Uzycie: zk_cubecheck.exe <folder>
#include <windows.h>

#include <cstdio>
#include <string>

#include "core/cube_lut.h"

using namespace zk;

static int g_ok = 0, g_bad = 0;

static void Scan(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        std::wstring p = dir + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Scan(p);
            continue;
        }
        if (n.size() < 5 || _wcsicmp(n.c_str() + n.size() - 5, L".cube") != 0) continue;
        CubeLut lut;
        CubeResult r = LoadCubeFile(p, lut);
        if (r.error == CubeError::Ok) {
            ++g_ok;
        } else {
            ++g_bad;
            std::wprintf(L"BLAD %d (wiersz %d): %ls\n", (int)r.error, r.line, p.c_str());
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wprintf(L"Uzycie: zk_cubecheck <folder>\n");
        return 2;
    }
    Scan(argv[1]);
    std::wprintf(L"Poprawne: %d, uszkodzone: %d\n", g_ok, g_bad);
    return g_bad || !g_ok ? 1 : 0;
}
