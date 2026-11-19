#include "ui/lut_library.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cwctype>

#include "core/settings.h"
#include "plugin/host.h"

namespace zk {
namespace ui {

namespace {

bool DirExists(const std::wstring& d) {
    DWORD a = GetFileAttributesW(d.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileExists(const std::wstring& f) {
    DWORD a = GetFileAttributesW(f.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towlower(c);
    return s;
}

bool HasCubeExt(const std::wstring& name) {
    return name.size() > 5 && Lower(name.substr(name.size() - 5)) == L".cube";
}

void ScanDir(const std::wstring& dir, const std::wstring& category, std::vector<LutEntry>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.cube").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring n = fd.cFileName;
        if (!HasCubeExt(n)) continue;
        out.push_back({dir + L"\\" + n, n.substr(0, n.size() - 5), category});
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

uint32_t Pack(float r, float g, float b) {
    auto q = [](float v) { return (uint32_t)std::lround(std::min(std::max(v, 0.0f), 1.0f) * 255.0f); };
    return 0xFF000000u | (q(r) << 16) | (q(g) << 8) | q(b);
}

}  // namespace

std::wstring LibraryDir() {
    AppSettings s = LoadSettings();
    if (!s.lutLibraryDir.empty() && DirExists(s.lutLibraryDir)) return s.lutLibraryDir;
    wchar_t pd[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"ProgramData", pd, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::wstring d = std::wstring(pd) + L"\\ZetmarKolor\\LUTs";
        if (DirExists(d)) return d;
    }
    std::wstring d = DataDir();
    if (d.empty()) return d;
    d += L"\\LUTs";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

void SetLibraryDir(const std::wstring& dir) {
    AppSettings s = LoadSettings();
    s.lutLibraryDir = dir;
    SaveSettings(s);
}

std::vector<LutEntry> ScanLibrary(const std::wstring& dir) {
    std::vector<LutEntry> out;
    if (dir.empty() || !DirExists(dir)) return out;
    ScanDir(dir, L"", out);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            ScanDir(dir + L"\\" + n, n, out);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(out.begin(), out.end(), [](const LutEntry& a, const LutEntry& b) {
        if (a.category != b.category) return Lower(a.category) < Lower(b.category);
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS, a.name.c_str(),
                               -1, b.name.c_str(), -1, nullptr, nullptr, 0) == CSTR_LESS_THAN;
    });
    return out;
}

std::vector<std::wstring> LibraryCategories(const std::vector<LutEntry>& entries) {
    std::vector<std::wstring> cats;
    for (const auto& e : entries)
        if (!e.category.empty() && std::find(cats.begin(), cats.end(), e.category) == cats.end())
            cats.push_back(e.category);
    return cats;
}

bool AddToLibrary(const std::wstring& dir, const std::wstring& category, const std::wstring& src, std::wstring& dst,
                  CubeResult& parse, DWORD& winErr) {
    winErr = 0;
    CubeLut lut;
    parse = LoadCubeFile(src, lut);
    if (parse.error != CubeError::Ok) return false;
    std::wstring target = category.empty() ? dir : dir + L"\\" + category;
    CreateDirectoryW(target.c_str(), nullptr);
    size_t s = src.find_last_of(L"\\/");
    std::wstring file = s == std::wstring::npos ? src : src.substr(s + 1);
    std::wstring base = HasCubeExt(file) ? file.substr(0, file.size() - 5) : file;
    dst = target + L"\\" + base + L".cube";
    if (Lower(dst) == Lower(src)) return true;  // plik juz jest w bibliotece
    for (int k = 2; FileExists(dst) && k < 1000; ++k) dst = target + L"\\" + base + L" (" + std::to_wstring(k) + L").cube";
    if (!CopyFileW(src.c_str(), dst.c_str(), TRUE)) {
        winErr = GetLastError();
        return false;
    }
    return true;
}

bool PathHasRecycleBin(const std::wstring& path) {
    if (path.size() < 3 || path[1] != L':') return false;  // UNC i nietypowe sciezki: bez Kosza
    const wchar_t root[4] = {path[0], L':', L'\\', 0};
    return GetDriveTypeW(root) == DRIVE_FIXED;
}

bool MoveToRecycleBin(HWND owner, const std::vector<std::wstring>& paths) {
    if (paths.empty()) return true;
    std::wstring list;
    for (const auto& p : paths) {
        list += p;
        list += L'\0';
    }
    list += L'\0';
    SHFILEOPSTRUCTW op = {};
    op.hwnd = owner;
    op.wFunc = FO_DELETE;
    op.pFrom = list.c_str();
    // Kosz (cofnij mozliwe), bez drugiego pytania - potwierdzenie robi wtyczka.
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}

std::vector<uint32_t> MakeThumbBase(const PreviewImage* pi) {
    std::vector<uint32_t> out((size_t)kThumbW * kThumbH, 0xFF404040u);
    if (!pi || pi->w <= 0 || pi->h <= 0) {
        // Bez podgladu: tecza + szarosci, zeby bylo widac dzialanie LUT-a.
        for (int y = 0; y < kThumbH; ++y)
            for (int x = 0; x < kThumbW; ++x) {
                float h = x / (float)kThumbW * 6.0f, v = 1.0f - y / (float)kThumbH;
                int i = (int)h;
                float f = h - i, r, g, b;
                switch (i % 6) {
                    case 0: r = 1; g = f; b = 0; break;
                    case 1: r = 1 - f; g = 1; b = 0; break;
                    case 2: r = 0; g = 1; b = f; break;
                    case 3: r = 0; g = 1 - f; b = 1; break;
                    case 4: r = f; g = 0; b = 1; break;
                    default: r = 1; g = 0; b = 1 - f; break;
                }
                out[(size_t)y * kThumbW + x] = Pack(r * v, g * v, b * v);
            }
        return out;
    }
    // Wypelnienie kadru (przyciecie srodka), probkowanie najblizszym sasiadem.
    const double sc = std::max((double)kThumbW / pi->w, (double)kThumbH / pi->h);
    const double ox = (pi->w * sc - kThumbW) / 2, oy = (pi->h * sc - kThumbH) / 2;
    for (int y = 0; y < kThumbH; ++y)
        for (int x = 0; x < kThumbW; ++x) {
            int sx = std::min(pi->w - 1, std::max(0, (int)((x + ox) / sc)));
            int sy = std::min(pi->h - 1, std::max(0, (int)((y + oy) / sc)));
            const float* s = &pi->rgba[((size_t)sy * pi->w + sx) * 4];
            float bg = (((x / 6) + (y / 6)) & 1) ? 0.8f : 1.0f, a = s[3];
            out[(size_t)y * kThumbW + x] = Pack(s[0] * a + bg * (1 - a), s[1] * a + bg * (1 - a), s[2] * a + bg * (1 - a));
        }
    return out;
}

std::vector<uint32_t> MakeThumb(const std::vector<uint32_t>& base, const CubeLut* lut) {
    std::vector<uint32_t> out(base);
    if (!lut || !lut->valid()) return out;
    for (auto& px : out) {
        float in[3] = {((px >> 16) & 0xFF) / 255.0f, ((px >> 8) & 0xFF) / 255.0f, (px & 0xFF) / 255.0f}, o[3];
        SampleCube(*lut, in, o);
        px = Pack(o[0], o[1], o[2]);
    }
    return out;
}

std::vector<uint32_t> MakeErrorThumb() {
    std::vector<uint32_t> out((size_t)kThumbW * kThumbH, 0xFFF2D7D7u);
    // Czerwony krzyzyk.
    for (int i = 0; i < kThumbH; ++i)
        for (int d = -1; d <= 1; ++d) {
            int x1 = (kThumbW - kThumbH) / 2 + i + d, x2 = (kThumbW + kThumbH) / 2 - i + d;
            if (x1 >= 0 && x1 < kThumbW) out[(size_t)i * kThumbW + x1] = 0xFFC00000u;
            if (x2 >= 0 && x2 < kThumbW) out[(size_t)i * kThumbW + x2] = 0xFFC00000u;
        }
    return out;
}

}  // namespace ui
}  // namespace zk
