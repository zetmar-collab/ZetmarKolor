// Zapisuje klasyczny zasob PIPL (binarny) do pliku podanego jako argument. Uruchamiane przy budowaniu wtyczki.
#include <cstdio>

#include "core/pipl_binary.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Uzycie: zk_makepipl <plik wyjsciowy>\n");
        return 2;
    }
    const auto bytes = zk::BuildClassicPipl();
    std::FILE* f = std::fopen(argv[1], "wb");
    if (!f) return 1;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return (std::fclose(f) == 0 && ok) ? 0 : 1;
}
