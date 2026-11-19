#include "ui/main_dialog.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <cwctype>
#include <map>
#include <vector>

#include "core/curves.h"
#include "core/i18n.h"
#include "core/look_preset.h"
#include "core/settings.h"
#include "core/pipeline.h"
#include "ui/curve_editor.h"
#include "ui/lut_library.h"

namespace zk {
namespace ui {

namespace {

const wchar_t kClass[] = L"ZetmarKolorDialog";
const wchar_t kPreviewClass[] = L"ZetmarKolorPreview";
const UINT_PTR kRenderTimer = 1, kThumbTimer = 2, kLutTimer = 3;

enum Ids {
    IDC_EFFECT = 100, IDC_STRENGTH, IDC_STRENGTH_LBL, IDC_MASKMODE, IDC_TILEMODE, IDC_MARK, IDC_DITHER, IDC_PVMODE,
    IDC_SHOWSEL, IDC_DIAG, IDC_COPY, IDC_OKB, IDC_CANCELB, IDC_LANG, IDC_LUT_CHOOSE, IDC_LUT_CLEAR, IDC_LUT_NAME,
    IDC_LUT_INFO, IDC_LUT_STRENGTH, IDC_LUT_STRENGTH_LBL, IDC_LUT_INTERP, IDC_TABS,
    IDC_CURVES_ON, IDC_CURVE_CH, IDC_CURVE_EDITOR, IDC_CURVE_IN, IDC_CURVE_OUT, IDC_CURVE_LUMA, IDC_CURVE_LOG,
    IDC_CURVE_PRESET, IDC_CURVE_RESET_CH, IDC_CURVE_RESET_ALL,
    IDC_LIB_DIR, IDC_LIB_CHANGE, IDC_LIB_OPEN, IDC_LIB_SEARCH, IDC_LIB_CAT, IDC_LIB_LIST, IDC_LIB_COUNT, IDC_LIB_ADD,
    IDC_LIB_DELETE, IDC_LIB_REFRESH,
    IDC_BW_MODE = 300, IDC_BW_FILTER, IDC_BW_HUE, IDC_BW_HUE_LBL, IDC_BW_HUE_SW, IDC_BW_SAT, IDC_BW_SAT_LBL,
    IDC_BW_RESET,
    IDC_TN_ON = 320, IDC_TN_HI_HUE, IDC_TN_HI_HUE_LBL, IDC_TN_HI_SW, IDC_TN_HI_SAT, IDC_TN_HI_SAT_LBL, IDC_TN_SH_HUE,
    IDC_TN_SH_HUE_LBL, IDC_TN_SH_SW, IDC_TN_SH_SAT, IDC_TN_SH_SAT_LBL, IDC_TN_BAL, IDC_TN_BAL_LBL, IDC_TN_PRESET,
    IDC_TN_RESET,
    IDC_OVERALL = 350, IDC_OVERALL_LBL,
    IDC_DUO_ON = 360, IDC_DUO_SH_SW, IDC_DUO_SH_HEX, IDC_DUO_SH_PICK, IDC_DUO_HI_SW, IDC_DUO_HI_HEX, IDC_DUO_HI_PICK,
    IDC_DUO_SWAP, IDC_DUO_GRAD, IDC_DUO_MID, IDC_DUO_MID_LBL, IDC_DUO_AMT, IDC_DUO_AMT_LBL, IDC_DUO_PRESET,
    IDC_DUO_RESET,
    IDC_LOOK_SAVE = 380, IDC_LOOK_LOAD, IDC_EXPORT_CUBE,
    IDC_BW_MIX = 400,      // ..405 suwaki miksera
    IDC_BW_MIX_LBL = 410,  // ..415 wartosci
};

enum PreviewMode { kPvAfter = 0, kPvBefore = 1, kPvSplit = 2 };
enum Page { kPageCurves = 0, kPageBw, kPageDuotone, kPageLut, kPageToning, kPageLibrary, kPageTest, kPageCount };
const wchar_t kGradClass[] = L"ZetmarKolorGradient";

// Suwak powiazany z polem parametrow (wartosc + etykieta z liczba).
struct SliderBind {
    int id, lbl;
    int32_t* field;
    const wchar_t* fmt;
};

struct State {
    DialogContext* ctx = nullptr;
    FilterParams p;
    HWND hwnd = nullptr, preview = nullptr, tabs = nullptr, curveEditor = nullptr, list = nullptr;
    HFONT font = nullptr, smallFont = nullptr;
    int dpi = 96;
    bool done = false, ok = false;
    int language = 0;  // ustawienie jezyka (0 auto, 1 PL, 2 EN)
    int pvMode = kPvAfter;
    int page = kPageCurves;
    bool showSel = false;
    bool holdBefore = false;   // przytrzymany przycisk myszy na podgladzie
    bool updating = false;     // blokada powiadomien przy programowym ustawianiu pol
    CubeLut lut;               // wczytany LUT (valid() == false: brak)
    std::wstring lutError;     // ostatni blad wczytywania (pusty: brak bledu)
    std::wstring pendingLut;   // LUT wybrany na liscie, wczytywany po krotkiej zwloce (przegladanie strzalkami)
    std::vector<uint32_t> bits;  // BGRA podgladu
    std::vector<std::pair<HWND, int>> pageCtl;  // kontrolka -> strona zakladek
    std::vector<SliderBind> sliders;
    // Probki koloru: barwienie, toning swiatla, toning cienie, duotone cienie, duotone swiatla.
    HBRUSH swatch[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    // Krzywe
    CurveEditorModel curveModel;
    uint32_t hist[kCurveCount][256] = {};
    // Biblioteka
    std::wstring libDir;
    std::vector<LutEntry> entries;
    std::vector<int> shown;      // indeksy entries widoczne na liscie (po filtrze)
    std::wstring search;
    int category = 0;            // 0 = wszystkie, 1 = folder glowny, 2.. = podfoldery
    std::vector<std::wstring> categories;
    std::map<std::wstring, std::vector<uint32_t>> thumbs;  // sciezka -> miniatura
    std::vector<uint32_t> thumbBase;
    std::vector<int> thumbQueue;  // pozycje na liscie czekajace na miniature
    HIMAGELIST images = nullptr;
};

int S(const State* st, int v) { return MulDiv(v, st->dpi, 96); }

std::wstring FileName(const std::wstring& path) {
    size_t s = path.find_last_of(L"\\/");
    return s == std::wstring::npos ? path : path.substr(s + 1);
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring(s.begin(), s.end());
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towlower(c);
    return s;
}

std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t b[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(b, _TRUNCATE, fmt, ap);
    va_end(ap);
    return b;
}

// ---------------------------------------------------------------------------------------------
// Podglad

void RenderPreview(State* st) {
    const PreviewImage* pi = st->ctx->preview;
    if (!pi || pi->w <= 0) return;
    CurveTables ct;
    const PipelineOptions o = MakeOptions(st->p, ct, st->lut.valid() ? &st->lut : nullptr);
    const int mode = st->holdBefore ? kPvBefore : st->pvMode;
    const int splitX = pi->w / 2;
    st->bits.resize((size_t)pi->w * pi->h);
    const bool showSel = st->showSel && !pi->sel.empty();
    // Wiersze liczone rownolegle (podglad nie blokuje okna przy duzych LUT-ach).
    ParallelRows(pi->h, 0, [&](int y) {
        for (int x = 0; x < pi->w; ++x) {
            size_t i = (size_t)y * pi->w + x;
            const float* s = &pi->rgba[i * 4];
            float c[3] = {s[0], s[1], s[2]};
            float m = pi->sel.empty() ? 1.0f : pi->sel[i] / 255.0f;
            const bool after = mode == kPvAfter || (mode == kPvSplit && x >= splitX);
            if (after && s[3] > 0.0f && m > 0.0f) {
                float r[3];
                ProcessPixel(o, c, r);
                for (int k = 0; k < 3; ++k) c[k] += (r[k] - c[k]) * m;
            }
            // Szachownica pod przezroczystoscia.
            float bg = (((x / 8) + (y / 8)) & 1) ? 0.8f : 1.0f;
            float a = s[3];
            uint8_t ch[3];
            for (int k = 0; k < 3; ++k) {
                float v = c[k] * a + bg * (1.0f - a);
                if (showSel) v *= 0.35f + 0.65f * m;
                ch[k] = (uint8_t)std::lround(std::min(std::max(v, 0.0f), 1.0f) * 255.0f);
            }
            st->bits[i] = 0xFF000000u | ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | ch[2];
        }
        if (mode == kPvSplit) st->bits[(size_t)y * pi->w + splitX] = 0xFFFFFFFFu;
    });
    InvalidateRect(st->preview, nullptr, FALSE);
}

// Odswiezenie podgladu z opoznieniem (przeciaganie suwakow i punktow nie blokuje okna).
void RequestRender(State* st) { SetTimer(st->hwnd, kRenderTimer, 15, nullptr); }

void ComputeHistograms(State* st) {
    const PreviewImage* pi = st->ctx->preview;
    std::memset(st->hist, 0, sizeof(st->hist));
    if (!pi) return;
    for (size_t i = 0; i < (size_t)pi->w * pi->h; ++i) {
        const float* s = &pi->rgba[i * 4];
        if (s[3] <= 0.0f) continue;
        for (int c = 0; c < 3; ++c) {
            int b = std::min(255, std::max(0, (int)std::lround(s[c] * 255.0f)));
            ++st->hist[kCurveR + c][b];
            ++st->hist[kCurveRGB][b];
        }
    }
}

void DrawLabel(HDC dc, HFONT font, const wchar_t* text, int x, int y, bool right) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &sz);
    int lx = right ? x - sz.cx - 10 : x + 2;
    RECT r = {lx - 4, y, lx + sz.cx + 4, y + sz.cy + 2};
    HBRUSH b = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &r, b);
    DeleteObject(b);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, lx, y + 1, text, (int)wcslen(text));
    SelectObject(dc, old);
}

