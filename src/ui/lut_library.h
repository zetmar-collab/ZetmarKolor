// Biblioteka LUT-ow: folder z plikami .cube (podfoldery = kategorie), dodawanie, usuwanie do Kosza, miniatury.
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/cube_lut.h"

namespace zk {

struct PreviewImage;

namespace ui {

struct LutEntry {
    std::wstring path;      // pelna sciezka
    std::wstring name;      // nazwa bez .cube
    std::wstring category;  // nazwa podfolderu albo pusta (folder glowny)
};

// Folder biblioteki: z ustawien, inaczej C:\ProgramData\ZetmarKolor\LUTs (gdy istnieje),
// inaczej %APPDATA%\ZetmarKolor\LUTs (tworzony).
std::wstring LibraryDir();
void SetLibraryDir(const std::wstring& dir);

// Pliki .cube z folderu glownego i podfolderow pierwszego poziomu, posortowane (kategoria, nazwa).
std::vector<LutEntry> ScanLibrary(const std::wstring& dir);
std::vector<std::wstring> LibraryCategories(const std::vector<LutEntry>& entries);

// Kopiuje plik do biblioteki (do podfolderu `category`, gdy niepusty) po sprawdzeniu parserem.
// Przy kolizji nazw dodaje " (2)", " (3)"... Zwraca false i opis bledu (CubeError albo blad kopiowania).
bool AddToLibrary(const std::wstring& dir, const std::wstring& category, const std::wstring& src, std::wstring& dst,
                  CubeResult& parse, DWORD& winErr);

// Przenosi pliki do Kosza (z mozliwoscia przywrocenia). Zwraca true, gdy operacja sie powiodla.
// false dla dyskow sieciowych / wymiennych: tam usuniecie jest trwale (brak Kosza).
bool PathHasRecycleBin(const std::wstring& path);
bool MoveToRecycleBin(HWND owner, const std::vector<std::wstring>& paths);

// Miniatura: obraz bazowy (BGRA, w x h) przepuszczony przez LUT. Bez LUT-a (nullptr) - sam obraz.
constexpr int kThumbW = 112, kThumbH = 74;
std::vector<uint32_t> MakeThumbBase(const PreviewImage* preview);  // kThumbW x kThumbH, RGBA float -> BGRA
std::vector<uint32_t> MakeThumb(const std::vector<uint32_t>& base, const CubeLut* lut);
std::vector<uint32_t> MakeErrorThumb();

}  // namespace ui
}  // namespace zk
