#include "core/cube_lut.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <locale.h>

namespace zk {

namespace {

// Liczby zawsze z kropka dziesietna, niezaleznie od ustawien regionalnych hosta.
_locale_t CLocale() {
    static _locale_t loc = _create_locale(LC_NUMERIC, "C");
    return loc;
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

// Jedna linia bez komentarza i bialych znakow na koncach.
struct Line {
    const char* b;
    const char* e;
    bool empty() const { return b >= e; }
};

Line Trim(const char* b, const char* e) {
    // Komentarz zaczyna sie od '#', ale nie wewnatrz cudzyslowu (np. TITLE "Look #3").
    bool quoted = false;
    for (const char* q = b; q < e; ++q) {
        if (*q == '"') quoted = !quoted;
        else if (*q == '#' && !quoted) { e = q; break; }
    }
    while (b < e && IsSpace(*b)) ++b;
    while (e > b && IsSpace(e[-1])) --e;
    return {b, e};
}

bool StartsWithKeyword(const Line& l, const char* kw) {
    size_t n = std::strlen(kw);
    return (size_t)(l.e - l.b) >= n && std::memcmp(l.b, kw, n) == 0 &&
           ((size_t)(l.e - l.b) == n || IsSpace(l.b[n]));
}

// Czyta do `count` liczb z linii; zwraca liczbe odczytanych albo -1 przy smieciach / NaN / Inf.
int ReadFloats(const char* b, const char* e, float* out, int count) {
    char buf[256];
    size_t n = std::min((size_t)(e - b), sizeof(buf) - 1);
    if ((size_t)(e - b) >= sizeof(buf)) return -1;
    std::memcpy(buf, b, n);
    buf[n] = 0;
    char* p = buf;
    int got = 0;
    while (true) {
        while (*p && IsSpace(*p)) ++p;
        if (!*p) break;
        if (got == count) return -1;  // za duzo liczb
        char* end = nullptr;
        double v = _strtod_l(p, &end, CLocale());
        if (end == p || (*end && !IsSpace(*end))) return -1;
        if (!std::isfinite(v) || std::fabs(v) > 1e6) return -1;
        out[got++] = (float)v;
        p = end;
    }
    return got;
}

bool ReadInt(const Line& l, size_t kwLen, long& v) {
    const char* p = l.b + kwLen;
    while (p < l.e && IsSpace(*p)) ++p;
    if (p >= l.e) return false;
    char buf[32];
    size_t n = (size_t)(l.e - p);
    if (n >= sizeof(buf)) return false;
    std::memcpy(buf, p, n);
    buf[n] = 0;
    char* end = nullptr;
    v = std::strtol(buf, &end, 10);
    return end != buf && *end == 0;
}

}  // namespace

CubeResult ParseCubeText(const char* text, size_t len, CubeLut& out) {
    out = CubeLut();
    CubeResult r;
    if ((int64_t)len > kMaxCubeFileBytes) { r.error = CubeError::TooLarge; return r; }
    // Tekst: brak bajtow zerowych i innych znakow sterujacych (poza bialymi znakami).
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != '\f' && c != '\v') {
            r.error = CubeError::Binary;
            return r;
        }
    }
    size_t start = 0;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        start = 3;

