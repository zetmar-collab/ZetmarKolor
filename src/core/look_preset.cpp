#include "core/look_preset.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#include "core/cube_lut.h"
#include "core/settings.h"

namespace zk {

namespace {

constexpr const char* kHeader = "ZKLOOK 1";

std::string ToUtf8(const wchar_t* w) {
    const int len = (int)wcslen(w);
    if (len == 0) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)std::max(n, 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, len, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Liczby zmiennoprzecinkowe zapisywane jako calkowite * 1e6 (niezalezne od ustawien regionalnych).
long long Q(float v) { return std::llround((double)v * 1000000.0); }

bool ReadInts(const std::string& rest, std::vector<long long>& out, size_t minCount, size_t maxCount) {
    out.clear();
    const char* p = rest.c_str();
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        char* end = nullptr;
        long long v = std::strtoll(p, &end, 10);
        if (end == p || (*end && *end != ' ' && *end != '\t')) return false;
        if (out.size() >= maxCount) return false;
        out.push_back(v);
        p = end;
    }
    return out.size() >= minCount;
}

// Sciezka LUT z presetu (plik moze pochodzic z internetu) to lista DOZWOLONYCH form, nie zabronionych: tylko
// bezwzgledna sciezka z litera dysku ("X:\..." albo "X:/..."), bez znakow '?' i ':' poza literą dysku (to wyklucza
// UNC, \\?\, \\.\, \??\ i strumienie NTFS), konczaca sie na ".cube", na dysku, ktory nie jest sieciowy. Otwarcie
// sciezki sieciowej wysylaloby dane logowania NTLM na obcy serwer.
bool IsSafeLocalCubePath(const std::wstring& w) {
    const size_t n = w.size();
    if (n < 8 || n >= 1024) return false;  // "C:\x.cube" ma 9 znakow; krotsze nie moga byc poprawne
    const wchar_t d = w[0];
    if (!((d >= L'A' && d <= L'Z') || (d >= L'a' && d <= L'z')) || w[1] != L':' || (w[2] != L'\\' && w[2] != L'/'))
        return false;
    for (size_t i = 2; i < n; ++i)
        if (w[i] == L'?' || w[i] == L':' || w[i] == L'*' || w[i] == L'"' || w[i] == L'<' || w[i] == L'>' ||
            w[i] == L'|' || w[i] < 32)
            return false;
    if (_wcsicmp(w.c_str() + n - 5, L".cube") != 0) return false;
    // Kanonizacja i typ dysku (dysk sieciowy zmapowany na litere tez odpada).
    wchar_t full[1024];
    const DWORD got = GetFullPathNameW(w.c_str(), 1024, full, nullptr);
    if (got == 0 || got >= 1024) return false;
    if (!(full[0] && full[1] == L':' && full[2] == L'\\')) return false;
    const wchar_t root[4] = {full[0], L':', L'\\', 0};
    return GetDriveTypeW(root) != DRIVE_REMOTE;
}

}  // namespace

std::string LookToText(const FilterParams& p) {
    std::ostringstream o;
    o << kHeader << "\n";
    o << "curvesOn " << (p.curvesOn ? 1 : 0) << "\n";
    o << "lumaOnly " << (p.curves.lumaOnly ? 1 : 0) << "\n";
    for (int ch = 0; ch < kCurveCount; ++ch) {
        const Curve& c = p.curves.c[ch];
        o << "curve " << ch << " " << c.n;
        for (int i = 0; i < c.n && i < kMaxCurvePoints; ++i) o << " " << Q(c.p[i].x) << " " << Q(c.p[i].y);
        o << "\n";
    }
    o << "bw " << p.bw.mode;
    for (int i = 0; i < kBwColorCount; ++i) o << " " << p.bw.mix[i];
    o << " " << p.bw.tintHue << " " << p.bw.tintSat << "\n";
    o << "toning " << p.toning.on << " " << p.toning.hiHue << " " << p.toning.hiSat << " " << p.toning.shHue << " "
      << p.toning.shSat << " " << p.toning.balance << "\n";
    o << "duotone " << p.duotone.on << " " << (p.duotone.shadow & 0xFFFFFF) << " " << (p.duotone.highlight & 0xFFFFFF)
      << " " << p.duotone.midpoint << " " << p.duotone.amount << "\n";
    o << "lut " << p.lutStrength << " " << p.lutTrilinear << "\n";
    o << "lutpath " << ToUtf8(p.lutPath) << "\n";
    o << "overall " << p.overall << "\n";
    return o.str();
}

bool LookFromText(const char* text, size_t len, FilterParams& p) {
    if ((int64_t)len > kMaxLookFileBytes) return false;
    for (size_t i = 0; i < len; ++i)
        if (text[i] == 0) return false;
    FilterParams t = p;
    size_t start = 0;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        start = 3;
    std::string all(text + start, len - start);
    std::istringstream in(all);
    std::string line;
    bool header = false;
    std::vector<long long> v;
    int seen = 0;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        if (!header) {
            if (line != kHeader) return false;
            header = true;
            continue;
        }
        const size_t sp = line.find(' ');
        const std::string key = line.substr(0, sp);
        const std::string rest = sp == std::string::npos ? std::string() : line.substr(sp + 1);
        if (key == "curvesOn") {
            if (!ReadInts(rest, v, 1, 1)) return false;
            t.curvesOn = v[0] != 0;
        } else if (key == "lumaOnly") {
            if (!ReadInts(rest, v, 1, 1)) return false;
            t.curves.lumaOnly = v[0] != 0;
        } else if (key == "curve") {
            if (!ReadInts(rest, v, 2, 2 + 2 * kMaxCurvePoints)) return false;
            if (v[0] < 0 || v[0] >= kCurveCount || v[1] < 2 || v[1] > kMaxCurvePoints) return false;
            if (v.size() != 2 + 2 * (size_t)v[1]) return false;
            Curve& c = t.curves.c[v[0]];
            c.n = (int32_t)v[1];
            for (int i = 0; i < c.n; ++i) {
                c.p[i].x = (float)(v[2 + 2 * i] / 1000000.0);
                c.p[i].y = (float)(v[3 + 2 * i] / 1000000.0);
            }
            ++seen;
        } else if (key == "bw") {
            const size_t want = 1 + kBwColorCount + 2;
            if (!ReadInts(rest, v, want, want)) return false;
            t.bw.mode = (int32_t)v[0];
            for (int i = 0; i < kBwColorCount; ++i) t.bw.mix[i] = (int32_t)v[1 + i];
            t.bw.tintHue = (int32_t)v[1 + kBwColorCount];
            t.bw.tintSat = (int32_t)v[2 + kBwColorCount];
            ++seen;
        } else if (key == "toning") {
            if (!ReadInts(rest, v, 6, 6)) return false;
            t.toning.on = (int32_t)v[0];
            t.toning.hiHue = (int32_t)v[1];
            t.toning.hiSat = (int32_t)v[2];
            t.toning.shHue = (int32_t)v[3];
            t.toning.shSat = (int32_t)v[4];
            t.toning.balance = (int32_t)v[5];
            ++seen;
        } else if (key == "duotone") {
            if (!ReadInts(rest, v, 5, 5)) return false;
            t.duotone.on = (int32_t)v[0];
            t.duotone.shadow = (uint32_t)v[1] & 0xFFFFFF;
            t.duotone.highlight = (uint32_t)v[2] & 0xFFFFFF;
            t.duotone.midpoint = (int32_t)v[3];
            t.duotone.amount = (int32_t)v[4];
            ++seen;
        } else if (key == "lut") {
            if (!ReadInts(rest, v, 2, 2)) return false;
            t.lutStrength = (int32_t)v[0];
            t.lutTrilinear = v[1] != 0;
        } else if (key == "lutpath") {
            std::wstring w = FromUtf8(rest);
            if (w.size() >= 1024 || (w.empty() && !rest.empty())) return false;
            if (!w.empty() && !IsSafeLocalCubePath(w)) return false;
            wcsncpy_s(t.lutPath, w.c_str(), _TRUNCATE);
        } else if (key == "overall") {
            if (!ReadInts(rest, v, 1, 1)) return false;
            t.overall = (int32_t)v[0];
        }
        // Nieznane klucze: pomijane (zgodnosc w przod).
    }
    if (!header || seen == 0) return false;
    SanitizeParams(t);
    p = t;
    return true;
}

