# Pochodzenie LUT-ów z paczki

> **Aktualizacja (wersja 1.0.0):** paczka zawiera 60 własnych LUT-ów „Zemar-Cyfrowy Przyjaciel” (5 kategorii po 12; nagłówki `TITLE` z nazwą autora, w pliku `README.txt` paczki wymienione LUT-y referencyjne Aqua, BW10, Cinematic-5). Sprawdzenie parserem (`zk_cubecheck`): 60 poprawnych, 0 uszkodzonych; w nagłówkach nie ma cudzych znaków praw autorskich. **Do potwierdzenia przez autora przed sprzedażą:** że LUT-y referencyjne były tylko wzorcem stylu, a pliki powstały niezależnie. Opis poniżej dotyczy starej paczki 0.1.0 i zostaje jako historia decyzji.


Specyfikacja (4.2) mówi: do wtyczki dołączać **wyłącznie LUT-y własne autora**. **2026-10-05:** 19 spornych plików (grupy Lutify.me i darmowe presety innych autorów z tabeli niżej) przeniesiono do `Photoshop\LUTS_sporne`. Paczka 0.1.0 zawiera pozostałe 71 plików z `Photoshop\LUTS`. Do repozytorium nie trafiają: `package.ps1` pobiera je z zewnętrznego folderu (`-LutDir`).

Nagłówki plików pokazują ich źródło:

| Grupa | Pliki | Co mówi nagłówek | Ryzyko |
|---|---|---|---|
| **Lutify.me** | 2-Strip-Process, Berlin Sky, Chrome 01, Classic Teal and Orange, Fade to Green, Film Print 01, Film Print 02, French Comedy, Studio Skin Tone Shaper, Vintage Chrome (10) | `Copyright Lutify.me 2016, G.T. Colord LTD` | **wysokie**: cudzy copyright wprost w pliku |
| Darmowe presety innych autorów | BW1 (Catch-A-Fire, Lookfilter), BW2 (Monochromatic 04 – Free), BW7, BW9, Moody1, Moody3 (Free-LR-Presets.co), Moody9 (Lovely Landscapes 07 – Free), LC4 (Sleeklens Starter), Moody2 (CatchaFirecolor) | eksport z presetu Lightrooma o tej nazwie | **wysokie**: licencje darmowych presetów zwykle zabraniają redystrybucji i sprzedaży |
| Eksport z presetów Lightrooma, źródło nieznane | pozostałe (Cinematic, Landscape, Portrait / „Portrait Effect”, NW, LC, Moody, BW, Aqua, Blues, Reds, Oranges, Purple, Green_*, Earth Tone Boost i in.) | `Created by Adobe Lightroom plugin Export LUT` + nazwa presetu | do sprawdzenia: OK tylko, jeśli presety są Twoje |
| „CP - INTERIOR DESIGN” | Moody7, Moody8, Portrait9, Portrait10 | preset „CP - INTERIOR DESIGN …” | prawdopodobnie własne (CP = Cyfrowy Przyjaciel?), do potwierdzenia |

Żeby wyłączyć wybrane pliki z paczki: przenieś je z folderu LUT-ów albo wskaż inny folder: `package.ps1 -LutDir <folder>`.
