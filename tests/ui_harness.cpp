// Pokazuje okno kroku 0 na sztucznym obrazie (bez Photoshopa) i zapisuje zrzut do tests/out/main_dialog.png.
#include <windows.h>

#include <objidl.h>
#include <gdiplus.h>

#include <cmath>
#include <string>

#include "plugin/host.h"
#include "ui/main_dialog.h"

using namespace zk;

static std::wstring g_out;
static bool g_interact = false;

static bool SavePng(HBITMAP bmp, const std::wstring& path) {
    Gdiplus::Bitmap b(bmp, nullptr);
    CLSID png;
    UINT n = 0, sz = 0;
    Gdiplus::GetImageEncodersSize(&n, &sz);
    std::string buf(sz, '\0');
    auto* enc = (Gdiplus::ImageCodecInfo*)buf.data();
    Gdiplus::GetImageEncoders(n, sz, enc);
    for (UINT i = 0; i < n; ++i)
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) png = enc[i].Clsid;
    return b.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
}

static void Pump(int rounds) {
    MSG m;
    for (int i = 0; i < rounds; ++i) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(20);
    }
}

// Symulacja: klik w edytorze krzywej (nowy punkt w cieniach) i przeciagniecie go w gore.
static void InteractCurve(HWND dlg) {
    HWND ed = FindWindowExW(dlg, nullptr, L"ZetmarKolorCurveEditor", nullptr);
    if (!ed) return;
    RECT r;
    GetClientRect(ed, &r);
    const int x = r.right / 4, y0 = r.bottom * 3 / 4, y1 = r.bottom / 2;
    SendMessageW(ed, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y0));
    for (int k = 1; k <= 10; ++k) SendMessageW(ed, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y0 + (y1 - y0) * k / 10));
    SendMessageW(ed, WM_LBUTTONUP, 0, MAKELPARAM(x, y1));
    ReleaseCapture();
    Pump(5);
}

static void OnShown(HWND h, void*) {
    if (g_interact) InteractCurve(h);
    MSG m;
    for (int i = 0; i < 20; ++i) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(20);
    }
    RECT rc;
    GetWindowRect(h, &rc);
    HDC sdc = GetDC(nullptr);
    HDC mdc = CreateCompatibleDC(sdc);
    HBITMAP bmp = CreateCompatibleBitmap(sdc, rc.right - rc.left, rc.bottom - rc.top);
    HGDIOBJ old = SelectObject(mdc, bmp);
    SetForegroundWindow(h);
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    Sleep(200);
    BitBlt(mdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, sdc, rc.left, rc.top, SRCCOPY);
    SelectObject(mdc, old);
    SavePng(bmp, g_out);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
    PostMessageW(h, WM_CLOSE, 0, 0);
}

int wmain(int argc, wchar_t** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Gdiplus::GdiplusStartupInput gi;
    ULONG_PTR tok;
    Gdiplus::GdiplusStartup(&tok, &gi, nullptr);
    g_out = argc > 1 ? argv[1] : L"main_dialog.png";
    const int lang = argc > 2 ? _wtoi(argv[2]) : 1;
    // Argumenty: <png> <1=PL|2=EN> [plik .cube albo -] [zakladka 0-6] [preset krzywej 0-7, -1] [biblioteka|-] [interact|-]
    const wchar_t* lutPath = argc > 3 && wcscmp(argv[3], L"-") != 0 ? argv[3] : nullptr;
    const int page = argc > 4 ? _wtoi(argv[4]) : 0;
    const int preset = argc > 5 ? _wtoi(argv[5]) : -1;
    const wchar_t* libDir = argc > 6 && wcscmp(argv[6], L"-") != 0 ? argv[6] : nullptr;
    g_interact = argc > 7 && wcscmp(argv[7], L"interact") == 0;
    // [8] tryb cz-b 0-3, [9] preset tonowania 0-4 (-1 = brak)
    const int bwMode = argc > 8 ? _wtoi(argv[8]) : 0;
    const int toningPreset = argc > 9 ? _wtoi(argv[9]) : -1;
    const int duotonePreset = argc > 10 ? _wtoi(argv[10]) : -1;  // [10] preset duotonu 0-5

    // Sztuczny obraz: gradienty + przezroczysty pas + eliptyczne "zaznaczenie" z miekka krawedzia.
    PreviewImage pi;
    pi.w = 640; pi.h = 427;
    pi.rgba.resize((size_t)pi.w * pi.h * 4);
    pi.sel.resize((size_t)pi.w * pi.h);
    for (int y = 0; y < pi.h; ++y)
        for (int x = 0; x < pi.w; ++x) {
            float* p = &pi.rgba[((size_t)y * pi.w + x) * 4];
            p[0] = x / (float)pi.w;
            p[1] = y / (float)pi.h;
            p[2] = 0.5f + 0.5f * std::sin(x * 0.03f);
            p[3] = (y > 360) ? 0.0f : 1.0f;
            float dx = (x - 320) / 220.0f, dy = (y - 200) / 150.0f;
            float r = std::sqrt(dx * dx + dy * dy);
            pi.sel[(size_t)y * pi.w + x] = (uint8_t)(255 * std::fmin(1.0f, std::fmax(0.0f, (1.15f - r) / 0.3f)));
        }

    ui::DialogContext ctx;
    ctx.instance = GetModuleHandleW(nullptr);
    ctx.preview = &pi;
    ctx.diagnostics = L"=== harness (bez Photoshopa) ===\r\nhostSig: '----'\r\nimageMode 3, depth 8, planes 4\r\n";
    ctx.onShown = OnShown;
    ctx.forceLanguage = lang;
    FilterParams p;
    ctx.startPage = page;
    if (preset >= 0) {
        ApplyCurvePreset((CurvePreset)preset, p.curves);
        ctx.previewMode = 2;
    }
    if (libDir) ctx.libraryDir = libDir;
    if (bwMode > 0) {
        p.bw.mode = bwMode;
        ctx.previewMode = 2;
    }
    if (duotonePreset >= 0) {
        ApplyDuotonePreset((DuotonePreset)duotonePreset, p.duotone);
        ctx.previewMode = 2;
    }
    if (toningPreset >= 0) {
        ApplyToningPreset((ToningPreset)toningPreset, p.toning);
        ctx.previewMode = 2;
    }
    if (lutPath) {
        wcsncpy_s(p.lutPath, lutPath, _TRUNCATE);
        ctx.previewMode = 2;
    }
    ui::ShowMainDialog(ctx, p);
    Gdiplus::GdiplusShutdown(tok);
    return 0;
}
