# Test w programie (Photoshop i Elements): wyniki i lista kontrolna

Dokument powstał przy pierwszym prototypie (2026-10-05); od tego czasu wtyczka urosła do wersji 1.0.0 (krzywe, cz-b, duotone, LUT, toning, biblioteka, presety Look, eksport .cube).
Testy rdzenia (`build\zk_tests.exe`) przechodzą bez Photoshopa. **Poniższa lista kontrolna w prawdziwych programach (Elements 2026, Photoshop 2026) nadal czeka na wypełnienie tabeli w sekcji 5**; do tego czasu zachowanie hosta (zaznaczenia, `maskData`, przezroczystość, kafelki, Ctrl+F) uznajemy za niepotwierdzone. Prototypowe opcje (efekt testowy, własne maskowanie, kafelki) są dostępne tylko w trybie diagnostycznym (`%APPDATA%\ZetmarKolor\debug.flag`, zakładka Test).

## 1. Zakres testowy (zakładka Test)

`build\ZetmarKolor_64.8bf`, menu **Filtr → Zetmar → Kolor (krzywe, LUT, toning)...**

- Okno testowe (tylko PL, celowo surowe): podgląd aktywnej warstwy z szachownicą, prosty efekt (S-krzywa / negatyw / cz-b) z suwakiem siły, przełącznik przed/po.
- Opcje testowe:
  - **Maskowanie zaznaczenia:** *Host (autoMask)*, czyli zaznaczenie i wtapianie robi program, albo *Własne (maskData)*: wtyczka wyłącza `autoMask` i sama miesza wynik według maski.
  - **Podział na kafelki:** paski ~16 MB, kafelki 256×256, kafelki 1024×1024. Opcja *Zaznacz granice kafelków* rysuje magentowe linie na krawędziach kafelków.
  - **Dithering** deterministyczny, liczony ze współrzędnych piksela.
- Zapis: float 0–1, wielowątkowo po wierszach, 8 i 16 bit, przezroczystość nietknięta, piksele z alfa = 0 pomijane.
- Ctrl+F powtarza filtr bez okna (parametry w uchwycie filtra).
- **Raport:** każde uruchomienie dopisuje pełną diagnostykę do `%APPDATA%\ZetmarKolor\diagnostic_report.txt`: pola `FilterRecord`, wersję hosta, czasy podglądu i zapisu oraz statystykę maski. Ta sama treść jest w oknie, z przyciskiem *Kopiuj diagnostykę*.

Podgląd jest czytany dwiema metodami naraz, a raport pokazuje, która zadziałała i ile trwała:
- **A:** channel ports (`documentInfo->targetCompositeChannels` + `targetTransparency`, skalowanie po stronie hosta),
- **B:** `advanceState` z `inputRate` (próbkowanie całkowite, jak w Watermarku).

## 2. Ustalenia z SDK (bez programu)

| Temat | Ustalenie | Pewność |
|---|---|---|
| `autoMask` | Domyślnie host sam maskuje zmiany do zaznaczenia (także miękkie krawędzie). Wtyczka może to wyłączyć tylko przy `haveMask=TRUE` i `isFloating=FALSE`. Z `WriteOutsideSelection=false` w PiPL (tak jak w Watermarku) host zachowuje się jak przy innych filtrach. | z nagłówka `PIFilter.h` |
| `filterRect` | Obwiednia zaznaczenia. Prototyp przetwarza tylko ten prostokąt, a reszta warstwy nie jest dotykana. | z nagłówka |
| `maskData` | **Sprzeczność w SDK.** Komentarz w `PIFilter.h` mówi „0 = zaznaczone, 255 = niezaznaczone”, a przykład *Dissolve* z tego samego SDK zostawia piksele z maską 0 bez zmian (czyli 255 = zaznaczone). Prototyp przyjmuje wersję z przykładu. Raport loguje min/max/udziały wartości, a test 3.2 to rozstrzyga. | do potwierdzenia |
| Kafelki | `inRect`/`outRect`/`maskRect` ustawiane per kafelek, współrzędne 32-bitowe (`bigDocumentData`), postęp i przerwanie (`abortProc`). Ten sam mechanizm działa w Watermarku w obu programach. | wysoka (Watermark) |
| Podgląd w zmniejszeniu | Metoda B jest sprawdzona w Watermarku. Metoda A jest szybsza i daje też kanał zaznaczenia do podglądu, ale nie wiadomo, czy Elements ją udostępnia. | do potwierdzenia |
| Wersja hosta | `propVersion` przez Property Suite. Rozkład bitów (major/minor/fix) nie jest opisany w nagłówku, więc raport podaje też surową wartość hex. | do potwierdzenia |

Testy automatyczne (`build\zk_tests.exe`, wszystkie OK):
- siła 0 przy włączonym ditheringu daje wynik identyczny co do bitu (8/16 bit, z alfą i bez),
- wynik nie zależy od podziału na kafelki (37×53, 256×16) ani od liczby wątków (1 i 8),
- przezroczystość nietknięta, piksele z alfa = 0 pominięte, 16 bit nie przekracza 32768,
- maska 0 nie zmienia obrazu, a szum ditheringu mieści się w (−0,5; 0,5) LSB.