LRESULT CALLBACK PreviewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    State* st = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_LBUTTONDOWN:
            if (st && st->pvMode != kPvBefore) {
                st->holdBefore = true;
                SetCapture(h);
                RenderPreview(st);
            }
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == h) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            if (st && st->holdBefore) {
                st->holdBefore = false;
                RenderPreview(st);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HBRUSH bg = CreateSolidBrush(RGB(64, 64, 64));
            FillRect(dc, &rc, bg);
            DeleteObject(bg);
            const PreviewImage* pi = st ? st->ctx->preview : nullptr;
            if (pi && pi->w > 0 && !st->bits.empty()) {
                double sc = std::min((double)rc.right / pi->w, (double)rc.bottom / pi->h);
                int dw = std::max(1, (int)(pi->w * sc)), dh = std::max(1, (int)(pi->h * sc));
                int dx = (rc.right - dw) / 2, dy = (rc.bottom - dh) / 2;
                BITMAPINFO bi = {};
                bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
                bi.bmiHeader.biWidth = pi->w;
                bi.bmiHeader.biHeight = -pi->h;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                SetStretchBltMode(dc, HALFTONE);
                StretchDIBits(dc, dx, dy, dw, dh, 0, 0, pi->w, pi->h, st->bits.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
                const int mode = st->holdBefore ? kPvBefore : st->pvMode;
                const int ly = dy + S(st, 6);
                if (mode == kPvSplit) {
                    const int mx = dx + dw / 2;
                    DrawLabel(dc, st->smallFont, T(Str::PvBeforeLabel), mx, ly, true);
                    DrawLabel(dc, st->smallFont, T(Str::PvAfterLabel), mx + S(st, 8), ly, false);
                } else {
                    DrawLabel(dc, st->smallFont, mode == kPvBefore ? T(Str::PvBeforeLabel) : T(Str::PvAfterLabel),
                              dx + S(st, 6), ly, false);
                }
            } else {
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(230, 230, 230));
                if (st) SelectObject(dc, st->font);
                DrawTextW(dc, T(Str::NoPreview), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------------------------------------------------------------------------------------
// Budowa kontrolek

HWND Make(State* st, int page, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h,
          int id, DWORD ex = 0, HFONT font = nullptr) {
    const DWORD vis = (page < 0 || page == st->page) ? WS_VISIBLE : 0;
    HWND c = CreateWindowExW(ex, cls, text, style | WS_CHILD | WS_CLIPSIBLINGS | vis, S(st, x), S(st, y), S(st, w),
                             S(st, h), st->hwnd, (HMENU)(INT_PTR)id, st->ctx->instance, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)(font ? font : st->font), TRUE);
    if (page >= 0) st->pageCtl.push_back({c, page});
    return c;
}

void FillCombo(HWND c, std::initializer_list<const wchar_t*> items, int sel) {
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (auto s : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
    SendMessageW(c, CB_SETCURSEL, sel, 0);
}

HWND MakeSlider(State* st, int page, int x, int y, int id, int value) {
    HWND tb = Make(st, page, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, x, y, 210, 28, id);
    SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(tb, TBM_SETPOS, TRUE, value);
    return tb;
}

void ShowPage(State* st, int page) {
    st->page = page;
    for (auto& pc : st->pageCtl) ShowWindow(pc.first, pc.second == page ? SW_SHOW : SW_HIDE);
    if (st->tabs) TabCtrl_SetCurSel(st->tabs, page);
}

void UpdateLabels(State* st) {
    SetDlgItemTextW(st->hwnd, IDC_STRENGTH_LBL, Fmt(L"%d%%", st->p.strength).c_str());
    SetDlgItemTextW(st->hwnd, IDC_LUT_STRENGTH_LBL, Fmt(L"%d%%", st->p.lutStrength).c_str());
}

void UpdateLutInfo(State* st) {
    std::wstring name = st->p.lutPath[0] ? FileName(st->p.lutPath) : std::wstring(T(Str::LutNone));
    SetDlgItemTextW(st->hwnd, IDC_LUT_NAME, name.c_str());
    std::wstring info;
    if (!st->lutError.empty()) {
        info = st->lutError;
    } else if (st->lut.valid()) {
        if (st->lut.size3d > 0) info = Fmt(T(Str::LutGrid3D), st->lut.size3d);
        if (st->lut.size1d > 0) info += (info.empty() ? L"" : L" + ") + Fmt(T(Str::LutGrid1D), st->lut.size1d);
        if (!st->lut.title.empty()) info += L" · " + Utf8ToWide(st->lut.title);
    }
    SetDlgItemTextW(st->hwnd, IDC_LUT_INFO, info.c_str());
    const bool has = st->lut.valid();
    EnableWindow(GetDlgItem(st->hwnd, IDC_LUT_CLEAR), has);
    EnableWindow(GetDlgItem(st->hwnd, IDC_LUT_STRENGTH), has);
    EnableWindow(GetDlgItem(st->hwnd, IDC_LUT_INTERP), has);
}

// Pola Wejscie/Wyjscie dla zaznaczonego punktu krzywej.
void UpdateCurveFields(State* st) {
    st->updating = true;
    const Curve& c = st->p.curves.c[st->curveModel.channel];
    const int sel = st->curveModel.selected;
    const bool has = sel >= 0 && sel < c.n;
    SetDlgItemTextW(st->hwnd, IDC_CURVE_IN, has ? std::to_wstring((int)std::lround(c.p[sel].x * 255)).c_str() : L"");
    SetDlgItemTextW(st->hwnd, IDC_CURVE_OUT, has ? std::to_wstring((int)std::lround(c.p[sel].y * 255)).c_str() : L"");
    EnableWindow(GetDlgItem(st->hwnd, IDC_CURVE_IN), has && sel > 0 && sel < c.n - 1);
    EnableWindow(GetDlgItem(st->hwnd, IDC_CURVE_OUT), has);
    st->updating = false;
}

void SetCurveChannel(State* st, int ch) {
    st->curveModel.channel = ch;
    st->curveModel.selected = -1;
    st->curveModel.histogram = st->ctx->preview ? st->hist[ch] : nullptr;
    UpdateCurveFields(st);
    if (st->curveEditor) CurveEditorRefresh(st->curveEditor);
}

void CurvesChanged(State* st) {
    if (st->curveEditor) CurveEditorRefresh(st->curveEditor);
    UpdateCurveFields(st);
    RequestRender(st);
}

void FillLibraryList(State* st);

void BuildCurvesPage(State* st, int x0, int y0) {
    const int P = kPageCurves;
    Make(st, P, L"BUTTON", T(Str::CurvesOn), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y0 + 2, 170, 22, IDC_CURVES_ON);
    CheckDlgButton(st->hwnd, IDC_CURVES_ON, st->p.curvesOn ? BST_CHECKED : BST_UNCHECKED);
    Make(st, P, L"STATIC", T(Str::CurveChannel), SS_RIGHT, x0 + 175, y0 + 5, 60, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, x0 + 240, y0, 124, 200, IDC_CURVE_CH),
              {T(Str::ChRGB), T(Str::ChRed), T(Str::ChGreen), T(Str::ChBlue)}, st->curveModel.channel);
    const int ed = 330;
    st->curveEditor = CreateCurveEditor(st->hwnd, st->ctx->instance, IDC_CURVE_EDITOR, S(st, x0 + (364 - ed) / 2),
                                        S(st, y0 + 30), S(st, ed), &st->curveModel);
    SetWindowLongW(st->curveEditor, GWL_STYLE, GetWindowLongW(st->curveEditor, GWL_STYLE) | WS_CLIPSIBLINGS);
    if (st->page != P) ShowWindow(st->curveEditor, SW_HIDE);
    st->pageCtl.push_back({st->curveEditor, P});
    int y = y0 + 30 + ed + 6;
    Make(st, P, L"STATIC", T(Str::CurveInput), 0, x0, y + 3, 60, 20, -1);
    Make(st, P, L"EDIT", L"", ES_NUMBER | WS_TABSTOP | ES_RIGHT, x0 + 62, y, 50, 22, IDC_CURVE_IN, WS_EX_CLIENTEDGE);
    Make(st, P, L"STATIC", T(Str::CurveOutput), 0, x0 + 130, y + 3, 60, 20, -1);
    Make(st, P, L"EDIT", L"", ES_NUMBER | WS_TABSTOP | ES_RIGHT, x0 + 192, y, 50, 22, IDC_CURVE_OUT, WS_EX_CLIENTEDGE);
    SendDlgItemMessageW(st->hwnd, IDC_CURVE_IN, EM_SETLIMITTEXT, 3, 0);
    SendDlgItemMessageW(st->hwnd, IDC_CURVE_OUT, EM_SETLIMITTEXT, 3, 0);
    y += 26;
    Make(st, P, L"STATIC", T(Str::CurveHint), 0, x0, y, 364, 28, -1, 0, st->smallFont);
    y += 32;
    Make(st, P, L"BUTTON", T(Str::CurveLuma), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y, 364, 22, IDC_CURVE_LUMA);
    CheckDlgButton(st->hwnd, IDC_CURVE_LUMA, st->p.curves.lumaOnly ? BST_CHECKED : BST_UNCHECKED);
    y += 24;
    Make(st, P, L"BUTTON", T(Str::CurveLogHist), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y, 364, 22, IDC_CURVE_LOG);
    CheckDlgButton(st->hwnd, IDC_CURVE_LOG, st->curveModel.logHistogram ? BST_CHECKED : BST_UNCHECKED);
    y += 28;
    Make(st, P, L"STATIC", T(Str::CurvePresetLabel), 0, x0, y + 3, 60, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0 + 62, y, 302, 300,
                   IDC_CURVE_PRESET),
              {T(Str::CurvePresetPick), T(Str::PresetSoftS), T(Str::PresetMediumS), T(Str::PresetStrongS),
               T(Str::PresetMatte), T(Str::PresetLiftShadows), T(Str::PresetTameHighlights), T(Str::PresetCross),
               T(Str::PresetNegative)},
              0);
    y += 32;
    Make(st, P, L"BUTTON", T(Str::CurveResetChannel), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 178, 26, IDC_CURVE_RESET_CH);
    Make(st, P, L"BUTTON", T(Str::CurveResetAll), BS_PUSHBUTTON | WS_TABSTOP, x0 + 186, y, 178, 26, IDC_CURVE_RESET_ALL);
    SetCurveChannel(st, st->curveModel.channel);
}

// Suwak powiazany z polem parametrow: etykieta z nazwa, suwak, wartosc liczbowa.
void MakeBound(State* st, int page, int x0, int y, const wchar_t* label, int labelW, int sliderW, int id, int lbl,
               int32_t* field, int lo, int hi, const wchar_t* fmt) {
    Make(st, page, L"STATIC", label, 0, x0, y + 4, labelW, 20, -1);
    HWND tb = Make(st, page, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, x0 + labelW, y, sliderW, 28, id);
    SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendMessageW(tb, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(tb, TBM_SETPOS, TRUE, *field);
    Make(st, page, L"STATIC", L"", SS_RIGHT, x0 + labelW + sliderW, y + 4, 48, 20, lbl);
    st->sliders.push_back({id, lbl, field, fmt});
}

void UpdateBoundLabel(State* st, const SliderBind& b) {
    SetDlgItemTextW(st->hwnd, b.lbl, Fmt(b.fmt, *b.field).c_str());
}

// Ustawia pozycje wszystkich suwakow z parametrow (po presetach / resecie).
void SyncSliders(State* st) {
    for (const auto& b : st->sliders) {
        SendDlgItemMessageW(st->hwnd, b.id, TBM_SETPOS, TRUE, *b.field);
        UpdateBoundLabel(st, b);
    }
    for (int id : {IDC_BW_HUE_SW, IDC_TN_HI_SW, IDC_TN_SH_SW, IDC_DUO_SH_SW, IDC_DUO_HI_SW, IDC_DUO_GRAD})
        InvalidateRect(GetDlgItem(st->hwnd, id), nullptr, TRUE);
}

// 0xRRGGBB <-> COLORREF (0x00BBGGRR) i tekst HEX.
COLORREF ToColorRef(uint32_t rgb) { return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }
uint32_t FromColorRef(COLORREF c) { return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c); }

// Pola HEX duotonu (bez wywolywania obslugi EN_CHANGE).
void SyncDuotoneFields(State* st) {
    st->updating = true;
    SetDlgItemTextW(st->hwnd, IDC_DUO_SH_HEX, Fmt(L"%06X", st->p.duotone.shadow).c_str());
    SetDlgItemTextW(st->hwnd, IDC_DUO_HI_HEX, Fmt(L"%06X", st->p.duotone.highlight).c_str());
    CheckDlgButton(st->hwnd, IDC_DUO_ON, st->p.duotone.on ? BST_CHECKED : BST_UNCHECKED);
    st->updating = false;
    SyncSliders(st);
}

// Pasek podgladu gradientu duotonu (z zaznaczonym punktem srodkowym).
LRESULT CALLBACK GradProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg != WM_PAINT) return DefWindowProcW(h, msg, wp, lp);
    State* st = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    if (st) {
        const DuotoneParams& d = st->p.duotone;
        const int w = std::max<LONG>(2, rc.right), barH = rc.bottom * 2 / 3;
        for (int x = 0; x < w; ++x) {
            // Wejscie: szarosc x/(w-1); wynik jak w efekcie (sila 100%).
            float v = x / (float)(w - 1), c[3] = {v, v, v};
            DuotoneParams full = d;
            full.on = 1;
            full.amount = 100;
            ApplyDuotone(full, st->p.bw.mix, c);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255)));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, x, 0, nullptr);
            LineTo(dc, x, barH);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        RECT below = {0, barH, rc.right, rc.bottom};
        FillRect(dc, &below, GetSysColorBrush(COLOR_BTNFACE));
        // Romb punktu srodkowego, jak w edytorze gradientu Photoshopa.
        const int mx = (int)std::lround(d.midpoint / 100.0 * (w - 1)), s = (rc.bottom - barH) - 1;
        POINT pts[4] = {{mx, barH}, {mx + s, barH + s}, {mx, barH + 2 * s}, {mx - s, barH + s}};
        HGDIOBJ ob = SelectObject(dc, GetStockObject(BLACK_BRUSH));
        Polygon(dc, pts, 4);
        SelectObject(dc, ob);
    }
    EndPaint(h, &ps);
    return 0;
}

