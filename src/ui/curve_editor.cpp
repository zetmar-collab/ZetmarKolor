#include "ui/curve_editor.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>

namespace zk {
namespace ui {

namespace {

const wchar_t kCurveClass[] = L"ZetmarKolorCurveEditor";

struct EditorState {
    CurveEditorModel* model = nullptr;
    bool dragging = false;
};

COLORREF ChannelColor(int ch) {
    switch (ch) {
        case kCurveR: return RGB(220, 30, 30);
        case kCurveG: return RGB(20, 160, 40);
        case kCurveB: return RGB(30, 80, 230);
        default: return RGB(20, 20, 20);
    }
}

// DPI zadane przez okno wtyczki (jedno zrodlo skali dla calego okna); inaczej DPI okna.
int Dpi(HWND h) {
    const EditorState* s = (const EditorState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (s && s->model && s->model->dpi > 0) return s->model->dpi;
    return (int)GetDpiForWindow(h);
}

int Margin(HWND h) { return MulDiv(6, Dpi(h), 96); }

RECT PlotRect(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    int m = Margin(h);
    return {m, m, rc.right - m, rc.bottom - m};
}

POINT ToScreen(const RECT& r, float x, float y) {
    return {r.left + (LONG)std::lround(x * (r.right - r.left - 1)),
            r.bottom - 1 - (LONG)std::lround(y * (r.bottom - r.top - 1))};
}

void ToCurve(const RECT& r, int px, int py, float& x, float& y) {
    x = (px - r.left) / (float)std::max<LONG>(1, r.right - r.left - 1);
    y = (r.bottom - 1 - py) / (float)std::max<LONG>(1, r.bottom - r.top - 1);
    x = std::min(std::max(x, 0.0f), 1.0f);
    y = std::min(std::max(y, 0.0f), 1.0f);
}

Curve& Cur(EditorState* s) { return s->model->set->c[s->model->channel]; }

void Notify(HWND h, WORD code) {
    SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), code), (LPARAM)h);
}

int HitTest(HWND h, const Curve& c, int px, int py) {
    const RECT r = PlotRect(h);
    const int rad = MulDiv(7, Dpi(h), 96);
    int best = -1, bestD = rad * rad + 1;
    for (int i = 0; i < c.n; ++i) {
        POINT p = ToScreen(r, c.p[i].x, c.p[i].y);
        int d = (p.x - px) * (p.x - px) + (p.y - py) * (p.y - py);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int AddPoint(Curve& c, float x, float y) {
    if (c.n >= kMaxCurvePoints) return -1;
    int k = 1;
    while (k < c.n && c.p[k].x < x) ++k;
    if (k == 0 || k >= c.n) return -1;
    if (x - c.p[k - 1].x < kMinPointGap || c.p[k].x - x < kMinPointGap) return -1;
    for (int i = c.n; i > k; --i) c.p[i] = c.p[i - 1];
    c.p[k] = {x, y};
    ++c.n;
    return k;
}

void DeletePoint(Curve& c, int i) {
    if (i <= 0 || i >= c.n - 1) return;  // konce zostaja
    for (int k = i; k < c.n - 1; ++k) c.p[k] = c.p[k + 1];
    --c.n;
}

void Paint(HWND h, EditorState* s) {
    PAINTSTRUCT ps;
    HDC wdc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(250, 250, 250));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    const RECT r = PlotRect(h);
    const bool focused = GetFocus() == h;
    HPEN frame = CreatePen(PS_SOLID, 1, focused ? RGB(0, 120, 215) : RGB(150, 150, 150));
    HGDIOBJ oldPen = SelectObject(dc, frame);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, r.left - 1, r.top - 1, r.right + 1, r.bottom + 1);

    if (s && s->model && s->model->set) {
        CurveEditorModel* m = s->model;
        // Histogram w tle.
        if (m->histogram) {
            double mx = 0;
            for (int i = 0; i < 256; ++i) {
                double v = m->logHistogram ? std::log1p((double)m->histogram[i]) : (double)m->histogram[i];
                mx = std::max(mx, v);
            }
            if (mx > 0) {
                HPEN hp = CreatePen(PS_SOLID, 1, RGB(215, 215, 215));
                SelectObject(dc, hp);
                const int w = r.right - r.left, hh = r.bottom - r.top;
                for (int px = 0; px < w; ++px) {
                    int bin = std::min(255, px * 256 / std::max(1, w));
                    double v = m->logHistogram ? std::log1p((double)m->histogram[bin]) : (double)m->histogram[bin];
                    int top = r.bottom - 1 - (int)(v / mx * (hh - 1));
                    MoveToEx(dc, r.left + px, r.bottom - 1, nullptr);
                    LineTo(dc, r.left + px, top);
                }
                SelectObject(dc, frame);
                DeleteObject(hp);
            }
        }
        // Siatka 4x4 i przekatna.
        HPEN grid = CreatePen(PS_DOT, 1, RGB(185, 185, 185));
        SelectObject(dc, grid);
        SetBkMode(dc, TRANSPARENT);
        for (int i = 1; i < 4; ++i) {
            int gx = r.left + (r.right - r.left) * i / 4, gy = r.top + (r.bottom - r.top) * i / 4;
            MoveToEx(dc, gx, r.top, nullptr); LineTo(dc, gx, r.bottom);
            MoveToEx(dc, r.left, gy, nullptr); LineTo(dc, r.right, gy);
        }
        POINT a = ToScreen(r, 0, 0), b = ToScreen(r, 1, 1);
        MoveToEx(dc, a.x, a.y, nullptr);
        LineTo(dc, b.x, b.y);
        SelectObject(dc, frame);
        DeleteObject(grid);

        auto drawCurve = [&](const Curve& c, COLORREF col, int width) {
            HPEN pen = CreatePen(PS_SOLID, width, col);
            SelectObject(dc, pen);
            const int steps = std::max<LONG>(64, r.right - r.left);
            for (int i = 0; i <= steps; ++i) {
                float x = i / (float)steps;
                POINT p = ToScreen(r, x, EvalCurve(c, x));
                if (i == 0) MoveToEx(dc, p.x, p.y, nullptr);
                else LineTo(dc, p.x, p.y);
            }
            SelectObject(dc, frame);
            DeleteObject(pen);
        };
        // Pozostale kanaly (jesli zmienione) - cienko i jasniej.
        for (int ch = 0; ch < kCurveCount; ++ch) {
            if (ch == m->channel || IsIdentity(m->set->c[ch])) continue;
            COLORREF c = ChannelColor(ch);
            drawCurve(m->set->c[ch], RGB((GetRValue(c) + 2 * 255) / 3, (GetGValue(c) + 2 * 255) / 3,
                                         (GetBValue(c) + 2 * 255) / 3), 1);
        }
        const Curve& c = m->set->c[m->channel];
        drawCurve(c, ChannelColor(m->channel), MulDiv(2, Dpi(h), 96));
        // Punkty.
        const int ps2 = MulDiv(4, Dpi(h), 96);
        HPEN pp = CreatePen(PS_SOLID, 1, RGB(20, 20, 20));
        SelectObject(dc, pp);
        for (int i = 0; i < c.n; ++i) {
            POINT p = ToScreen(r, c.p[i].x, c.p[i].y);
            SelectObject(dc, i == m->selected ? GetStockObject(BLACK_BRUSH) : GetStockObject(WHITE_BRUSH));
            Rectangle(dc, p.x - ps2, p.y - ps2, p.x + ps2 + 1, p.y + ps2 + 1);
        }
        SelectObject(dc, frame);
        DeleteObject(pp);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(frame);
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(h, &ps);
}

LRESULT CALLBACK CurveProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    EditorState* s = (EditorState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_NCCREATE: {
            auto* st = new EditorState();
            st->model = (CurveEditorModel*)((CREATESTRUCTW*)lp)->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)st);
            break;
        }
        case WM_NCDESTROY:
            delete s;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: Paint(h, s); return 0;
        case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_SETFOCUS:
        case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_LBUTTONDOWN: {
            if (!s || !s->model || !s->model->set) return 0;
            SetFocus(h);
            Curve& c = Cur(s);
            const int px = GET_X_LPARAM(lp), py = GET_Y_LPARAM(lp);
            int hit = HitTest(h, c, px, py);
            if (hit < 0) {
                float x, y;
                ToCurve(PlotRect(h), px, py, x, y);
                hit = AddPoint(c, x, y);
                if (hit >= 0) Notify(h, kCurveChanged);
            }
            s->model->selected = hit;
            Notify(h, kCurveSelected);
            if (hit >= 0) {
                s->dragging = true;
                SetCapture(h);
            }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (s && s->dragging && s->model->selected >= 0) {
                float x, y;
                ToCurve(PlotRect(h), GET_X_LPARAM(lp), GET_Y_LPARAM(lp), x, y);
                CurveMovePoint(Cur(s), s->model->selected, x, y);
                InvalidateRect(h, nullptr, FALSE);
                Notify(h, kCurveChanged);
            }
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == h) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            if (s && s->dragging) {
                s->dragging = false;
                Notify(h, kCurveDragEnd);
            }
            return 0;
        case WM_RBUTTONDOWN: {
            if (!s || !s->model || !s->model->set) return 0;
            SetFocus(h);
            Curve& c = Cur(s);
            int hit = HitTest(h, c, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (hit > 0 && hit < c.n - 1) {
                DeletePoint(c, hit);
                s->model->selected = -1;
                Notify(h, kCurveSelected);
                Notify(h, kCurveChanged);
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (!s || !s->model || !s->model->set) return 0;
            Curve& c = Cur(s);
            int& sel = s->model->selected;
            if (sel < 0 || sel >= c.n) {
                if (wp == VK_TAB) break;
                return 0;
            }
            const float step = 1.0f / 255.0f;
            float x = c.p[sel].x, y = c.p[sel].y;
            switch (wp) {
                case VK_LEFT: x -= step; break;
                case VK_RIGHT: x += step; break;
                case VK_UP: y += step; break;
                case VK_DOWN: y -= step; break;
                case VK_DELETE:
                case VK_BACK:
                    if (sel > 0 && sel < c.n - 1) {
                        DeletePoint(c, sel);
                        sel = -1;
                        Notify(h, kCurveSelected);
                        Notify(h, kCurveChanged);
                        InvalidateRect(h, nullptr, FALSE);
                    }
                    return 0;
                default: return 0;
            }
            CurveMovePoint(c, sel, x, y);
            Notify(h, kCurveSelected);
            Notify(h, kCurveChanged);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

void CurveMovePoint(Curve& c, int i, float x, float y) {
    if (i < 0 || i >= c.n) return;
    if (i == 0) x = 0.0f;
    else if (i == c.n - 1) x = 1.0f;
    else x = std::min(std::max(x, c.p[i - 1].x + kMinPointGap), c.p[i + 1].x - kMinPointGap);
    c.p[i].x = x;
    c.p[i].y = std::min(std::max(y, 0.0f), 1.0f);
}

void UnregisterCurveEditor(HINSTANCE inst) { UnregisterClassW(kCurveClass, inst); }

void RegisterCurveEditor(HINSTANCE inst) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = CurveProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kCurveClass;
    RegisterClassW(&wc);
}

HWND CreateCurveEditor(HWND parent, HINSTANCE inst, int id, int x, int y, int size, CurveEditorModel* model) {
    return CreateWindowExW(0, kCurveClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, y, size, size, parent,
                           (HMENU)(INT_PTR)id, inst, model);
}

void CurveEditorRefresh(HWND editor) { InvalidateRect(editor, nullptr, FALSE); }

}  // namespace ui
}  // namespace zk
