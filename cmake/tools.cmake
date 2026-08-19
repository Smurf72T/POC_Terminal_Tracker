# Диагностика сканера штрих-кода: показывает сырые KeyPress/KeyRelease,
# задержки между нажатиями и результат BarcodeScanner+BarcodeParser.
add_executable(scanner_probe
        tools/scanner_probe/scanner_probe.cpp
        src/utils/barcodescanner.cpp
        src/utils/barcodeparser.cpp
        src/utils/serialscanner.cpp
)
target_include_directories(scanner_probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(scanner_probe PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
