cmake_minimum_required(VERSION 3.20)
project(ZetmarKolor LANGUAGES CXX RC)

# Jedno zrodlo numeru wersji: version.txt (czytane tez przez package.ps1). Trafia do pliku wersji w zasobach (.rc).
file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" ZK_VERSION LIMIT_COUNT 1)
string(REPLACE "." ";" _zk_v "${ZK_VERSION}")
list(GET _zk_v 0 ZK_VER_MAJOR)
list(GET _zk_v 1 ZK_VER_MINOR)
list(GET _zk_v 2 ZK_VER_PATCH)
configure_file(res/version.h.in "${CMAKE_BINARY_DIR}/generated/version.h" @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Statyczny CRT: wtyczka nie moze zalezec od redystrybuowalnych DLL-i obecnych/nieobecnych u uzytkownika.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Sciezka do Adobe Photoshop SDK (katalog zawierajacy photoshopapi/ i samplecode/). SDK jest poza repozytorium.
if(EXISTS "${CMAKE_SOURCE_DIR}/sdk/pluginsdk")
    set(_zk_sdk "${CMAKE_SOURCE_DIR}/sdk/pluginsdk")
else()
    set(_zk_sdk "${CMAKE_SOURCE_DIR}/../Photoshop/sdk/pluginsdk")
endif()
set(PS_SDK_DIR "${_zk_sdk}" CACHE PATH "Adobe Photoshop SDK (pluginsdk)")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
add_compile_options(/utf-8 /EHsc /W3 /fp:precise /wd4828)

# ---- Rdzen (bez zaleznosci od Photoshopa)
add_library(zkcore STATIC
    src/core/pipeline.cpp
    src/core/cube_lut.cpp
    src/core/curves.cpp
    src/core/bw_toning.cpp
    src/core/i18n.cpp
    src/core/settings.cpp
    src/core/look_preset.cpp
    src/core/pipl_binary.cpp
    src/plugin/params.cpp
)
target_include_directories(zkcore PUBLIC src)
target_link_libraries(zkcore PUBLIC shell32 ole32)

# ---- Testy rdzenia
add_executable(zk_tests tests/core_tests.cpp src/ui/lut_library.cpp)
target_link_libraries(zk_tests PRIVATE zkcore shell32)
enable_testing()
add_test(NAME core COMMAND zk_tests)

# ---- Sprawdzanie folderu LUT-ow przed pakowaniem
add_executable(zk_cubecheck tests/cube_check.cpp)
target_link_libraries(zk_cubecheck PRIVATE zkcore)

# ---- Wtyczka .8bf
option(ZK_BUILD_PLUGIN "Buduj wtyczke .8bf" ON)
if(ZK_BUILD_PLUGIN)
set(ZK_SDK_INCLUDES
    ${PS_SDK_DIR}/photoshopapi/photoshop
    ${PS_SDK_DIR}/photoshopapi/pica_sp
    ${PS_SDK_DIR}/samplecode/common/includes
)
# Klasyczny zasob PIPL (dla programow, ktore nie czytaja JSON, np. Affinity) generowany z kodu rdzenia.
add_executable(zk_makepipl tools/make_pipl.cpp)
target_link_libraries(zk_makepipl PRIVATE zkcore)
set(ZK_PIPL_BIN "${CMAKE_BINARY_DIR}/generated/zk.pipl")
add_custom_command(OUTPUT "${ZK_PIPL_BIN}"
    COMMAND zk_makepipl "${ZK_PIPL_BIN}"
    DEPENDS zk_makepipl
    COMMENT "Generowanie klasycznego PIPL")
set_source_files_properties(res/plugin.rc PROPERTIES OBJECT_DEPENDS "${ZK_PIPL_BIN}")
add_library(ZetmarKolor SHARED
    src/plugin/plugin_main.cpp
    src/plugin/host.cpp
    src/ui/main_dialog.cpp
    src/ui/curve_editor.cpp
    src/ui/lut_library.cpp
    res/plugin.rc
)
set_target_properties(ZetmarKolor PROPERTIES
    OUTPUT_NAME "ZetmarKolor_64"
    SUFFIX ".8bf"
    PREFIX ""
)
target_include_directories(ZetmarKolor PRIVATE src res "${CMAKE_BINARY_DIR}/generated" ${ZK_SDK_INCLUDES})
target_link_libraries(ZetmarKolor PRIVATE zkcore comctl32 comdlg32 shell32 ole32 uuid)

# Harness okna (bez Photoshopa): pokazuje okno na sztucznym obrazie i zapisuje zrzut.
add_executable(zk_ui_harness tests/ui_harness.cpp src/ui/main_dialog.cpp src/ui/curve_editor.cpp src/ui/lut_library.cpp)
target_include_directories(zk_ui_harness PRIVATE src ${ZK_SDK_INCLUDES})
target_link_libraries(zk_ui_harness PRIVATE zkcore comctl32 comdlg32 shell32 ole32 uuid gdiplus)
endif()