    std::vector<float> rows;  // wszystkie wiersze danych (po 3 liczby)
    bool dataStarted = false;
    long size3 = 0, size1 = 0;
    int lineNo = 0;
    const char* end = text + len;
    for (const char* p = text + start; p < end;) {
        // Koniec linii: \n, \r\n albo samo \r (stare pliki z Mac OS).
        const char* nl = p;
        while (nl < end && *nl != '\n' && *nl != '\r') ++nl;
        ++lineNo;
        Line l = Trim(p, nl);
        if (nl + 1 < end && *nl == '\r' && nl[1] == '\n') ++nl;
        p = nl < end ? nl + 1 : end;
        if (l.empty()) continue;

        char c = *l.b;
        const bool numeric = (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
        if (numeric) {
            float v[3];
            if (ReadFloats(l.b, l.e, v, 3) != 3) { r.error = CubeError::BadNumber; r.line = lineNo; return r; }
            if (size3 == 0 && size1 == 0) { r.error = CubeError::NoSize; r.line = lineNo; return r; }
            const size_t expected = (size_t)size1 * 3 + (size_t)size3 * size3 * size3 * 3;
            if (rows.size() + 3 > expected) { r.error = CubeError::WrongRowCount; r.line = lineNo; return r; }
            rows.insert(rows.end(), v, v + 3);
            dataStarted = true;
            continue;
        }
        if (dataStarted) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }  // slowa kluczowe po danych
        if (StartsWithKeyword(l, "TITLE")) {
            const char* q1 = (const char*)std::memchr(l.b, '"', (size_t)(l.e - l.b));
            const char* q2 = q1 ? (const char*)std::memchr(q1 + 1, '"', (size_t)(l.e - q1 - 1)) : nullptr;
            if (q1 && q2) out.title.assign(q1 + 1, q2);
            else {
                const char* s = l.b + 5;
                while (s < l.e && IsSpace(*s)) ++s;
                out.title.assign(s, l.e);
            }
            if (out.title.size() > 200) out.title.resize(200);
        } else if (StartsWithKeyword(l, "LUT_3D_SIZE")) {
            if (!ReadInt(l, 11, size3)) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }
            if (size3 < 2 || size3 > 65) { r.error = CubeError::BadSize; r.line = lineNo; return r; }
        } else if (StartsWithKeyword(l, "LUT_1D_SIZE")) {
            if (!ReadInt(l, 11, size1)) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }
            if (size1 < 2 || size1 > 65536) { r.error = CubeError::BadSize; r.line = lineNo; return r; }
        } else if (StartsWithKeyword(l, "DOMAIN_MIN") || StartsWithKeyword(l, "DOMAIN_MAX")) {
            float v[3];
            if (ReadFloats(l.b + 10, l.e, v, 3) != 3) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }
            float* dst = l.b[8] == 'I' ? out.domainMin : out.domainMax;
            for (int i = 0; i < 3; ++i) dst[i] = v[i];
        } else if (StartsWithKeyword(l, "LUT_1D_INPUT_RANGE") || StartsWithKeyword(l, "LUT_3D_INPUT_RANGE")) {
            // Wariant z DaVinci Resolve: jeden zakres dla wszystkich kanalow.
            float v[2];
            if (ReadFloats(l.b + 18, l.e, v, 2) != 2) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }
            for (int i = 0; i < 3; ++i) { out.domainMin[i] = v[0]; out.domainMax[i] = v[1]; }
        } else {
            // Nieznane slowo kluczowe (np. LUT_IN_VIDEO_RANGE): dozwolone, jesli wyglada jak identyfikator.
            bool ident = true;
            for (const char* s = l.b; s < l.e && !IsSpace(*s); ++s)
                if (!((*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')) { ident = false; break; }
            if (!ident) { r.error = CubeError::BadKeyword; r.line = lineNo; return r; }
        }
    }
    if (size3 == 0 && size1 == 0) { r.error = CubeError::NoSize; return r; }
    const size_t expected = (size_t)size1 * 3 + (size_t)size3 * size3 * size3 * 3;
    if (rows.size() != expected) { r.error = CubeError::WrongRowCount; return r; }
    for (int i = 0; i < 3; ++i)
        if (!(out.domainMin[i] < out.domainMax[i])) { r.error = CubeError::BadDomain; return r; }

    // Kolejnosc jak w pliku: najpierw tablica 1D (shaper), potem 3D.
    out.size1d = (int)size1;
    out.size3d = (int)size3;
    out.data1d.assign(rows.begin(), rows.begin() + (ptrdiff_t)size1 * 3);
    out.data3d.assign(rows.begin() + (ptrdiff_t)size1 * 3, rows.end());
    return r;
}

CubeResult LoadCubeFile(const std::wstring& path, CubeLut& out) {
    out = CubeLut();
    CubeResult r;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) { r.error = CubeError::OpenFailed; return r; }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); r.error = CubeError::OpenFailed; return r; }
    if (sz.QuadPart > kMaxCubeFileBytes) { CloseHandle(h); r.error = CubeError::TooLarge; return r; }
    std::string buf((size_t)sz.QuadPart, '\0');
    DWORD got = 0;
    bool ok = buf.empty() || (ReadFile(h, buf.data(), (DWORD)buf.size(), &got, nullptr) && got == buf.size());
    CloseHandle(h);
    if (!ok) { r.error = CubeError::OpenFailed; return r; }
    return ParseCubeText(buf.data(), buf.size(), out);
}

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void Sample1D(const CubeLut& lut, const float in[3], float out[3]) {
    const int n = lut.size1d;
    for (int c = 0; c < 3; ++c) {
        float t = Clamp01((in[c] - lut.domainMin[c]) / (lut.domainMax[c] - lut.domainMin[c])) * (n - 1);
        int i0 = std::min((int)t, n - 2);
        float f = t - i0;
        float a = lut.data1d[(size_t)i0 * 3 + c], b = lut.data1d[(size_t)(i0 + 1) * 3 + c];
        out[c] = a + (b - a) * f;
    }
}