## 3. Test do wykonania przez Marka (Elements 2026 i Photoshop 2026)

Instalacja: `powershell -ExecutionPolicy Bypass -File install.ps1` (kopiuje do `Plug-Ins\Zetmar` każdej znalezionej instalacji, prosi o UAC). Alternatywa dla Elements: Preferences > Plug-ins > Additional Plugins Folder wskazujący katalog z `ZetmarKolor_64.8bf`. Po instalacji uruchom program ponownie.

Przed testem usuń stary `%APPDATA%\ZetmarKolor\diagnostic_report.txt`, a po teście przyślij ten plik.

1. **Ładowanie i menu.** Czy pozycja jest w Filtr → Zetmar? Czy jest wyszarzona w trybie Skala szarości i w 32 bit (tylko PS)?
2. **Zaznaczenie z miękką krawędzią.** Na zdjęciu (warstwa tła) zaznacz elipsę z wtapianiem 50 px. Uruchom filtr z efektem *Negatyw*, maskowanie *Host*, OK. Czy przejście jest miękkie, a poza zaznaczeniem nic się nie zmieniło? Cofnij (Ctrl+Z).
3. **To samo z maskowaniem *Własne (maskData)*.** Czy wynik wygląda tak samo jak w punkcie 2? Jeśli efekt jest „odwrócony” (zmienia się to, co poza zaznaczeniem, w obrębie obwiedni), odpowiedź brzmi: komentarz w SDK ma rację, a przykład Dissolve nie. Zapisz, co widać.
4. **Warstwa z przezroczystością.** Nowa warstwa z kilkoma pociągnięciami pędzla na przezroczystym tle. Filtr (S-krzywa). Czy podgląd pokazuje szachownicę? Czy krawędzie pędzla (półprzezroczyste) zostały, a przezroczyste tło pozostało przezroczyste?
5. **Kafelki.** Duże zdjęcie (np. >10 000 px albo powiększ obraz do ~100 MP). Filtr z *Kafelki 256×256* i *Zaznacz granice kafelków*. Czy widać regularną magentową siatkę bez przesunięć? Potem bez znaczników, *Paski ~16 MB*: czy działa bez błędu pamięci? Czas zapisu jest w raporcie.
6. **16 bit.** Obraz > Tryb > 16 bitów/kanał, S-krzywa. Czy wynik jest poprawny (bez przekłamań kolorów)?
7. **Ctrl+F.** Po OK naciśnij Ctrl+F. Czy filtr powtarza się bez okna? Czy jedno Ctrl+Z cofa jedno uruchomienie?
8. **LUT (od wersji 0.2).** *Wybierz…*, wskaż plik z `C:\ProgramData\ZetmarKolor\LUTs`. Sprawdź podgląd *Podział: przed | po* i przytrzymanie myszy na podglądzie, potem OK. Następnie w Photoshopie: ten sam plik w *Dopasowania > Wyszukiwanie kolorów* (Color Lookup) na kopii warstwy i tryb *Różnica*. Przy 100% różnice powinny być ≤ 2 poziomy (kryterium 4 specyfikacji). Ctrl+F powinien użyć tego samego LUT-a.
9. **Wersja programu:** Pomoc > Informacje o systemie, dokładny numer kompilacji Photoshopa i Elements:
   - Photoshop: `__________`
   - Elements: `__________`

## 4. Pytania do Marka

1. Wynik punktów 1–7 w Elements: czy coś nie działa? Zgodnie ze specyfikacją, **jeśli selekcje albo kafelki nie działają w Elements, zatrzymuję pracę** i szukamy obejścia.
2. Raport `diagnostic_report.txt` z obu programów (rozstrzyga kwestie: metoda podglądu A/B, `maskData`, czy Elements wypełnia te same pola co Photoshop: `filterCase`, `inLayerPlanes`, `inTransparencyMask`, `documentInfo`, `bigDocumentData`).
3. ~~Repozytorium~~: potwierdzone 2026-10-05, osobny katalog `C:\Users\Marek\Claude\ZetmarKolor`.
4. LUT-y: na prośbę Marka (2026-10-05) są w ZIP-ie i instalatorze wersji testowej, ale nie w repozytorium. **Część ma cudzy copyright** (szczegóły: [`LUTY_POCHODZENIE.md`](LUTY_POCHODZENIE.md)). Decyzja przed sprzedażą.

## 5. Wyniki (do uzupełnienia po teście)

| Punkt | Photoshop 2026 | Elements 2026 |
|---|---|---|
| 1. Menu / wyszarzenie | | |
| 2. Zaznaczenie, host | | |
| 3. Zaznaczenie, maskData | | |
| 4. Przezroczystość | | |
| 5. Kafelki / duże zdjęcie | | |
| 6. 16 bit | | |
| 7. Ctrl+F / Cofnij | | |
| Podgląd: metoda A / B | | |
