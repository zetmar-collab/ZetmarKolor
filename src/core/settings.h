// Ustawienia trwale: %APPDATA%\ZetmarKolor\settings.json (jezyk, ostatni folder LUT-ow).
#pragma once
#include <string>

namespace zk {

struct AppSettings {
    int language = 0;  // 0 = automatycznie (z Windows), 1 = polski, 2 = angielski
    std::wstring lastLutDir;     // ostatni folder w oknie "Wybierz..."
    std::wstring lutLibraryDir;  // folder biblioteki (pusty = domyslny)
    std::wstring lastLookDir;    // ostatni folder zapisu / odczytu presetow "Look"
};

// Katalog %APPDATA%\ZetmarKolor (tworzony). Pusty, gdy nie da sie ustalic.
std::wstring DataDir();

// Tryb diagnostyczny: istnieje plik %APPDATA%\ZetmarKolor\debug.flag (zakladka Test, raport, log).
bool DebugEnabled();

// Zapis pliku przez plik tymczasowy + zamiane (atomowo). Zwraca false przy bledzie.
bool WriteFileAtomic(const std::wstring& path, const std::string& data);

AppSettings LoadSettings();
bool SaveSettings(const AppSettings& s);

}  // namespace zk
