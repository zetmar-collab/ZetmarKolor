// Kontrolka edytora krzywej (Win32). Klik dodaje punkt, przeciaganie przesuwa, prawy klik lub Del usuwa,
// strzalki przesuwaja zaznaczony punkt o 1 poziom. Konce krzywej przesuwaja sie tylko w pionie.
#pragma once
#include <windows.h>

#include <cstdint>

#include "core/curves.h"

namespace zk {
namespace ui {

// Kody powiadomien WM_COMMAND wysylanych do rodzica (HIWORD(wParam)).
constexpr WORD kCurveChanged = 0x4001;   // zmiana ksztaltu krzywej
constexpr WORD kCurveSelected = 0x4002;  // zmiana zaznaczonego punktu
constexpr WORD kCurveDragEnd = 0x4003;   // koniec przeciagania (zwolniony przycisk)

struct CurveEditorModel {
    CurveSet* set = nullptr;
    int channel = kCurveRGB;
    int selected = -1;                 // indeks zaznaczonego punktu albo -1
    const uint32_t* histogram = nullptr;  // 256 kubelkow dla biezacego kanalu (moze byc nullptr)
    bool logHistogram = false;
    int dpi = 0;                       // skala okna wtyczki (0 = DPI okna)
};

void RegisterCurveEditor(HINSTANCE inst);
void UnregisterCurveEditor(HINSTANCE inst);
HWND CreateCurveEditor(HWND parent, HINSTANCE inst, int id, int x, int y, int size, CurveEditorModel* model);
// Przesuwa punkt i z ograniczeniami: konce tylko w pionie, punkty wewnetrzne miedzy sasiadami.
void CurveMovePoint(Curve& c, int i, float x, float y);
// Po zmianie modelu z zewnatrz (kanal, reset, preset, pola liczbowe).
void CurveEditorRefresh(HWND editor);

}  // namespace ui
}  // namespace zk