// Okno wyboru koloru (systemowe), wynik 0xRRGGBB.
bool PickColor(State* st, uint32_t& rgb) {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = st->hwnd;
    cc.rgbResult = ToColorRef(rgb);
    cc.lpCustColors = custom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (!ChooseColorW(&cc)) return false;
    rgb = FromColorRef(cc.rgbResult);
    return true;
}

void BuildDuotonePage(State* st, int x0, int y0) {
    const int P = kPageDuotone;
    int y = y0;
    Make(st, P, L"BUTTON", T(Str::DuoOn), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y + 2, 364, 22, IDC_DUO_ON);
    y += 32;
    const struct { Str label; int sw, hex, pick; } rows[2] = {
        {Str::DuoShadow, IDC_DUO_SH_SW, IDC_DUO_SH_HEX, IDC_DUO_SH_PICK},
        {Str::DuoHighlight, IDC_DUO_HI_SW, IDC_DUO_HI_HEX, IDC_DUO_HI_PICK},
    };
    for (const auto& r : rows) {
        Make(st, P, L"STATIC", T(r.label), 0, x0, y + 4, 100, 20, -1);
        Make(st, P, L"STATIC", L"", SS_SUNKEN | SS_NOTIFY, x0 + 100, y, 60, 26, r.sw);
        Make(st, P, L"STATIC", L"#", SS_RIGHT, x0 + 164, y + 4, 12, 20, -1);
        HWND e = Make(st, P, L"EDIT", L"", ES_UPPERCASE | WS_TABSTOP, x0 + 178, y + 2, 66, 22, r.hex, WS_EX_CLIENTEDGE);
        SendMessageW(e, EM_SETLIMITTEXT, 6, 0);
        Make(st, P, L"BUTTON", T(Str::DuoPick), BS_PUSHBUTTON | WS_TABSTOP, x0 + 254, y, 110, 26, r.pick);
        y += 32;
    }
    HWND g = CreateWindowExW(0, kGradClass, L"", WS_CHILD | WS_CLIPSIBLINGS | (st->page == P ? WS_VISIBLE : 0), S(st, x0),
                             S(st, y), S(st, 364), S(st, 30), st->hwnd, (HMENU)(INT_PTR)IDC_DUO_GRAD, st->ctx->instance,
                             nullptr);
    SetWindowLongPtrW(g, GWLP_USERDATA, (LONG_PTR)st);
    st->pageCtl.push_back({g, P});
    y += 36;
    Make(st, P, L"BUTTON", T(Str::DuoSwap), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 178, 26, IDC_DUO_SWAP);
    y += 34;
    MakeBound(st, P, x0, y, T(Str::DuoMidpoint), 100, 216, IDC_DUO_MID, IDC_DUO_MID_LBL, &st->p.duotone.midpoint, 5, 95,
              L"%d%%");
    y += 30;
    MakeBound(st, P, x0, y, T(Str::DuoAmount), 100, 216, IDC_DUO_AMT, IDC_DUO_AMT_LBL, &st->p.duotone.amount, 0, 100,
              L"%d%%");
    y += 36;
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0, y, 364, 300, IDC_DUO_PRESET),
              {T(Str::DuoPresetPick), T(Str::DuoQuickStart), T(Str::DuoNavyOrange), T(Str::DuoGreenGold),
               T(Str::DuoPurplePink), T(Str::DuoBrownCream), T(Str::DuoBlackTeal)},
              0);
    y += 36;
    Make(st, P, L"STATIC", T(Str::DuoHint), 0, x0, y, 364, 72, -1, 0, st->smallFont);
    y += 78;
    Make(st, P, L"BUTTON", T(Str::DuoReset), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 178, 26, IDC_DUO_RESET);
    SyncDuotoneFields(st);
}

// Wpisany kod HEX (6 cyfr) zmienia kolor; niepelny kod jest ignorowany do czasu dokonczenia.
void OnDuotoneHex(State* st, int id) {
    if (st->updating) return;
    wchar_t b[16];
    GetDlgItemTextW(st->hwnd, id, b, 16);
    if (wcslen(b) != 6) return;
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(b, &end, 16);
    if (!end || *end) return;
    (id == IDC_DUO_SH_HEX ? st->p.duotone.shadow : st->p.duotone.highlight) = (uint32_t)v & 0xFFFFFF;
    for (int sw : {IDC_DUO_SH_SW, IDC_DUO_HI_SW, IDC_DUO_GRAD}) InvalidateRect(GetDlgItem(st->hwnd, sw), nullptr, TRUE);
}

void UpdateBwEnable(State* st) {
    const int mode = st->p.bw.mode;
    const bool on = mode > 0, tint = mode == (int)BwMode::Sepia || mode == (int)BwMode::Monochrome;
    EnableWindow(GetDlgItem(st->hwnd, IDC_BW_FILTER), on);
    for (int i = 0; i < kBwColorCount; ++i) EnableWindow(GetDlgItem(st->hwnd, IDC_BW_MIX + i), on);
    EnableWindow(GetDlgItem(st->hwnd, IDC_BW_HUE), mode == (int)BwMode::Monochrome);
    EnableWindow(GetDlgItem(st->hwnd, IDC_BW_SAT), tint);
    InvalidateRect(GetDlgItem(st->hwnd, IDC_BW_HUE_SW), nullptr, TRUE);
}

void UpdateToningEnable(State* st) {
    const bool on = st->p.toning.on != 0;
    for (int id : {IDC_TN_HI_HUE, IDC_TN_HI_SAT, IDC_TN_SH_HUE, IDC_TN_SH_SAT, IDC_TN_BAL})
        EnableWindow(GetDlgItem(st->hwnd, id), on);
}

// Kolor probki dla suwaka odcienia (sepia: staly odcien).
COLORREF SwatchColor(State* st, int which) {
    float hue = 0, sat = 1;
    if (which == 0) {
        hue = (float)(st->p.bw.mode == (int)BwMode::Sepia ? kSepiaHue : st->p.bw.tintHue);
        sat = st->p.bw.mode == (int)BwMode::Sepia || st->p.bw.mode == (int)BwMode::Monochrome ? 1.0f : 0.0f;
    } else if (which == 1) {
        hue = (float)st->p.toning.hiHue;
    } else if (which == 2) {
        hue = (float)st->p.toning.shHue;
    } else {
        return ToColorRef(which == 3 ? st->p.duotone.shadow : st->p.duotone.highlight);
    }
    float c[3];
    HueColor(hue, c);
    for (int k = 0; k < 3; ++k) c[k] = 0.5f + (c[k] - 0.5f) * sat;
    return RGB((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255));
}

