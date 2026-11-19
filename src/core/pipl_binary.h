// Klasyczny binarny zasob PIPL (typ "PIPL", id 16000) dla wtyczki .8bf. Nowe wersje Photoshopa i Elements czytaja
// tez opis JSON (res/pipl.json), ale inne programy obslugujace wtyczki Photoshopa (np. Affinity) szukaja
// wylacznie klasycznego PIPL - bez niego wtyczka w ogole sie nie pojawia. Zasob jest generowany w czasie
// budowania (tools/make_pipl.cpp) i dolaczany w res/plugin.rc.
//
// Uklad (little-endian, jak w zasobach PIPL wtyczek Adobe dla Windows): naglowek 6 bajtow {1,0,0,0,0,0},
// uint32 liczba wlasciwosci, potem wlasciwosci: "MIB8" (vendor 8BIM odwrocony), klucz odwrocony (4 znaki),
// uint32 indeks (0), uint32 dlugosc danych, dane dopelnione zerami do wielokrotnosci 4.
//
// UWAGA: wartosci musza byc zgodne z res/pipl.json (nazwa, kategoria, EnableInfo, FilterCaseInfo, ...).
#pragma once
#include <cstdint>
#include <vector>

namespace zk {

std::vector<uint8_t> BuildClassicPipl();

}  // namespace zk
