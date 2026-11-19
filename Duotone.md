# Efekt Duotone w fotografii

**Duotone** polega na przekształceniu zdjęcia tonalnego w obraz zbudowany z dwóch kolorów: jednego dla cieni i drugiego dla świateł. Nie jest to zwykłe pokolorowanie zdjęcia — jasność każdego piksela zostaje przypisana do określonego miejsca na przejściu między dwoma wybranymi kolorami.

Najłatwiej wykonać ten efekt za pomocą **mapy gradientu**, ponieważ zachowuje oryginalne zdjęcie i pozwala później swobodnie zmieniać kolory. Adobe wskazuje, że duotone najlepiej działa na fotografiach o wyraźnym kontraście.\[[helpx.adobe](https://helpx.adobe.com/photoshop/using/duotones.html)\]\[[adobe](https://www.adobe.com/creativecloud/photography/discover/duotone-effect.html)\]

## Co oznaczają dwa kolory

W typowym duotonie:

-   ciemny kolor przypisuje się cieniom,
-   jasny lub kontrastowy kolor przypisuje się światłom,
-   tony pośrednie otrzymują mieszankę obu kolorów,
-   szczegóły zdjęcia pozostają zależne od jasności, a nie od oryginalnych barw.

Przykładowe zestawy:

| Kolor cieni | Kolor świateł | Charakter efektu |
| --- | --- | --- |

| Granatowy `#101A3A` | Pomarańczowy `#FF9A3D` | Kontrastowy, nowoczesny |
| Ciemna zieleń `#12352B` | Złoty `#F2C14E` | Naturalny, retro |
| Fiolet `#24113F` | Róż `#FF5C8A` | Artystyczny, modowy |
| Ciemny brąz `#2A1710` | Kremowy `#F5D6A1` | Analogowy, vintage |
| Czarny `#000000` | Turkus `#35D6D0` | Technologiczny, graficzny |

Dla portretów zwykle lepiej działają zestawy, w których najciemniejsze partie nie są całkowicie czarne. Bardzo twardy czarny kolor może sprawić, że twarz będzie wyglądała zbyt agresywnie.

# Photoshop — najlepsza metoda

## 1\. Przygotuj zdjęcie

1.  Otwórz fotografię w Photoshopie.
2.  Zduplikuj warstwę zdjęcia skrótem `Ctrl+J`.
3.  Zachowaj oryginał na dole i nazwij go na przykład `ORYGINAŁ`.
4.  Jeśli zdjęcie jest blade, dodaj najpierw:
    
    -   **Warstwa → Nowa warstwa dopasowania → Poziomy**, albo
    -   **Krzywe**, aby zwiększyć separację między światłami i cieniami.

Efekt duotone będzie czytelniejszy, jeśli fotografia ma wyraźne czernie, półtony i światła.\[[adobe](https://www.adobe.com/creativecloud/photography/discover/duotone-effect.html)\]

## 2\. Dodaj mapę gradientu

1.  Otwórz panel **Warstwy**.
2.  Kliknij ikonę tworzenia warstwy dopasowania.
3.  Wybierz **Mapa gradientu**.
4.  Kliknij pasek gradientu w panelu **Właściwości**.

Mapa gradientu działa niedestrukcyjnie: nie zmienia trwale pikseli zdjęcia, a kolory możesz później zmienić w dowolnym momencie. Adobe opisuje tę technikę jako mapowanie kolorów na wartości szarości za pomocą warstwy dopasowania.\[[adobe](https://www.adobe.com/creativecloud/photography/discover/duotone-effect.html)\]

## 3\. Ustaw dwa kolory

W edytorze gradientu zobaczysz dwa znaczniki:

-   lewy — odpowiada cieniom,
-   prawy — odpowiada światłom.

Ustaw na przykład:

-   lewy znacznik: `#101A3A`,
-   prawy znacznik: `#FF9A3D`.

Kolejność jest ważna: w standardowym układzie lewa strona gradientu odpowiada czerni i cieniom, a prawa — bieli i światłom.

## 4\. Dostosuj punkt środkowy

Pomiędzy kolorami znajduje się mały romb — **punkt środkowy gradientu**.

Przesuwaj go, aby zmienić proporcję kolorów:

-   w lewo — więcej partii zdjęcia otrzyma kolor świateł,
-   w prawo — większa część obrazu pozostanie w kolorze cieni,
-   na środku — uzyskasz bardziej równomierne przejście.

Przykład:

-   portret z jasnym tłem: przesuń punkt środkowy lekko w prawo,
-   ciemne zdjęcie uliczne: przesuń go lekko w lewo,
-   fotografia z dużą ilością nieba: przetestuj położenie około 45–55%.

## 5\. Popraw kontrast

Jeżeli zdjęcie wygląda zbyt płasko:

1.  Dodaj nad mapą gradientu warstwę **Krzywe**.
2.  Utwórz delikatną krzywą w kształcie litery „S”.
3.  Przyciemnij cienie.
4.  Lekko rozjaśnij światła.
5.  Jeśli efekt jest zbyt mocny, zmniejsz krycie warstwy dopasowania albo zmniejsz kontrast.

Praktyczna kolejność warstw:

```
Krzywe — opcjonalnie
Mapa gradientu
Krzywe / Czarno-białe — opcjonalnie
Zdjęcie
ORYGINAŁ
```

# Photoshop — metoda z trybem Duotone

Photoshop ma także właściwy tryb **Duotone**, przeznaczony szczególnie do przygotowania plików drukowanych dwoma farbami.

## Procedura

1.  Otwórz zdjęcie.
2.  Wybierz **Obraz → Tryb → Skala szarości**.
3.  Jeśli Photoshop zapyta o odrzucenie informacji kolorystycznych, zaakceptuj.
4.  Wybierz **Obraz → Tryb → Duotone**.
5.  W polu **Typ** wybierz **Duotone**.
6.  Kliknij próbkę pierwszego koloru i ustaw kolor dla cieni.
7.  Kliknij próbkę drugiego koloru i ustaw kolor dla półtonów oraz świateł.
8.  Kliknij pole krzywej obok każdego koloru.
9.  Dostosuj udział farby w poszczególnych zakresach tonalnych.
10.  Zatwierdź przyciskiem **OK**.

Photoshop pozwala używać nie tylko duotonu, ale także monotone, tritone i quadtone. Właściwy tryb Duotone działa na 8-bitowym obrazie w skali szarości i steruje rozkładem farb za pomocą osobnych krzywych.\[[helpx.adobe](https://helpx.adobe.com/photoshop/using/duotones.html)\]

## Jak rozumieć krzywe duotonu

W oknie krzywej:

-   oś pozioma przedstawia wartości od świateł do cieni,
-   oś pionowa określa procent użycia danej farby,
-   podniesienie krzywej zwiększa udział koloru,
-   obniżenie krzywej ogranicza jego obecność.

Przykład dla granatowo-pomarańczowego duotonu:

-   granatowy: mocny w cieniach, słabszy w światłach,
-   pomarańczowy: umiarkowany w półtonach, silniejszy w światłach.

Dzięki temu możesz uzyskać efekt bardziej fotograficzny niż przy zwykłym liniowym gradiencie. Photoshop pozwala zapisywać i ponownie ładować ustawienia krzywych duotonu.\[[helpx.adobe](https://helpx.adobe.com/photoshop/using/duotones.html)\]

# Najlepszy workflow do zdjęć

Do grafiki internetowej, okładki ebooka, miniatury lub posta użyj tej kolejności:

1.  Przygotuj zdjęcie w RGB.
2.  Usuń dominujący zafarb i popraw balans tonalny.
3.  Dodaj warstwę dopasowania **Czarno-białe**.
4.  Zwiększ kontrast za pomocą **Poziomów** lub **Krzywych**.
5.  Dodaj **Mapę gradientu**.
6.  Wybierz kolor cieni i kolor świateł.
7.  Przesuń punkt środkowy gradientu.
8.  Zmniejsz krycie mapy gradientu, jeśli efekt jest zbyt intensywny.
9.  Dodaj maskę warstwy i przywróć oryginalny kolor w wybranych obszarach, jeśli potrzebujesz częściowego duotonu.
10.  Eksportuj kopię do PNG lub JPG.

Nie spłaszczaj pliku PSD, jeśli chcesz później łatwo zmieniać kolory.

# Kontrolowany efekt częściowy

Możesz zastosować duotone tylko do części fotografii:

1.  Dodaj mapę gradientu.
2.  Kliknij maskę warstwy.
3.  Wypełnij maskę czarnym kolorem, aby ukryć efekt.
4.  Białym miękkim pędzlem pomaluj obszary, które mają zostać zabarwione.
5.  Zmniejsz krycie pędzla do 20–40%, aby uzyskać delikatne przejścia.

Dobry przykład to kolorowanie tylko osoby na zdjęciu, przy jednoczesnym pozostawieniu tła w czerni i bieli.

# Częste błędy

-   **Zbyt mały kontrast zdjęcia** — duotone wygląda wtedy jak wyblakły filtr.
-   **Dwa kolory o podobnej jasności** — obraz traci czytelność.
-   **Czysta czerń i bardzo jasny neon** — efekt może wyglądać sztucznie.
-   **Brak kontroli nad półtonami** — twarz może mieć nienaturalny kolor.
-   **Zbyt intensywne nasycenie** — fotografia zaczyna wyglądać jak grafika, a nie zdjęcie.
-   **Praca destrukcyjna na oryginale** — utrudnia późniejsze poprawki.
-   **Brak sprawdzenia obrazu w małym rozmiarze** — drobne szczegóły mogą zniknąć po publikacji.

# Szybkie ustawienie startowe

Na początek użyj takiego zestawu:

```
Cienie:   #17213D
Światła:  #F2A65A
Kontrast: delikatna krzywa S
Punkt środkowy gradientu: 48%
Krycie mapy gradientu: 100%
```

Dla zdjęcia portretowego najpierw ustaw mapę gradientu, a następnie zmniejsz nasycenie jaśniejszego koloru, jeśli skóra wygląda zbyt pomarańczowo. Dla fotografii miejskiej możesz użyć granatu i pomarańczu z mocniejszym kontrastem; dla architektury dobrze sprawdzą się zieleń i złoto.

W praktyce do większości cyfrowych projektów polecam **Mapę gradientu**, natomiast tryb **Obraz → Tryb → Duotone** warto wybrać wtedy, gdy przygotowujesz rzeczywisty materiał drukowany i potrzebujesz kontroli nad farbami oraz krzywymi ich nakładania.\[[helpx.adobe](https://helpx.adobe.com/photoshop/using/duotones.html)\]\[[adobe](https://www.adobe.com/creativecloud/photography/discover/duotone-effect.html)\]