void BuildBwPage(State* st, int x0, int y0) {
    const int P = kPageBw;
    int y = y0;
    Make(st, P, L"STATIC", T(Str::BwMode), 0, x0, y + 3, 90, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, x0 + 90, y, 274, 200, IDC_BW_MODE),
              {T(Str::BwOff), T(Str::BwBlackWhite), T(Str::BwSepia), T(Str::BwMonochrome)}, st->p.bw.mode);
    y += 32;
    Make(st, P, L"STATIC", T(Str::BwFilterLabel), 0, x0, y + 3, 90, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0 + 90, y, 274, 300,
                   IDC_BW_FILTER),
              {T(Str::BwFilterPick), T(Str::BwFilterDefault), T(Str::BwFilterRed), T(Str::BwFilterOrange),
               T(Str::BwFilterYellow), T(Str::BwFilterGreen), T(Str::BwFilterBlue), T(Str::BwFilterContrast)},
              0);
    y += 34;
    const Str names[kBwColorCount] = {Str::BwRed, Str::BwYellow, Str::BwGreen, Str::BwCyan, Str::BwBlue, Str::BwMagenta};
    for (int i = 0; i < kBwColorCount; ++i, y += 28)
        MakeBound(st, P, x0, y, T(names[i]), 90, 226, IDC_BW_MIX + i, IDC_BW_MIX_LBL + i, &st->p.bw.mix[i], -200, 300,
                  L"%d%%");
    y += 6;
    Make(st, P, L"STATIC", T(Str::BwTintTitle), 0, x0, y, 364, 18, -1);
    y += 22;
    MakeBound(st, P, x0, y, T(Str::BwTintHue), 90, 186, IDC_BW_HUE, IDC_BW_HUE_LBL, &st->p.bw.tintHue, 0, 360, L"%d°");
    Make(st, P, L"STATIC", L"", SS_SUNKEN, x0 + 330, y + 3, 34, 22, IDC_BW_HUE_SW);
    y += 30;
    MakeBound(st, P, x0, y, T(Str::BwTintSat), 90, 226, IDC_BW_SAT, IDC_BW_SAT_LBL, &st->p.bw.tintSat, 0, 100, L"%d%%");
    y += 34;
    Make(st, P, L"STATIC", T(Str::BwHint), 0, x0, y, 364, 44, -1, 0, st->smallFont);
    y += 48;
    Make(st, P, L"BUTTON", T(Str::BwReset), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 178, 26, IDC_BW_RESET);
    UpdateBwEnable(st);
}

void BuildToningPage(State* st, int x0, int y0) {
    const int P = kPageToning;
    int y = y0;
    Make(st, P, L"BUTTON", T(Str::ToningOn), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y + 2, 364, 22, IDC_TN_ON);
    CheckDlgButton(st->hwnd, IDC_TN_ON, st->p.toning.on ? BST_CHECKED : BST_UNCHECKED);
    y += 32;
    Make(st, P, L"STATIC", T(Str::ToningHighlights), 0, x0, y, 364, 18, -1);
    y += 22;
    MakeBound(st, P, x0, y, T(Str::BwTintHue), 90, 186, IDC_TN_HI_HUE, IDC_TN_HI_HUE_LBL, &st->p.toning.hiHue, 0, 360,
              L"%d°");
    Make(st, P, L"STATIC", L"", SS_SUNKEN, x0 + 330, y + 3, 34, 22, IDC_TN_HI_SW);
    y += 30;
    MakeBound(st, P, x0, y, T(Str::BwTintSat), 90, 226, IDC_TN_HI_SAT, IDC_TN_HI_SAT_LBL, &st->p.toning.hiSat, 0, 100,
              L"%d%%");
    y += 36;
    Make(st, P, L"STATIC", T(Str::ToningShadows), 0, x0, y, 364, 18, -1);
    y += 22;
    MakeBound(st, P, x0, y, T(Str::BwTintHue), 90, 186, IDC_TN_SH_HUE, IDC_TN_SH_HUE_LBL, &st->p.toning.shHue, 0, 360,
              L"%d°");
    Make(st, P, L"STATIC", L"", SS_SUNKEN, x0 + 330, y + 3, 34, 22, IDC_TN_SH_SW);
    y += 30;
    MakeBound(st, P, x0, y, T(Str::BwTintSat), 90, 226, IDC_TN_SH_SAT, IDC_TN_SH_SAT_LBL, &st->p.toning.shSat, 0, 100,
              L"%d%%");
    y += 38;
    MakeBound(st, P, x0, y, T(Str::ToningBalance), 90, 226, IDC_TN_BAL, IDC_TN_BAL_LBL, &st->p.toning.balance, -100, 100,
              L"%+d");
    y += 36;
    Make(st, P, L"STATIC", T(Str::CurvePresetLabel), 0, x0, y + 3, 90, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0 + 90, y, 274, 300,
                   IDC_TN_PRESET),
              {T(Str::ToningPresetPick), T(Str::ToningClassic), T(Str::ToningTealOrange), T(Str::ToningSelenium),
               T(Str::ToningGold), T(Str::ToningCold)},
              0);
    y += 36;
    Make(st, P, L"STATIC", T(Str::ToningHint), 0, x0, y, 364, 44, -1, 0, st->smallFont);
    y += 48;
    Make(st, P, L"BUTTON", T(Str::ToningReset), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 178, 26, IDC_TN_RESET);
    UpdateToningEnable(st);
}

void BuildLutPage(State* st, int x0, int y0) {
    const int P = kPageLut;
    int y = y0;
    Make(st, P, L"STATIC", T(Str::LutFile), 0, x0, y + 4, 150, 20, -1);
    Make(st, P, L"BUTTON", T(Str::LutChoose), BS_PUSHBUTTON | WS_TABSTOP, x0 + 150, y, 120, 26, IDC_LUT_CHOOSE);
    Make(st, P, L"BUTTON", T(Str::LutClear), BS_PUSHBUTTON | WS_TABSTOP, x0 + 274, y, 90, 26, IDC_LUT_CLEAR);
    y += 32;
    Make(st, P, L"STATIC", L"", SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS, x0, y, 364, 20, IDC_LUT_NAME);
    y += 20;
    Make(st, P, L"STATIC", L"", SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS, x0, y, 364, 20, IDC_LUT_INFO, 0, st->smallFont);
    y += 28;
    Make(st, P, L"STATIC", T(Str::LutStrength), 0, x0, y + 3, 110, 20, -1);
    MakeSlider(st, P, x0 + 110, y, IDC_LUT_STRENGTH, st->p.lutStrength);
    Make(st, P, L"STATIC", L"", 0, x0 + 324, y + 3, 40, 20, IDC_LUT_STRENGTH_LBL);
    y += 36;
    Make(st, P, L"STATIC", T(Str::LutInterp), 0, x0, y + 3, 110, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, x0 + 150, y, 214, 200, IDC_LUT_INTERP),
              {T(Str::InterpTetra), T(Str::InterpTri)}, st->p.lutTrilinear ? 1 : 0);
    y += 36;
    Make(st, P, L"STATIC", T(Str::LutLogNote), 0, x0, y, 364, 30, -1, 0, st->smallFont);
}

void BuildLibraryPage(State* st, int x0, int y0) {
    const int P = kPageLibrary;
    int y = y0;
    Make(st, P, L"STATIC", T(Str::LibFolder), 0, x0, y + 4, 150, 20, -1);
    Make(st, P, L"BUTTON", T(Str::LibChangeFolder), BS_PUSHBUTTON | WS_TABSTOP, x0 + 180, y, 90, 26, IDC_LIB_CHANGE);
    Make(st, P, L"BUTTON", T(Str::LibOpenFolder), BS_PUSHBUTTON | WS_TABSTOP, x0 + 274, y, 90, 26, IDC_LIB_OPEN);
    y += 28;
    Make(st, P, L"STATIC", st->libDir.c_str(), SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS, x0, y, 364, 18, IDC_LIB_DIR, 0,
         st->smallFont);
    y += 24;
    Make(st, P, L"STATIC", T(Str::LibSearch), 0, x0, y + 3, 80, 20, -1);
    st->updating = true;
    Make(st, P, L"EDIT", st->search.c_str(), ES_AUTOHSCROLL | WS_TABSTOP, x0 + 82, y, 282, 22, IDC_LIB_SEARCH,
         WS_EX_CLIENTEDGE);
    st->updating = false;
    y += 28;
    Make(st, P, L"STATIC", T(Str::LibCategory), 0, x0, y + 3, 80, 20, -1);
    HWND cat = Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0 + 82, y, 282, 300,
                    IDC_LIB_CAT);
    SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)T(Str::LibAllCategories));
    SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)T(Str::LibMainFolder));
    for (const auto& c : st->categories) SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)c.c_str());
    if (st->category >= 2 + (int)st->categories.size()) st->category = 0;
    SendMessageW(cat, CB_SETCURSEL, st->category, 0);
    y += 32;
    st->list = Make(st, P, WC_LISTVIEWW, L"",
                    LVS_ICON | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE | LVS_SHAREIMAGELISTS | WS_TABSTOP, x0, y, 364,
                    676 - 96 - y, IDC_LIB_LIST, WS_EX_CLIENTEDGE, st->smallFont);
    ListView_SetExtendedListViewStyle(st->list, LVS_EX_DOUBLEBUFFER);
    ListView_SetIconSpacing(st->list, kThumbW + 14, kThumbH + 36);
    y = 676 - 92;
    Make(st, P, L"STATIC", L"", 0, x0, y, 364, 30, IDC_LIB_COUNT, 0, st->smallFont);
    y += 34;
    Make(st, P, L"BUTTON", T(Str::LibAdd), BS_PUSHBUTTON | WS_TABSTOP, x0, y, 130, 26, IDC_LIB_ADD);
    Make(st, P, L"BUTTON", T(Str::LibDelete), BS_PUSHBUTTON | WS_TABSTOP, x0 + 136, y, 110, 26, IDC_LIB_DELETE);
    Make(st, P, L"BUTTON", T(Str::LibRefresh), BS_PUSHBUTTON | WS_TABSTOP, x0 + 252, y, 112, 26, IDC_LIB_REFRESH);
    FillLibraryList(st);
}

void BuildTestPage(State* st, int x0, int y0) {
    const int P = kPageTest;
    int y = y0;
    Make(st, P, L"STATIC", T(Str::Effect), 0, x0, y + 3, 150, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x0 + 150, y, 214, 200, IDC_EFFECT),
              {T(Str::EffectNone), T(Str::EffectSCurve), T(Str::EffectInvert), T(Str::EffectGray)}, st->p.effect);
    y += 32;
    Make(st, P, L"STATIC", T(Str::Strength), 0, x0, y + 3, 110, 20, -1);
    MakeSlider(st, P, x0 + 110, y, IDC_STRENGTH, st->p.strength);
    Make(st, P, L"STATIC", L"", 0, x0 + 324, y + 3, 40, 20, IDC_STRENGTH_LBL);
    y += 34;
    Make(st, P, L"STATIC", T(Str::MaskMode), 0, x0, y + 3, 150, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, x0 + 150, y, 214, 200, IDC_MASKMODE),
              {T(Str::MaskHost), T(Str::MaskOwn)}, st->p.maskMode);
    y += 32;
    Make(st, P, L"STATIC", T(Str::Tiles), 0, x0, y + 3, 150, 20, -1);
    FillCombo(Make(st, P, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, x0 + 150, y, 214, 200, IDC_TILEMODE),
              {T(Str::TilesStrips), T(Str::Tiles256), T(Str::Tiles1024)}, st->p.tileMode);
    y += 32;
    Make(st, P, L"BUTTON", T(Str::MarkTiles), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y, 364, 22, IDC_MARK);
    CheckDlgButton(st->hwnd, IDC_MARK, st->p.markTiles ? BST_CHECKED : BST_UNCHECKED);
    y += 24;
    Make(st, P, L"BUTTON", T(Str::Dither), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y, 364, 22, IDC_DITHER);
    CheckDlgButton(st->hwnd, IDC_DITHER, st->p.dither ? BST_CHECKED : BST_UNCHECKED);
    y += 24;
    Make(st, P, L"BUTTON", T(Str::ShowSel), BS_AUTOCHECKBOX | WS_TABSTOP, x0, y, 364, 22, IDC_SHOWSEL);
    CheckDlgButton(st->hwnd, IDC_SHOWSEL, st->showSel ? BST_CHECKED : BST_UNCHECKED);
    y += 30;
    Make(st, P, L"STATIC", T(Str::DiagLabel), 0, x0, y, 364, 30, -1, 0, st->smallFont);
    y += 32;
    HWND diag = Make(st, P, L"EDIT", st->ctx->diagnostics.c_str(),
                     ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
                     x0, y, 364, 676 - 36 - y, IDC_DIAG, WS_EX_CLIENTEDGE, st->smallFont);
    SendMessageW(diag, EM_SETLIMITTEXT, 0, 0);
    Make(st, P, L"BUTTON", T(Str::CopyDiag), BS_PUSHBUTTON | WS_TABSTOP, x0, 676 - 30, 170, 26, IDC_COPY);
}

