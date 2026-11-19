// Testy biblioteki LUT-ow na tymczasowym folderze (dolaczane do core_tests.cpp).
#include "ui/lut_library.h"

static bool WriteText(const std::wstring& path, const std::string& s) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, s.data(), (DWORD)s.size(), &w, nullptr) && w == s.size();
    CloseHandle(h);
    return ok;
}

// Usuwa folder testowy z zawartoscia (tylko pliki utworzone przez ten test, 1 poziom podfolderow).
static void RemoveTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(dir + L"\\" + n);
            else DeleteFileW((dir + L"\\" + n).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

static void RunLibraryTests() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring root = std::wstring(tmp) + L"zk_lib_test_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring src = root + L"\\src", lib = root + L"\\lib";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(src.c_str(), nullptr);
    CreateDirectoryW(lib.c_str(), nullptr);
    WriteText(src + L"\\Ciepły.cube", IdentityCube(5));
    WriteText(src + L"\\zly.cube", "LUT_3D_SIZE 3\n0 0 0\n");
    WriteText(src + L"\\Zimny.CUBE", IdentityCube(3));

    std::wstring dst;
    CubeResult pr;
    DWORD err = 0;
    bool a1 = ui::AddToLibrary(lib, L"", src + L"\\Ciepły.cube", dst, pr, err);
    CHECK(a1 && dst == lib + L"\\Ciepły.cube", "biblioteka: dodanie poprawnego pliku (polskie znaki w nazwie)");
    bool a2 = ui::AddToLibrary(lib, L"", src + L"\\Ciepły.cube", dst, pr, err);
    CHECK(a2 && dst == lib + L"\\Ciepły (2).cube", "biblioteka: kolizja nazw -> \"(2)\" zamiast nadpisania");
    bool a3 = ui::AddToLibrary(lib, L"", src + L"\\zly.cube", dst, pr, err);
    CHECK(!a3 && pr.error == CubeError::WrongRowCount && GetFileAttributesW((lib + L"\\zly.cube").c_str()) ==
                                                             INVALID_FILE_ATTRIBUTES,
          "biblioteka: uszkodzony plik odrzucony i nie skopiowany");
    bool a4 = ui::AddToLibrary(lib, L"Portrety", src + L"\\Zimny.CUBE", dst, pr, err);
    CHECK(a4 && dst == lib + L"\\Portrety\\Zimny.cube", "biblioteka: dodanie do kategorii (podfolder tworzony)");

    auto entries = ui::ScanLibrary(lib);
    auto cats = ui::LibraryCategories(entries);
    CHECK(entries.size() == 3 && cats.size() == 1 && cats[0] == L"Portrety" && entries.back().category == L"Portrety" &&
              entries.back().name == L"Zimny",
          "biblioteka: skanowanie folderu i podfolderow (kategorie)");
    CHECK(ui::ScanLibrary(root + L"\\nie_ma").empty(), "biblioteka: brak folderu -> pusta lista bez bledu");

    auto base = ui::MakeThumbBase(nullptr);
    CubeLut id;
    std::string idText = IdentityCube(17);
    ParseCubeText(idText.data(), idText.size(), id);
    auto th = ui::MakeThumb(base, &id);
    int maxd = 0;
    for (size_t i = 0; i < base.size(); ++i)
        for (int s = 0; s < 24; s += 8)
            maxd = std::max(maxd, std::abs((int)((base[i] >> s) & 0xFF) - (int)((th[i] >> s) & 0xFF)));
    CHECK(base.size() == (size_t)ui::kThumbW * ui::kThumbH && maxd <= 1, "biblioteka: miniatura z LUT-em tozsamosciowym = obraz bazowy");

    RemoveTree(root);
    CHECK(GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES, "biblioteka: sprzatanie folderu testowego");
}