void Sample3D(const CubeLut& lut, const float in[3], float out[3], bool trilinear) {
    const int n = lut.size3d;
    float t[3];
    int i0[3];
    float f[3];
    for (int c = 0; c < 3; ++c) {
        t[c] = Clamp01((in[c] - lut.domainMin[c]) / (lut.domainMax[c] - lut.domainMin[c])) * (n - 1);
        i0[c] = std::min((int)t[c], n - 2);
        f[c] = t[c] - i0[c];
    }
    const float* d = lut.data3d.data();
    auto at = [&](int dr, int dg, int db) {
        return d + (((size_t)(i0[2] + db) * n + (i0[1] + dg)) * n + (i0[0] + dr)) * 3;
    };
    const float* c000 = at(0, 0, 0);
    const float* c111 = at(1, 1, 1);
    if (trilinear) {
        const float* c100 = at(1, 0, 0); const float* c010 = at(0, 1, 0); const float* c001 = at(0, 0, 1);
        const float* c110 = at(1, 1, 0); const float* c101 = at(1, 0, 1); const float* c011 = at(0, 1, 1);
        for (int k = 0; k < 3; ++k) {
            float x00 = c000[k] + (c100[k] - c000[k]) * f[0];
            float x10 = c010[k] + (c110[k] - c010[k]) * f[0];
            float x01 = c001[k] + (c101[k] - c001[k]) * f[0];
            float x11 = c011[k] + (c111[k] - c011[k]) * f[0];
            float y0 = x00 + (x10 - x00) * f[1];
            float y1 = x01 + (x11 - x01) * f[1];
            out[k] = y0 + (y1 - y0) * f[2];
        }
        return;
    }
    // Interpolacja tetraedralna: szescian dzielony na 6 czworoscianow wg kolejnosci ulamkow fr, fg, fb.
    const float fr = f[0], fg = f[1], fb = f[2];
    const float *a, *b;
    float w0, w1, w2, w3;  // wagi: c000, a, b, c111
    if (fr > fg) {
        if (fg > fb) { a = at(1, 0, 0); b = at(1, 1, 0); w0 = 1 - fr; w1 = fr - fg; w2 = fg - fb; w3 = fb; }
        else if (fr > fb) { a = at(1, 0, 0); b = at(1, 0, 1); w0 = 1 - fr; w1 = fr - fb; w2 = fb - fg; w3 = fg; }
        else { a = at(0, 0, 1); b = at(1, 0, 1); w0 = 1 - fb; w1 = fb - fr; w2 = fr - fg; w3 = fg; }
    } else {
        if (fb > fg) { a = at(0, 0, 1); b = at(0, 1, 1); w0 = 1 - fb; w1 = fb - fg; w2 = fg - fr; w3 = fr; }
        else if (fb > fr) { a = at(0, 1, 0); b = at(0, 1, 1); w0 = 1 - fg; w1 = fg - fb; w2 = fb - fr; w3 = fr; }
        else { a = at(0, 1, 0); b = at(1, 1, 0); w0 = 1 - fg; w1 = fg - fr; w2 = fr - fb; w3 = fb; }
    }
    for (int k = 0; k < 3; ++k) out[k] = w0 * c000[k] + w1 * a[k] + w2 * b[k] + w3 * c111[k];
}

}  // namespace

void SampleCube(const CubeLut& lut, const float in[3], float out[3], bool trilinear) {
    float v[3] = {in[0], in[1], in[2]};
    if (lut.size1d > 0) {
        Sample1D(lut, v, out);
        v[0] = out[0]; v[1] = out[1]; v[2] = out[2];
    }
    if (lut.size3d > 0) Sample3D(lut, v, out, trilinear);
    else { out[0] = v[0]; out[1] = v[1]; out[2] = v[2]; }
}

}  // namespace zk