bool SaveLookFile(const std::wstring& path, const FilterParams& p) { return WriteFileAtomic(path, LookToText(p)); }

bool LoadLookFile(const std::wstring& path, FilterParams& p) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    std::string buf;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart <= kMaxLookFileBytes;
    if (ok) {
        buf.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = buf.empty() || (ReadFile(h, buf.data(), (DWORD)buf.size(), &got, nullptr) && got == buf.size());
    }
    CloseHandle(h);
    return ok && LookFromText(buf.data(), buf.size(), p);
}

std::string ExportPipelineToCube(const FilterParams& p, const CubeLut* lut, int size, const std::string& title) {
    size = std::min(65, std::max(2, size));
    CurveTables ct;
    PipelineOptions o = MakeOptions(p, ct, lut && lut->valid() ? lut : nullptr);
    std::string s;
    s.reserve((size_t)size * size * size * 28 + 256);
    s += "# Zetmar Kolor: eksport potoku (krzywe, czern-biel, duotone, LUT, toning, sila ogolna)\n";
    std::string t;
    for (char c : title)
        if (c != '"' && (unsigned char)c >= 32) t += c;
    s += "TITLE \"" + t + "\"\n";
    s += "LUT_3D_SIZE " + std::to_string(size) + "\n";
    s += "DOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n";
    char b[96];
    const float inv = 1.0f / (float)(size - 1);
    for (int bl = 0; bl < size; ++bl)
        for (int g = 0; g < size; ++g)
            for (int r = 0; r < size; ++r) {
                const float in[3] = {r * inv, g * inv, bl * inv};
                float out[3];
                ProcessPixel(o, in, out);
                for (float& v : out) v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                // Kropka dziesietna niezaleznie od ustawien regionalnych: formatowanie z liczb calkowitych.
                long long q[3] = {std::llround(out[0] * 1000000.0), std::llround(out[1] * 1000000.0),
                                  std::llround(out[2] * 1000000.0)};
                std::snprintf(b, sizeof b, "%lld.%06lld %lld.%06lld %lld.%06lld\n", q[0] / 1000000, q[0] % 1000000,
                              q[1] / 1000000, q[1] % 1000000, q[2] / 1000000, q[2] % 1000000);
                s += b;
            }
    return s;
}

}  // namespace zk