void Build(State* st) {
    st->pageCtl.clear();
    st->preview = CreateWindowExW(0, kPreviewClass, L"", WS_CHILD | WS_VISIBLE, S(st, 10), S(st, 10), S(st, 680),
                                  S(st, 660), st->hwnd, nullptr, st->ctx->instance, nullptr);
    SetWindowLongPtrW(st->preview, GWLP_USERDATA, (LONG_PTR)st);

    const int px = 700;
    Make(st, -1, L"STATIC", T(Str::Language), 0, px, 13, 160, 20, -1);
    FillCombo(Make(st, -1, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, px + 165, 10, 215, 200, IDC_LANG),
              {T(Str::LangAuto), T(Str::LangPl), T(Str::LangEn)}, st->language);
    Make(st, -1, L"STATIC", T(Str::PreviewMode), 0, px, 45, 160, 20, -1);
    FillCombo(Make(st, -1, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, px + 165, 42, 215, 200, IDC_PVMODE),
              {T(Str::PvAfter), T(Str::PvBefore), T(Str::PvSplit)}, st->pvMode);
    Make(st, -1, L"STATIC", T(Str::PvHint), 0, px, 70, 380, 16, -1, 0, st->smallFont);

    // Zakladki: sam pasek naglowkow, strony leza na tle okna (bez problemow z kolorem tla motywu).
    st->tabs = Make(st, -1, WC_TABCONTROLW, L"", WS_TABSTOP, px, 90, 380, 26, IDC_TABS);
    const wchar_t* names[kPageCount] = {T(Str::TabCurves), T(Str::TabBw),      T(Str::TabDuotone), T(Str::TabLut),
                                        T(Str::TabToning), T(Str::TabLibrary), T(Str::TabTest)};
    // Zakladka "Test" (diagnostyka kroku 0) tylko w trybie diagnostycznym (debug.flag).
    const int tabCount = DebugEnabled() ? kPageCount : kPageCount - 1;
    if (st->page >= tabCount) st->page = 0;
    for (int i = 0; i < tabCount; ++i) {
        TCITEMW ti = {};
        ti.mask = TCIF_TEXT;
        ti.pszText = (LPWSTR)names[i];
        TabCtrl_InsertItem(st->tabs, i, &ti);
    }
    st->sliders.clear();
    const int x0 = px + 8, y0 = 124;
    BuildCurvesPage(st, x0, y0);
    BuildBwPage(st, x0, y0);
    BuildDuotonePage(st, x0, y0);
    BuildLutPage(st, x0, y0);
    BuildToningPage(st, x0, y0);
    BuildLibraryPage(st, x0, y0);
    BuildTestPage(st, x0, y0);

    // Pod podgladem: zapis / odczyt wygladu (preset Look) i eksport calego potoku do LUT-a .cube.
    Make(st, -1, L"BUTTON", T(Str::LookSave), BS_PUSHBUTTON | WS_TABSTOP, 10, 684, 150, 26, IDC_LOOK_SAVE);
    Make(st, -1, L"BUTTON", T(Str::LookLoad), BS_PUSHBUTTON | WS_TABSTOP, 166, 684, 150, 26, IDC_LOOK_LOAD);
    Make(st, -1, L"BUTTON", T(Str::ExportCube), BS_PUSHBUTTON | WS_TABSTOP, 322, 684, 170, 26, IDC_EXPORT_CUBE);

    // Dol: sila ogolna (miesza caly wynik z oryginalem) oraz OK / Anuluj.
    MakeBound(st, -1, px, 684, T(Str::Overall), 78, 116, IDC_OVERALL, IDC_OVERALL_LBL, &st->p.overall, 0, 100, L"%d%%");
    Make(st, -1, L"BUTTON", T(Str::Ok), BS_DEFPUSHBUTTON | WS_TABSTOP, px + 250, 684, 62, 26, IDC_OKB);
    Make(st, -1, L"BUTTON", T(Str::Cancel), BS_PUSHBUTTON | WS_TABSTOP, px + 316, 684, 64, 26, IDC_CANCELB);
    SyncSliders(st);
    UpdateLabels(st);
    UpdateLutInfo(st);
    ShowPage(st, st->page);
}

// ---------------------------------------------------------------------------------------------
// LUT

// Wczytuje LUT do stanu. Przy bledzie poprzedni LUT zostaje, a blad jest pokazany pod nazwa pliku.
bool LoadLut(State* st, const std::wstring& path) {
    st->pendingLut.clear();
    CubeLut lut;
    CubeResult r = LoadCubeFile(path, lut);
    if (r.error != CubeError::Ok) {
        st->lutError = FileName(path) + L": " + CubeErrorText(r);
        UpdateLutInfo(st);
        return false;
    }
    st->lut = std::move(lut);
    st->lutError.clear();
    wcsncpy_s(st->p.lutPath, path.c_str(), _TRUNCATE);
    UpdateLutInfo(st);
    RequestRender(st);
    return true;
}

std::wstring DefaultLutDir(const State* st) {
    if (st->p.lutPath[0]) {
        std::wstring p = st->p.lutPath;
        size_t s = p.find_last_of(L"\\/");
        if (s != std::wstring::npos) return p.substr(0, s);
    }
    std::wstring last = LoadSettings().lastLutDir;
    if (!last.empty() && GetFileAttributesW(last.c_str()) != INVALID_FILE_ATTRIBUTES) return last;
    return st->libDir;
}

void ChooseLut(State* st) {
    wchar_t file[1024] = L"";
    std::wstring filter = std::wstring(T(Str::LutFilter)) + L'\0' + L"*.cube" + L'\0' + L'\0';
    std::wstring dir = DefaultLutDir(st);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->hwnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring path = file;
    size_t s = path.find_last_of(L"\\/");
    if (s != std::wstring::npos) {
        AppSettings set = LoadSettings();
        set.lastLutDir = path.substr(0, s);
        SaveSettings(set);
    }
    LoadLut(st, path);
}

// Wczytuje LUT zaznaczony na liscie biblioteki (jesli czeka).
void FlushPendingLut(State* st) {
    KillTimer(st->hwnd, kLutTimer);
    if (st->pendingLut.empty()) return;
    const std::wstring path = std::move(st->pendingLut);
    st->pendingLut.clear();
    LoadLut(st, path);
}

void ClearLut(State* st) {
    st->pendingLut.clear();
    st->lut = CubeLut();
    st->lutError.clear();
    st->p.lutPath[0] = 0;
    UpdateLutInfo(st);
    RequestRender(st);
}

// ---------------------------------------------------------------------------------------------
// Biblioteka

HBITMAP ThumbBitmap(const std::vector<uint32_t>& px) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = kThumbW;
    bi.bmiHeader.biHeight = -kThumbH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) std::memcpy(bits, px.data(), px.size() * 4);
    return bmp;
}

void UpdateLibraryCount(State* st) {
    std::wstring t = st->entries.empty() ? std::wstring(T(Str::LibEmpty))
                                         : Fmt(T(Str::LibCount), (int)st->entries.size(), (int)st->shown.size());
    SetDlgItemTextW(st->hwnd, IDC_LIB_COUNT, t.c_str());
}

// Wypelnia liste wg filtra; brakujace miniatury trafiaja do kolejki liczonej w tle (timer).
void FillLibraryList(State* st) {
    if (!st->list) return;
    SendMessageW(st->list, WM_SETREDRAW, FALSE, 0);
    st->updating = true;
    ListView_DeleteAllItems(st->list);
    if (st->images) ImageList_Destroy(st->images);
    st->images = ImageList_Create(kThumbW, kThumbH, ILC_COLOR32, 16, 16);
    ListView_SetImageList(st->list, st->images, LVSIL_NORMAL);
    st->shown.clear();
    st->thumbQueue.clear();
    const std::wstring q = Lower(st->search);
    std::vector<uint32_t> placeholder(st->thumbBase.size(), 0xFFD0D0D0u);
    int current = -1;
    for (int i = 0; i < (int)st->entries.size(); ++i) {
        const LutEntry& e = st->entries[i];
        if (st->category == 1 && !e.category.empty()) continue;
        if (st->category >= 2 && e.category != st->categories[st->category - 2]) continue;
        if (!q.empty() && Lower(e.name).find(q) == std::wstring::npos) continue;
        const int pos = (int)st->shown.size();
        st->shown.push_back(i);
        auto it = st->thumbs.find(e.path);
        HBITMAP bmp = ThumbBitmap(it != st->thumbs.end() ? it->second : placeholder);
        ImageList_Add(st->images, bmp, nullptr);
        DeleteObject(bmp);
        if (it == st->thumbs.end()) st->thumbQueue.push_back(pos);
        std::wstring label = (st->category == 0 && !e.category.empty()) ? e.category + L" / " + e.name : e.name;
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        item.iItem = pos;
        item.pszText = (LPWSTR)label.c_str();
        item.iImage = pos;
        item.lParam = i;
        ListView_InsertItem(st->list, &item);
        if (st->p.lutPath[0] && Lower(e.path) == Lower(st->p.lutPath)) {
            ListView_SetItemState(st->list, pos, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            current = pos;
        }
    }
    if (current >= 0) ListView_EnsureVisible(st->list, current, FALSE);
    st->updating = false;
    SendMessageW(st->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st->list, nullptr, TRUE);
    UpdateLibraryCount(st);
    if (!st->thumbQueue.empty()) SetTimer(st->hwnd, kThumbTimer, 1, nullptr);
}

void ScanLibraryNow(State* st) {
    st->libDir = st->ctx->libraryDir.empty() ? LibraryDir() : st->ctx->libraryDir;
    st->entries = ScanLibrary(st->libDir);
    st->categories = LibraryCategories(st->entries);
    if (st->category >= 2 + (int)st->categories.size()) st->category = 0;
}

// Odbudowa strony biblioteki po zmianie folderu / plikow (lista kategorii moze sie zmienic).
void RefreshLibrary(State* st) {
    ScanLibraryNow(st);
    SetDlgItemTextW(st->hwnd, IDC_LIB_DIR, st->libDir.c_str());
    HWND cat = GetDlgItem(st->hwnd, IDC_LIB_CAT);
    SendMessageW(cat, CB_RESETCONTENT, 0, 0);
    SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)T(Str::LibAllCategories));
    SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)T(Str::LibMainFolder));
    for (const auto& c : st->categories) SendMessageW(cat, CB_ADDSTRING, 0, (LPARAM)c.c_str());
    SendMessageW(cat, CB_SETCURSEL, st->category, 0);
    FillLibraryList(st);
}

