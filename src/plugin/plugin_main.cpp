// Punkt wejscia filtra (.8bf). Odczyt warstwy, okno, zapis kafelkami, raport diagnostyczny.
#include <windows.h>

#include "PIDefines.h"
#include "PIAbout.h"
#include "PIFilter.h"
#include "PITypes.h"

#include <cstdio>
#include <new>

#include "core/cube_lut.h"
#include "core/i18n.h"
#include "core/settings.h"
#include "plugin/host.h"
#include "ui/main_dialog.h"

using namespace zk;

namespace {

HINSTANCE g_instance = nullptr;

struct PluginData {
    bool queryForParameters = true;
};

FilterRecord* g_fr = nullptr;
int16* g_result = nullptr;
PluginData* g_data = nullptr;

void EnsureStorage() {
    if (!g_fr->parameters) {
        g_fr->parameters = g_fr->handleProcs->newProc((int32)sizeof(FilterParams));
        if (!g_fr->parameters) { *g_result = memFullErr; return; }
        FilterParams* p = (FilterParams*)g_fr->handleProcs->lockProc(g_fr->parameters, TRUE);
        if (p) { SetDefaults(*p); g_fr->handleProcs->unlockProc(g_fr->parameters); }
    }
    if (!g_data) {
        g_data = new (std::nothrow) PluginData();
        if (!g_data) *g_result = memFullErr;
    }
}

bool LoadParams(FilterParams& p) {
    if (!g_fr->parameters) return false;
    if (g_fr->handleProcs->getSizeProc(g_fr->parameters) < (int32)sizeof(FilterParams)) { SetDefaults(p); return true; }
    FilterParams* h = (FilterParams*)g_fr->handleProcs->lockProc(g_fr->parameters, TRUE);
    if (!h) return false;
    p = *h;
    g_fr->handleProcs->unlockProc(g_fr->parameters);
    if (!IsValid(p)) SetDefaults(p);
    SanitizeParams(p);
    return true;
}

void StoreParams(const FilterParams& p) {
    if (!g_fr->parameters) return;
    FilterParams* h = (FilterParams*)g_fr->handleProcs->lockProc(g_fr->parameters, TRUE);
    if (!h) return;
    *h = p;
    g_fr->handleProcs->unlockProc(g_fr->parameters);
}

void DoAbout(AboutRecord* ar) {
    HWND hwnd = ar && ar->platformData ? (HWND)((PlatformData*)ar->platformData)->hwnd : nullptr;
    SetLanguage(ResolveLanguage(LoadSettings().language));
    ui::ShowInfo(hwnd, T(Str::AboutText));
}

void DoStart() {
    EnsureStorage();
    if (*g_result != noErr) return;
    FilterParams p;
    if (!LoadParams(p)) { *g_result = memFullErr; return; }

    SetLanguage(ResolveLanguage(LoadSettings().language));
    HostInfo hi = QueryHost(g_fr);
    // Opis hosta i raport tylko w trybie diagnostycznym (debug.flag); poza nim nic nie jest budowane ani zapisywane.
    const bool debug = DebugEnabled();
    std::wstring diag = debug ? DescribeHost(g_fr, hi) : std::wstring();
    const bool dialog = g_data->queryForParameters;
    diag += dialog ? L"Wywolanie: z oknem\r\n" : L"Wywolanie: bez okna (Ctrl+F / powtorzenie)\r\n";

    if (!hi.rgbOk) {
        diag += L"WYNIK: nieobslugiwany tryb obrazu\r\n";
        AppendReport(diag);
        ui::ShowInfo(hi.hwnd, T(Str::MsgUnsupportedMode));
        *g_result = userCanceledErr;
        return;
    }

    if (dialog) {
        PreviewImage prev;
        std::wstring plog;
        bool havePreview = ReadPreview(g_fr, hi, 800, prev, plog);
        diag += plog;
        ui::DialogContext dc;
        dc.parent = hi.hwnd;
        dc.instance = g_instance;
        dc.preview = havePreview ? &prev : nullptr;
        dc.diagnostics = diag;
        if (!ui::ShowMainDialog(dc, p)) {
            diag += L"WYNIK: anulowano w oknie\r\n";
            AppendReport(diag);
            *g_result = userCanceledErr;
            return;
        }
        StoreParams(p);
    }
    g_data->queryForParameters = false;

    wchar_t buf[512];
    swprintf_s(buf, L"Ustawienia: efekt %d sila %d maskowanie %s podzial %d granice %d dithering %d\r\n", p.effect,
               p.strength, p.maskMode ? L"wlasne" : L"host", p.tileMode, p.markTiles, p.dither);
    diag += buf;

    if (p.curvesOn) {
        CurveTables ct;
        BuildCurveTables(p.curves, ct);
        swprintf_s(buf, L"Krzywe: aktywne %d, punkty RGB/R/G/B %d/%d/%d/%d, tylko jasnosc %d\r\n", (int)ct.active,
                   p.curves.c[0].n, p.curves.c[1].n, p.curves.c[2].n, p.curves.c[3].n, p.curves.lumaOnly);
        diag += buf;
    }

    swprintf_s(buf, L"Cz-B: tryb %d, mikser %d/%d/%d/%d/%d/%d, barwa %d/%d; toning %d (%d/%d, %d/%d, balans %d); sila ogolna %d\r\n",
               p.bw.mode, p.bw.mix[0], p.bw.mix[1], p.bw.mix[2], p.bw.mix[3], p.bw.mix[4], p.bw.mix[5], p.bw.tintHue,
               p.bw.tintSat, p.toning.on, p.toning.hiHue, p.toning.hiSat, p.toning.shHue, p.toning.shSat,
               p.toning.balance, p.overall);
    diag += buf;
    swprintf_s(buf, L"Duotone: %d, cienie %06X, swiatla %06X, srodek %d, sila %d\r\n", p.duotone.on, p.duotone.shadow,
               p.duotone.highlight, p.duotone.midpoint, p.duotone.amount);
    diag += buf;

    // LUT wczytywany od nowa takze przy Ctrl+F (plik mogl zniknac albo sie zmienic).
    CubeLut lut;
    if (p.lutPath[0]) {
        CubeResult lr = LoadCubeFile(p.lutPath, lut);
        if (lr.error != CubeError::Ok) {
            std::wstring msg = std::wstring(T(Str::MsgLutLoadFailed)) + L"\n" + p.lutPath + L"\n\n" + ui::CubeErrorText(lr);
            diag += L"LUT: blad wczytywania " + std::to_wstring((int)lr.error) + L" w " + p.lutPath + L"\r\n";
            AppendReport(diag);
            ui::ShowInfo(hi.hwnd, msg.c_str());
            *g_result = userCanceledErr;
            return;
        }
        swprintf_s(buf, L"LUT: 3D %d, 1D %d, sila %d, %s: ", lut.size3d, lut.size1d, p.lutStrength,
                   p.lutTrilinear ? L"trojliniowa" : L"tetraedralna");
        diag += buf;
        diag += p.lutPath;
        diag += L"\r\n";
    }

    ApplyStats st;
    short hostErr = 0;
    ApplyStatus as = Apply(g_fr, hi, p, lut.valid() ? &lut : nullptr, st, hostErr);
    swprintf_s(buf,
               L"Zapis: status %d hostErr %d, kafelki %d, czas %.0f ms, wlasne maskowanie aktywne %d, autoMask po "
               L"zmianie %d\r\n",
               (int)as, (int)hostErr, st.tiles, st.ms, (int)st.ownMaskActive, st.autoMaskAfter);
    diag += buf;
    if (st.maskDataSeen) {
        swprintf_s(buf, L"maskData: min %d max %d, probek %lld: =0 %lld, =255 %lld, posrednie %lld\r\n", st.maskMin,
                   st.maskMax, st.maskSamples, st.maskZero, st.maskFull, st.maskPartial);
        diag += buf;
    } else {
        diag += L"maskData: nie dostarczono\r\n";
    }
    AppendReport(diag);

    if (as == ApplyStatus::HostError) *g_result = hostErr;
    else if (as == ApplyStatus::Cancelled) *g_result = userCanceledErr;
}

void DoContinue() {
    VRect z;
    z.left = z.top = z.right = z.bottom = 0;
    if (g_fr->bigDocumentData && g_fr->bigDocumentData->PluginUsing32BitCoordinates) {
        g_fr->bigDocumentData->inRect32 = z;
        g_fr->bigDocumentData->outRect32 = z;
        g_fr->bigDocumentData->maskRect32 = z;
    } else {
        Rect r;
        r.left = r.top = r.right = r.bottom = 0;
        g_fr->inRect = r;
        g_fr->outRect = r;
        g_fr->maskRect = r;
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

DLLExport MACPASCAL void PluginMain(const int16 selector, FilterRecordPtr filterRecord, intptr_t* data,
                                    int16* result) {
    try {
        g_fr = filterRecord;
        g_result = result;
        g_data = (PluginData*)*data;
        // Tryb diagnostyczny (debug.flag): ślad wywołań, pomaga ustalić, czy i jak host woła wtyczkę.
        DebugLog(L"PluginMain: selektor %d, host %p", (int)selector, (void*)filterRecord);

        if (selector == filterSelectorAbout) {
            DoAbout((AboutRecord*)filterRecord);
            return;
        }
        if (g_fr->bigDocumentData != nullptr) g_fr->bigDocumentData->PluginUsing32BitCoordinates = true;

        switch (selector) {
            case filterSelectorParameters:
                EnsureStorage();
                if (*g_result == noErr) g_data->queryForParameters = true;
                break;
            case filterSelectorPrepare:
                EnsureStorage();
                if (*g_result == noErr) g_fr->bufferSpace = 0;
                break;
            case filterSelectorStart: DoStart(); break;
            case filterSelectorContinue: DoContinue(); break;
            default: break;
        }
        *data = (intptr_t)g_data;
    } catch (...) {
        // Wyjatek C++ (zwykle brak pamieci): znany kod bledu zamiast -1, zeby host pokazal sensowny komunikat.
        if (result) *result = memFullErr;
    }
}
