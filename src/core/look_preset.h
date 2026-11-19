// Preset "Look": zapis i odczyt calego wygladu (krzywe, czern-biel, duotone, LUT, toning, sila ogolna) do pliku
// tekstowego .zklook oraz eksport potoku do LUT-a .cube. Bez zaleznosci od Photoshopa.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "plugin/host.h"

namespace zk {

constexpr int64_t kMaxLookFileBytes = 1 << 20;

// Tekst presetu (UTF-8, koniec linii \n). Ustawienia testowe (zakladka Test) nie sa zapisywane.
std::string LookToText(const FilterParams& p);

// Wczytuje preset do `p`. Zmienia tylko pola wygladu; przy bledzie zwraca false i nie zmienia `p`.
// Wartosci spoza zakresu sa przycinane (SanitizeParams).
bool LookFromText(const char* text, size_t len, FilterParams& p);

bool SaveLookFile(const std::wstring& path, const FilterParams& p);
bool LoadLookFile(const std::wstring& path, FilterParams& p);

// Probkuje caly potok (bez ditheringu) na siatce size^3 (2..65) i zwraca tekst .cube (R zmienia sie najszybciej).
// `lut` moze byc nullptr. Wynik mozna wczytac ParseCubeText.
std::string ExportPipelineToCube(const FilterParams& p, const CubeLut* lut, int size, const std::string& title);

}  // namespace zk