void ThumbTick(State* st) {
    // Kilka miniatur na tik (do ok. 30 ms) - okno pozostaje responsywne.
    const DWORD t0 = GetTickCount();
    while (!st->thumbQueue.empty() && GetTickCount() - t0 < 30) {
        const int pos = st->thumbQueue.front();
        st->thumbQueue.erase(st->thumbQueue.begin());
        if (pos >= (int)st->shown.size()) continue;
        const LutEntry& e = st->entries[st->shown[pos]];
        CubeLut lut;
        std::vector<uint32_t> px =
            LoadCubeFile(e.path, lut).error == CubeError::Ok ? MakeThumb(st->thumbBase, &lut) : MakeErrorThumb();
        st->thumbs[e.path] = px;
        HBITMAP bmp = ThumbBitmap(px);
        ImageList_Replace(st->images, pos, bmp, nullptr);
        DeleteObject(bmp);
        ListView_RedrawItems(st->list, pos, pos);
    }
    if (st->thumbQueue.empty()) KillTimer(st->hwnd, kThumbTimer);
}

std::vector<int> SelectedEntries(State* st) {
    std::vector<int> out;
    int i = -1;
    while ((i = ListView_GetNextItem(st->list, i, LVNI_SELECTED)) >= 0)
        if (i < (int)st->shown.size()) out.push_back(st->shown[i]);
    return out;
}

void AddLibraryFiles(State* st) {
    std::vector<wchar_t> buf(65536, 0);
    std::wstring filter = std::wstring(T(Str::LutFilter)) + L'\0' + L"*.cube" + L'\0' + L'\0';
    std::wstring dir = LoadSettings().lastLutDir;
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->hwnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = (DWORD)buf.size();
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT |
                OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    // Bufor: "folder\0plik1\0plik2\0\0" albo jedna pelna sciezka.
    std::vector<std::wstring> files;
    std::wstring first = buf.data();
    const wchar_t* p = buf.data() + first.size() + 1;
    if (!*p) files.push_back(first);
    else
        for (; *p; p += wcslen(p) + 1) files.push_back(first + L"\\" + p);
    if (!files.empty()) {
        size_t s = files[0].find_last_of(L"\\/");
        AppSettings set = LoadSettings();
        set.lastLutDir = files[0].substr(0, s);
        SaveSettings(set);
    }
    const std::wstring category = st->category >= 2 ? st->categories[st->category - 2] : std::wstring();
    int added = 0;
    std::wstring failed, lastAdded;
    for (const auto& f : files) {
        std::wstring dst;
        CubeResult pr;
        DWORD err = 0;
        if (AddToLibrary(st->libDir, category, f, dst, pr, err)) {
            ++added;
            lastAdded = dst;
            continue;
        }
        failed += L"\n• " + FileName(f) + L": " +
                  (pr.error != CubeError::Ok ? CubeErrorText(pr) : Fmt(T(Str::LibAddCopyFailed), (unsigned)err));
    }
    if (added == 1 && failed.empty()) LoadLut(st, lastAdded);  // przed odswiezeniem: lista zaznaczy nowy plik
    RefreshLibrary(st);
    std::wstring msg = Fmt(T(Str::LibAdded), added);
    if (!failed.empty()) msg += L"\n\n" + std::wstring(T(Str::LibAddFailed)) + failed;
    if (!failed.empty() || added > 1) ShowInfo(st->hwnd, msg.c_str());
}

