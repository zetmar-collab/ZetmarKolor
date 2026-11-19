// LUT-y w formacie .cube (Adobe Cube LUT Specification 1.0): parser odporny na uszkodzone pliki i probkowanie
// (3D: tetraedralne lub trojliniowe, 1D: liniowe). Bez zaleznosci od Photoshopa.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zk {

enum class CubeError {
    Ok = 0,
    OpenFailed,     // nie da sie otworzyc / odczytac pliku
    TooLarge,       // plik wiekszy niz kMaxCubeFileBytes
    Binary,         // dane binarne / niepoprawny tekst
    NoSize,         // brak LUT_3D_SIZE i LUT_1D_SIZE
    BadSize,        // rozmiar poza zakresem (3D: 2..65, 1D: 2..65536)
    BadKeyword,     // zle zapisane slowo kluczowe (np. brak liczby po LUT_3D_SIZE)
    BadNumber,      // wiersz danych nie jest trzema liczbami, NaN albo nieskonczonosc
    WrongRowCount,  // liczba wierszy danych nie zgadza sie z rozmiarem
    BadDomain,      // DOMAIN_MIN >= DOMAIN_MAX
};

constexpr int64_t kMaxCubeFileBytes = 64LL << 20;  // 65^3 wierszy to ok. 9 MB, wiec 64 MB wystarczy z zapasem

struct CubeLut {
    std::string title;           // TITLE (UTF-8)
    int size3d = 0;              // 0 = brak tablicy 3D
    int size1d = 0;              // 0 = brak tablicy 1D
    float domainMin[3] = {0, 0, 0};
    float domainMax[3] = {1, 1, 1};
    std::vector<float> data3d;   // size3d^3 * 3, R zmienia sie najszybciej
    std::vector<float> data1d;   // size1d * 3
    bool valid() const { return size3d > 0 || size1d > 0; }
};

struct CubeResult {
    CubeError error = CubeError::Ok;
    int line = 0;  // numer wiersza bledu (1..), 0 = nie dotyczy
};

CubeResult ParseCubeText(const char* text, size_t len, CubeLut& out);
CubeResult LoadCubeFile(const std::wstring& path, CubeLut& out);

// Przeksztalca kolor 0..1 (wynik moze wyjsc poza 0..1 - przycina zapis). trilinear=false -> tetraedralnie.
void SampleCube(const CubeLut& lut, const float in[3], float out[3], bool trilinear = false);

}  // namespace zk
