# Zetmar Kolor (krzywe, LUT, toning) – wtyczka .8bf dla Photoshop Elements i Photoshopa

**Stan: wersja 1.0.0** (numer w `version.txt`, jedno źródło dla pliku wersji .8bf i paczek). Zakładki: **Krzywe** (PCHIP,
RGB/R/G/B, „tylko jasność”, presety, histogram), **Cz-B** (czerń-biel z mikserem 6 kolorów, sepia, monochromia), **Duotone** (mapa gradientu 2 kolorów),
**LUT** (`.cube`, tetraedralnie, siła), **Toning** (światła / cienie, balans), **Biblioteka** (miniatury, dodawanie,
usuwanie do Kosza), **Test** (diagnostyka, tylko z `debug.flag`). Pod podglądem: **Zapisz / Wczytaj look** (cały wygląd do pliku `.zklook`) i **Eksport do .cube** (cały potok jako LUT 33³ do użycia w innych programach). Potok: krzywe → cz-b → duotone → LUT → toning → siła ogólna. Podgląd: po / przed / podział.
Opis testu i ustalenia: [`docs/TEST_HOSTA.md`](docs/TEST_HOSTA.md).
Specyfikacja: `SPECYFIKACJA_Kolor_8bf.md` (w repozytorium Watermarka).

Menu: **Filtr → Zetmar → Kolor (krzywe, LUT, toning)...**
Interfejs PL/EN: tabela `src/core/i18n_strings.inc`; język wybierany w oknie, zapisywany w `settings.json`.

## Budowanie

Wymagania jak w Watermarku: Visual Studio 2022 / Build Tools (C++, CMake, Ninja) i Adobe Photoshop SDK.
SDK nie jest w repozytorium. Domyślnie brane z `sdk/pluginsdk`, a gdy go nie ma, z `../Photoshop/sdk/pluginsdk`
(zmiana: zmienna CMake `PS_SDK_DIR`).

```
build.cmd              # wszystko (Release x64) do build\
build.cmd zk_tests     # tylko testy (rdzeń, krzywe, parser .cube, biblioteka, presety, eksport)
ctest --test-dir build  # to samo przez CTest (po zbudowaniu)
```

Wyniki: `build\ZetmarKolor_64.8bf`, `build\zk_tests.exe`, `build\zk_ui_harness.exe <png> <1=PL|2=EN> [.cube|-] [zakładka 0-3] [preset krzywej] [folder biblioteki|-] [interact]` (zrzut okna bez Photoshopa).

## Paczki dystrybucyjne

```
powershell -ExecutionPolicy Bypass -File package.ps1 [-LutDir <folder z .cube>] [-NoBuild]
```

Buduje i testuje, a potem tworzy w `dist\`:
- `ZetmarKolor_<wersja>.zip`: wtyczka, `LUTs\`, `install.ps1`, `INSTRUKCJA_PL.txt`, `INSTALL_EN.txt` (instalacja ręczna lub skryptem),
- `ZetmarKolor_<wersja>_Setup.exe`: instalator automatyczny (Inno Setup 6, PL/EN), który wykrywa Photoshopa i Elements.

Domyślnie brane z `LUTS\Zemar-Cyfrowy_Przyjaciel_60_LUTs` (60 własnych LUT-ów), a instalowane do `C:\ProgramData\ZetmarKolor\LUTs`.
Pochodzenie i licencje: [`docs/LUTY_POCHODZENIE.md`](docs/LUTY_POCHODZENIE.md).

## Instalacja (z repozytorium)

```
powershell -ExecutionPolicy Bypass -File install.ps1
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

Elements: alternatywnie Edit > Preferences > Plug-Ins > Additional Plug-Ins Folder.

## Dane

`%APPDATA%\ZetmarKolor\settings.json`: język (0 auto, 1 PL, 2 EN), ostatnie foldery, folder biblioteki.
`%APPDATA%\ZetmarKolor\diagnostic_report.txt`: raport diagnostyczny (tylko gdy istnieje `debug.flag`).
`%APPDATA%\ZetmarKolor\debug.flag`: jeśli istnieje, włącza tryb diagnostyczny: zakładka Test, raport i `log.txt`.

## Ograniczenia

Wtyczka działa w trybie RGB, 8 i 16 bitów na kanał, na Windows 64-bit. Skala szarości, CMYK, Lab i 32 bity są wyszarzone (nieobsługiwane). Nie zapisuje parametrów w Akcjach ani w Filtrach inteligentnych Photoshopa; Ctrl+F powtarza ostatnie ustawienia w tej samej sesji. LUT-y zakładają dane zakodowane jak w dokumencie (profil ICC nie jest uwzględniany).
