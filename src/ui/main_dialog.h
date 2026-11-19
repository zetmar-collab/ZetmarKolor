// Okno wtyczki (etap prototypu): podglad przed/po, wybor pliku .cube, opcje testowe kroku 0.
#pragma once
#include <windows.h>

#include <string>

#include "core/cube_lut.h"
#include "plugin/host.h"

namespace zk {
namespace ui {

struct DialogContext {
    HWND parent = nullptr;
    HINSTANCE instance = nullptr;
    const PreviewImage* preview = nullptr;  // moze byc nullptr (brak podgladu)
    std::wstring diagnostics;
    int previewMode = 0;     // 0 = po, 1 = przed, 2 = podzial
    std::wstring libraryDir;  // harness: folder biblioteki zamiast ustawienia (pusty = z ustawien)
    int startPage = 0;       // 0 Krzywe, 1 Cz-B, 2 Duotone, 3 LUT, 4 Toning, 5 Biblioteka, 6 Test
    int forceLanguage = -1;  // harness: 0/1/2 zamiast ustawienia z settings.json
    // Hak testowy (harness): wywolywany po pokazaniu okna.
    void (*onShown)(HWND dlg, void* user) = nullptr;
    void* onShownUser = nullptr;
};

// true = OK.
bool ShowMainDialog(DialogContext& ctx, FilterParams& p);

void ShowInfo(HWND parent, const wchar_t* text);

// Czytelny opis bledu wczytywania .cube w biezacym jezyku.
std::wstring CubeErrorText(const CubeResult& r);

}  // namespace ui
}  // namespace zk