void DeleteLibraryFiles(State* st) {
    std::vector<int> sel = SelectedEntries(st);
    if (sel.empty()) return;
    std::wstring names;
    std::vector<std::wstring> paths;
    for (size_t k = 0; k < sel.size(); ++k) {
        paths.push_back(st->entries[sel[k]].path);
        if (k < 10) names += FileName(st->entries[sel[k]].path) + L"\n";
        else if (k == 10) names += L"…\n";
    }
    std::wstring q = Fmt(T(Str::LibConfirmDelete), (int)sel.size(), names.c_str());
    if (MessageBoxW(st->hwnd, q.c_str(), L"Zetmar Kolor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    // Dyski sieciowe i wymienne nie maja Kosza: tam usuniecie jest trwale, wiec dodatkowe, wyrazne pytanie.
    for (const auto& pth : paths) {
        if (!PathHasRecycleBin(pth)) {
            if (MessageBoxW(st->hwnd, T(Str::LibConfirmNoBin), L"Zetmar Kolor", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) !=
                IDYES)
                return;
            break;
        }
    }
    const bool ok = MoveToRecycleBin(st->hwnd, paths);
    // Usuniety plik nie moze zostac biezacym LUT-em (Ctrl+F by go nie znalazl).
    for (const auto& pth : paths)
        if (st->p.lutPath[0] && Lower(pth) == Lower(st->p.lutPath) &&
            GetFileAttributesW(pth.c_str()) == INVALID_FILE_ATTRIBUTES)
            ClearLut(st);
    for (const auto& pth : paths) st->thumbs.erase(pth);
    RefreshLibrary(st);
    if (!ok) ShowInfo(st->hwnd, T(Str::LibDeleteFailed));
}

void ChangeLibraryFolder(State* st) {
    const bool comInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dlg->SetTitle(T(Str::LibPickFolder));
        IShellItem* cur = nullptr;
        if (!st->libDir.empty() &&
            SUCCEEDED(SHCreateItemFromParsingName(st->libDir.c_str(), nullptr, IID_PPV_ARGS(&cur)))) {
            dlg->SetFolder(cur);
            cur->Release();
        }
        if (SUCCEEDED(dlg->Show(st->hwnd))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    SetLibraryDir(path);
                    CoTaskMemFree(path);
                    st->category = 0;
                    RefreshLibrary(st);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
    if (comInit) CoUninitialize();
}

// ---------------------------------------------------------------------------------------------

// Odtworzenie wszystkich kontrolek z biezacych parametrow i jezyka (stan filtra zostaje).
void RebuildUi(State* st) {
    SendMessageW(st->hwnd, WM_SETREDRAW, FALSE, 0);
    KillTimer(st->hwnd, kThumbTimer);
    while (HWND c = GetWindow(st->hwnd, GW_CHILD)) DestroyWindow(c);
    st->curveEditor = st->list = st->tabs = nullptr;
    st->curveModel.selected = -1;
    Build(st);
    SetWindowTextW(st->hwnd, T(Str::Title));
    SendMessageW(st->hwnd, WM_SETREDRAW, TRUE, 0);
    RenderPreview(st);
    RedrawWindow(st->hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

// Zmiana jezyka: zapis ustawienia i odtworzenie kontrolek z nowymi napisami.
void Relabel(State* st, int language) {
    st->language = language;
    AppSettings s = LoadSettings();
    s.language = language;
    SaveSettings(s);
    SetLanguage(ResolveLanguage(language));
    RebuildUi(st);
    SetFocus(GetDlgItem(st->hwnd, IDC_LANG));
}

// Okno wyboru pliku do zapisu / odczytu (presety Look i eksport .cube). Zwraca false po anulowaniu.
bool PickFile(State* st, bool save, const wchar_t* filterName, const wchar_t* pattern, const wchar_t* defExt,
              const std::wstring& initialDir, const std::wstring& suggestedName, std::wstring& out) {
    wchar_t file[1024] = L"";
    wcsncpy_s(file, suggestedName.c_str(), _TRUNCATE);
    std::wstring filter = std::wstring(filterName) + L'\0' + pattern + L'\0' + L'\0';
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->hwnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = defExt;
    ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn))) return false;
    out = file;
    return true;
}

std::wstring DirOf(const std::wstring& path) {
    size_t s = path.find_last_of(L"\\/");
    return s == std::wstring::npos ? std::wstring() : path.substr(0, s);
}

void RememberLookDir(const std::wstring& path) {
    AppSettings set = LoadSettings();
    set.lastLookDir = DirOf(path);
    SaveSettings(set);
}

void SaveLook(State* st) {
    std::wstring path;
    if (!PickFile(st, true, T(Str::LookFilter), L"*.zklook", L"zklook", LoadSettings().lastLookDir, L"", path)) return;
    RememberLookDir(path);
    if (!SaveLookFile(path, st->p)) ShowInfo(st->hwnd, T(Str::LookSaveFailed));
}

void LoadLook(State* st) {
    std::wstring path;
    if (!PickFile(st, false, T(Str::LookFilter), L"*.zklook", L"zklook", LoadSettings().lastLookDir, L"", path)) return;
    RememberLookDir(path);
    FilterParams np = st->p;
    if (!LoadLookFile(path, np)) { ShowInfo(st->hwnd, T(Str::LookLoadFailed)); return; }
    // Zastosowanie: nowe parametry, potem LUT z presetu (jesli plik nadal istnieje, inaczej blad pod nazwa pliku).
    st->p = np;
    const std::wstring lutPath = np.lutPath;
    st->pendingLut.clear();
    st->lut = CubeLut();
    st->lutError.clear();
    st->p.lutPath[0] = 0;
    if (!lutPath.empty()) {
        if (!LoadLut(st, lutPath)) {
            st->p.lutPath[0] = 0;  // LoadLut zostawia blad do pokazania; sciezka nie jest przyjeta
        }
    }
    SetCurveChannel(st, st->curveModel.channel);
    RebuildUi(st);
}

void ExportCube(State* st) {
    std::wstring path;
    if (!PickFile(st, true, T(Str::CubeFilter), L"*.cube", L"cube", LoadSettings().lastLookDir, L"", path)) return;
    RememberLookDir(path);
    std::wstring name = FileName(path);
    if (name.size() > 5) name.resize(name.size() - 5);
    const std::string title = WideToUtf8(name);
    const std::string text = ExportPipelineToCube(st->p, st->lut.valid() ? &st->lut : nullptr, 33, title);
    if (!WriteFileAtomic(path, text)) { ShowInfo(st->hwnd, T(Str::ExportFailed)); return; }
    ShowInfo(st->hwnd, T(Str::ExportDone));
}

void CopyText(HWND owner, const std::wstring& t) {
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
    if (g) {
        memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
    }
    CloseClipboard();
}

void OnCurveField(State* st, int id) {
    if (st->updating) return;
    Curve& c = st->p.curves.c[st->curveModel.channel];
    const int sel = st->curveModel.selected;
    if (sel < 0 || sel >= c.n) return;
    wchar_t b[16];
    GetDlgItemTextW(st->hwnd, id, b, 16);
    if (!b[0]) return;
    int v = std::min(255, std::max(0, _wtoi(b)));
    float x = c.p[sel].x, y = c.p[sel].y;
    if (id == IDC_CURVE_IN) x = v / 255.0f;
    else y = v / 255.0f;
    CurveMovePoint(c, sel, x, y);
    if (st->curveEditor) CurveEditorRefresh(st->curveEditor);
    RequestRender(st);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    State* st = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_NCCREATE: {
            st = (State*)((CREATESTRUCTW*)lp)->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)st);
            st->hwnd = h;
            break;
        }
        case WM_TIMER:
            if (!st) break;
            if (wp == kRenderTimer) {
                KillTimer(h, kRenderTimer);
                RenderPreview(st);
            } else if (wp == kThumbTimer) {
                ThumbTick(st);
            } else if (wp == kLutTimer) {
                FlushPendingLut(st);
            }
            return 0;
        case WM_CTLCOLORSTATIC:
            if ((HWND)lp == GetDlgItem(h, IDC_DIAG)) break;
            SetBkMode((HDC)wp, TRANSPARENT);
            if (st && (HWND)lp == GetDlgItem(h, IDC_LUT_INFO))
                SetTextColor((HDC)wp, st->lutError.empty() ? GetSysColor(COLOR_GRAYTEXT) : RGB(200, 0, 0));
            if (st) {
                // Probki koloru przy suwakach odcienia.
                const int sw = GetDlgCtrlID((HWND)lp);
                const int which = sw == IDC_BW_HUE_SW   ? 0
                                  : sw == IDC_TN_HI_SW  ? 1
                                  : sw == IDC_TN_SH_SW  ? 2
                                  : sw == IDC_DUO_SH_SW ? 3
                                  : sw == IDC_DUO_HI_SW ? 4
                                                        : -1;
                if (which >= 0) {
                    if (st->swatch[which]) DeleteObject(st->swatch[which]);
                    st->swatch[which] = CreateSolidBrush(SwatchColor(st, which));
                    return (LRESULT)st->swatch[which];
                }
            }
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        case WM_HSCROLL:
            if (!st) return 0;
            if ((HWND)lp == GetDlgItem(h, IDC_STRENGTH)) {
                st->p.strength = (int)SendMessageW((HWND)lp, TBM_GETPOS, 0, 0);
            } else if ((HWND)lp == GetDlgItem(h, IDC_LUT_STRENGTH)) {
                st->p.lutStrength = (int)SendMessageW((HWND)lp, TBM_GETPOS, 0, 0);
            } else {
                const int id = GetDlgCtrlID((HWND)lp);
                auto it = std::find_if(st->sliders.begin(), st->sliders.end(), [id](const SliderBind& b) { return b.id == id; });
                if (it == st->sliders.end()) return 0;
                *it->field = (int32_t)SendMessageW((HWND)lp, TBM_GETPOS, 0, 0);
                UpdateBoundLabel(st, *it);
                if (id == IDC_BW_HUE) InvalidateRect(GetDlgItem(h, IDC_BW_HUE_SW), nullptr, TRUE);
                if (id == IDC_TN_HI_HUE) InvalidateRect(GetDlgItem(h, IDC_TN_HI_SW), nullptr, TRUE);
                if (id == IDC_TN_SH_HUE) InvalidateRect(GetDlgItem(h, IDC_TN_SH_SW), nullptr, TRUE);
                if (id == IDC_DUO_MID || (id >= IDC_BW_MIX && id < IDC_BW_MIX + kBwColorCount))
                    InvalidateRect(GetDlgItem(h, IDC_DUO_GRAD), nullptr, TRUE);
            }
            UpdateLabels(st);
            RequestRender(st);
            return 0;
        case WM_NOTIFY: {
            if (!st) break;
            const NMHDR* nm = (const NMHDR*)lp;
            if (nm->idFrom == IDC_TABS && nm->code == TCN_SELCHANGE) {
                ShowPage(st, TabCtrl_GetCurSel(st->tabs));
                return 0;
            }
            if (nm->idFrom == IDC_LIB_LIST) {
                if (nm->code == LVN_ITEMCHANGED && !st->updating) {
                    const NMLISTVIEW* lv = (const NMLISTVIEW*)lp;
                    if ((lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED) &&
                        ListView_GetSelectedCount(st->list) == 1 && lv->iItem >= 0 &&
                        lv->iItem < (int)st->shown.size())
                    {
                        // Wczytanie z krotka zwloka: przegladanie lista strzalkami nie parsuje kazdego pliku.
                        st->pendingLut = st->entries[st->shown[lv->iItem]].path;
                        SetTimer(h, kLutTimer, 150, nullptr);
                    }
                } else if (nm->code == LVN_KEYDOWN && ((const NMLVKEYDOWN*)lp)->wVKey == VK_DELETE) {
                    DeleteLibraryFiles(st);
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND: {
            if (!st) break;
            int id = LOWORD(wp), code = HIWORD(wp);
            if (id == IDC_CURVE_EDITOR) {
                if (code == kCurveChanged) CurvesChanged(st);
                else if (code == kCurveSelected) UpdateCurveFields(st);
                return 0;
            }
            if (code == EN_CHANGE) {
                if (id == IDC_CURVE_IN || id == IDC_CURVE_OUT) OnCurveField(st, id);
                if (id == IDC_DUO_SH_HEX || id == IDC_DUO_HI_HEX) {
                    OnDuotoneHex(st, id);
                    RequestRender(st);
                }
                if (id == IDC_LIB_SEARCH && !st->updating) {
                    wchar_t b[256];
                    GetDlgItemTextW(h, IDC_LIB_SEARCH, b, 256);
                    st->search = b;
                    FillLibraryList(st);
                }
                return 0;
            }
            if (code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW((HWND)lp, CB_GETCURSEL, 0, 0);
                switch (id) {
                    case IDC_EFFECT: st->p.effect = sel; break;
                    case IDC_MASKMODE: st->p.maskMode = sel; break;
                    case IDC_TILEMODE: st->p.tileMode = sel; break;
                    case IDC_LUT_INTERP: st->p.lutTrilinear = sel; break;
                    case IDC_PVMODE: st->pvMode = sel; break;
                    case IDC_CURVE_CH: SetCurveChannel(st, sel); return 0;
                    case IDC_CURVE_PRESET:
                        if (sel > 0) {
                            const int luma = st->p.curves.lumaOnly;
                            ApplyCurvePreset((CurvePreset)(sel - 1), st->p.curves);
                            st->p.curves.lumaOnly = luma;
                            st->p.curvesOn = 1;
                            CheckDlgButton(h, IDC_CURVES_ON, BST_CHECKED);
                            st->curveModel.selected = -1;
                            CurvesChanged(st);
                        }
                        SendMessageW((HWND)lp, CB_SETCURSEL, 0, 0);
                        return 0;
                    case IDC_BW_MODE:
                        st->p.bw.mode = sel;
                        // Monochromia startuje od chlodnego odcienia, zeby nie wygladala jak sepia.
                        if (sel == (int)BwMode::Monochrome && st->p.bw.tintHue == kSepiaHue) {
                            st->p.bw.tintHue = 210;
                            SyncSliders(st);
                        }
                        UpdateBwEnable(st);
                        break;
                    case IDC_BW_FILTER:
                        if (sel > 0) {
                            ApplyBwFilter((BwFilter)(sel - 1), st->p.bw);
                            if (st->p.bw.mode == 0) {
                                st->p.bw.mode = (int)BwMode::BlackWhite;
                                SendDlgItemMessageW(h, IDC_BW_MODE, CB_SETCURSEL, st->p.bw.mode, 0);
                                UpdateBwEnable(st);
                            }
                            SyncSliders(st);
                        }
                        SendMessageW((HWND)lp, CB_SETCURSEL, 0, 0);
                        break;
                    case IDC_DUO_PRESET:
                        if (sel > 0) {
                            ApplyDuotonePreset((DuotonePreset)(sel - 1), st->p.duotone);
                            SyncDuotoneFields(st);
                        }
                        SendMessageW((HWND)lp, CB_SETCURSEL, 0, 0);
                        break;
                    case IDC_TN_PRESET:
                        if (sel > 0) {
                            ApplyToningPreset((ToningPreset)(sel - 1), st->p.toning);
                            CheckDlgButton(h, IDC_TN_ON, BST_CHECKED);
                            UpdateToningEnable(st);
                            SyncSliders(st);
                        }
                        SendMessageW((HWND)lp, CB_SETCURSEL, 0, 0);
                        break;
                    case IDC_LIB_CAT:
                        st->category = sel;
                        FillLibraryList(st);
                        return 0;
                    case IDC_LANG:
                        if (sel != st->language) Relabel(st, sel);
                        return 0;
                    default: return 0;
                }
                RequestRender(st);
                return 0;
            }
            switch (id) {
                case IDC_MARK: st->p.markTiles = IsDlgButtonChecked(h, IDC_MARK) == BST_CHECKED; return 0;
                case IDC_DITHER: st->p.dither = IsDlgButtonChecked(h, IDC_DITHER) == BST_CHECKED; return 0;
                case IDC_SHOWSEL:
                    st->showSel = IsDlgButtonChecked(h, IDC_SHOWSEL) == BST_CHECKED;
                    RequestRender(st);
                    return 0;
                case IDC_CURVES_ON:
                    st->p.curvesOn = IsDlgButtonChecked(h, IDC_CURVES_ON) == BST_CHECKED;
                    RequestRender(st);
                    return 0;
                case IDC_DUO_ON:
                    st->p.duotone.on = IsDlgButtonChecked(h, IDC_DUO_ON) == BST_CHECKED;
                    RequestRender(st);
                    return 0;
                case IDC_DUO_SH_SW:
                case IDC_DUO_SH_PICK:
                case IDC_DUO_HI_SW:
                case IDC_DUO_HI_PICK: {
                    if (code != BN_CLICKED && code != STN_CLICKED) return 0;
                    const bool shadow = id == IDC_DUO_SH_SW || id == IDC_DUO_SH_PICK;
                    uint32_t& c = shadow ? st->p.duotone.shadow : st->p.duotone.highlight;
                    if (PickColor(st, c)) {
                        st->p.duotone.on = 1;  // wybor koloru = chec uzycia efektu
                        SyncDuotoneFields(st);
                        RequestRender(st);
                    }
                    return 0;
                }
                case IDC_DUO_SWAP:
                    std::swap(st->p.duotone.shadow, st->p.duotone.highlight);
                    SyncDuotoneFields(st);
                    RequestRender(st);
                    return 0;
                case IDC_DUO_RESET: {
                    const int32_t on = st->p.duotone.on;
                    st->p.duotone = DuotoneParams();
                    st->p.duotone.on = on;
                    SyncDuotoneFields(st);
                    RequestRender(st);
                    return 0;
                }
                case IDC_TN_ON:
                    st->p.toning.on = IsDlgButtonChecked(h, IDC_TN_ON) == BST_CHECKED;
                    UpdateToningEnable(st);
                    RequestRender(st);
                    return 0;
                case IDC_BW_RESET: {
                    const int32_t mode = st->p.bw.mode;
                    st->p.bw = BwParams();
                    st->p.bw.mode = mode;
                    SyncSliders(st);
                    RequestRender(st);
                    return 0;
                }
                case IDC_TN_RESET: {
                    const int32_t on = st->p.toning.on;
                    st->p.toning = ToningParams();
                    st->p.toning.on = on;
                    SyncSliders(st);
                    RequestRender(st);
                    return 0;
                }
                case IDC_CURVE_LUMA:
                    st->p.curves.lumaOnly = IsDlgButtonChecked(h, IDC_CURVE_LUMA) == BST_CHECKED;
                    RequestRender(st);
                    return 0;
                case IDC_CURVE_LOG:
                    st->curveModel.logHistogram = IsDlgButtonChecked(h, IDC_CURVE_LOG) == BST_CHECKED;
                    if (st->curveEditor) CurveEditorRefresh(st->curveEditor);
                    return 0;
                case IDC_CURVE_RESET_CH:
                    ResetCurve(st->p.curves.c[st->curveModel.channel]);
                    st->curveModel.selected = -1;
                    CurvesChanged(st);
                    return 0;
                case IDC_CURVE_RESET_ALL:
                    for (auto& c : st->p.curves.c) ResetCurve(c);
                    st->curveModel.selected = -1;
                    CurvesChanged(st);
                    return 0;
                case IDC_LUT_CHOOSE: ChooseLut(st); return 0;
                case IDC_LUT_CLEAR:
                    ClearLut(st);
                    if (st->list) {
                        st->updating = true;
                        ListView_SetItemState(st->list, -1, 0, LVIS_SELECTED);
                        st->updating = false;
                    }
                    return 0;
                case IDC_LOOK_SAVE: SaveLook(st); return 0;
                case IDC_LOOK_LOAD: LoadLook(st); return 0;
                case IDC_EXPORT_CUBE: ExportCube(st); return 0;
                case IDC_LIB_ADD: AddLibraryFiles(st); return 0;
                case IDC_LIB_DELETE: DeleteLibraryFiles(st); return 0;
                case IDC_LIB_REFRESH: RefreshLibrary(st); return 0;
                case IDC_LIB_OPEN:
                    if (!st->libDir.empty()) ShellExecuteW(h, L"open", st->libDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                case IDC_LIB_CHANGE: ChangeLibraryFolder(st); return 0;
                case IDC_COPY: CopyText(h, st->ctx->diagnostics); return 0;
                case IDOK:
                case IDC_OKB:
                    FlushPendingLut(st);  // LUT zaznaczony chwile wczesniej ma trafic do parametrow
                    st->ok = true;
                    st->done = true;
                    return 0;
                case IDCANCEL:
                case IDC_CANCELB: st->ok = false; st->done = true; return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (st) { st->ok = false; st->done = true; }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void RegisterClasses(HINSTANCE inst) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = kClass;
    RegisterClassW(&wc);
    WNDCLASSW pc = {};
    pc.lpfnWndProc = PreviewProc;
    pc.hInstance = inst;
    pc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    pc.lpszClassName = kPreviewClass;
    RegisterClassW(&pc);
    RegisterCurveEditor(inst);
    WNDCLASSW gc = {};
    gc.lpfnWndProc = GradProc;
    gc.hInstance = inst;
    gc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    gc.lpszClassName = kGradClass;
    RegisterClassW(&gc);
}

void UnregisterClasses(HINSTANCE inst) {
    UnregisterClassW(kClass, inst);
    UnregisterClassW(kPreviewClass, inst);
    UnregisterClassW(kGradClass, inst);
    UnregisterCurveEditor(inst);
}

}  // namespace

std::wstring CubeErrorText(const CubeResult& r) {
    Str s;
    switch (r.error) {
        case CubeError::Ok: return std::wstring();
        case CubeError::OpenFailed: s = Str::ErrLutOpen; break;
        case CubeError::TooLarge: s = Str::ErrLutTooLarge; break;
        case CubeError::Binary: s = Str::ErrLutBinary; break;
        case CubeError::NoSize: s = Str::ErrLutNoSize; break;
        case CubeError::BadSize: s = Str::ErrLutBadSize; break;
        case CubeError::BadKeyword: s = Str::ErrLutKeyword; break;
        case CubeError::BadNumber: s = Str::ErrLutNumber; break;
        case CubeError::WrongRowCount: s = Str::ErrLutRows; break;
        default: s = Str::ErrLutDomain; break;
    }
    std::wstring t = T(s);
    if (r.line > 0) {
        if (!t.empty() && t.back() == L'.') t.pop_back();
        t += Fmt(T(Str::ErrLutLine), r.line);
        t += L'.';
    }
    return t;
}

bool ShowMainDialog(DialogContext& ctx, FilterParams& p) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES |
                                                 ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    RegisterClasses(ctx.instance);

    State st;
    st.ctx = &ctx;
    st.p = p;
    st.pvMode = std::min(std::max(ctx.previewMode, 0), 2);
    st.page = std::min(std::max(ctx.startPage, 0), (int)kPageCount - 1);
    st.language = ctx.forceLanguage >= 0 ? ctx.forceLanguage : LoadSettings().language;
    SetLanguage(ResolveLanguage(st.language));
    st.curveModel.set = &st.p.curves;
    ComputeHistograms(&st);
    st.thumbBase = MakeThumbBase(ctx.preview);
    ScanLibraryNow(&st);
    // DPI monitora okna nadrzednego (a nie tylko systemowy), a gdy okno nie miesci sie w obszarze roboczym
    // (np. 1366x768 przy 100%, 1920x1080 przy 150%), skala jest zmniejszana, zeby OK / Anuluj byly widoczne.
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    UINT realDpi = ctx.parent ? GetDpiForWindow(ctx.parent) : GetDpiForSystem();
    if (realDpi == 0) realDpi = 96;
    RECT wa = {0, 0, 1024, 768};
    {
        HMONITOR mon = ctx.parent ? MonitorFromWindow(ctx.parent, MONITOR_DEFAULTTONEAREST)
                                  : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfoW(mon, &mi)) wa = mi.rcWork;
        else SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    }
    const int availW = wa.right - wa.left, availH = wa.bottom - wa.top;
    st.dpi = (int)realDpi;
    for (int i = 0; i < 4; ++i) {
        RECT r = {0, 0, S(&st, 1090), S(&st, 720)};
        AdjustWindowRectExForDpi(&r, style, FALSE, WS_EX_DLGMODALFRAME, realDpi);
        const int w = r.right - r.left, hgt = r.bottom - r.top;
        if (w <= availW && hgt <= availH) break;
        const double f = std::min((double)availW / w, (double)availH / hgt) * 0.98;
        st.dpi = std::max(48, (int)(st.dpi * f));
    }
    st.curveModel.dpi = st.dpi;
    st.font = CreateFontW(-MulDiv(9, st.dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    st.smallFont = CreateFontW(-MulDiv(8, st.dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // LUT z poprzedniego uzycia (sciezka w parametrach filtra).
    std::wstring startLut = st.p.lutPath;
    st.p.lutPath[0] = 0;

    RECT rc = {0, 0, S(&st, 1090), S(&st, 720)};
    AdjustWindowRectExForDpi(&rc, style, FALSE, WS_EX_DLGMODALFRAME, realDpi);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int x = wa.left + std::max(0, (int)(wa.right - wa.left - ww) / 2);
    int y = wa.top + std::max(0, (int)(wa.bottom - wa.top - wh) / 2);
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, T(Str::Title), style, x, y, ww, wh,
                             ctx.parent, nullptr, ctx.instance, &st);
    if (!h) { DeleteObject(st.font); DeleteObject(st.smallFont); return false; }
    if (!startLut.empty()) LoadLut(&st, startLut);  // przed Build: lista biblioteki zaznaczy biezacy LUT
    Build(&st);
    RenderPreview(&st);
    if (ctx.parent) EnableWindow(ctx.parent, FALSE);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetFocus(GetDlgItem(h, IDC_OKB));
    if (ctx.onShown) ctx.onShown(h, ctx.onShownUser);

    MSG msg;
    for (;;) {
        if (st.done) break;
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) { PostQuitMessage((int)msg.wParam); break; }  // zamykanie programu: oddaj WM_QUIT hostowi
        if (got < 0) break;
        // Enter w polu edycji (liczba krzywej, HEX, szukanie) zatwierdza tylko pole - zmiany sa juz zastosowane
        // na biezaco - zamiast wywolywac domyslny przycisk OK. Esc obsluguje IsDialogMessage (anuluj).
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            wchar_t cls[16] = L"";
            GetClassNameW(msg.hwnd, cls, 16);
            if (_wcsicmp(cls, L"Edit") == 0) continue;
        }
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (ctx.parent) EnableWindow(ctx.parent, TRUE);
    DestroyWindow(h);
    UnregisterClasses(ctx.instance);  // nie zostawiamy klas wskazujacych na kod DLL, ktory host moze zwolnic
    if (st.images) ImageList_Destroy(st.images);
    if (ctx.parent) SetForegroundWindow(ctx.parent);
    DeleteObject(st.font);
    DeleteObject(st.smallFont);
    for (HBRUSH b : st.swatch)
        if (b) DeleteObject(b);
    if (st.ok) {
        for (auto& c : st.p.curves.c) SanitizeCurve(c);
        p = st.p;
    }
    return st.ok;
}

void ShowInfo(HWND parent, const wchar_t* text) {
    MessageBoxW(parent, text, L"Zetmar Kolor", MB_OK | MB_ICONINFORMATION);
}

}  // namespace ui
}  // namespace zk
